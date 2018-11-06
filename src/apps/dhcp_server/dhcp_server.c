/**
 ******************************************************************************
 * @file    dhcp_server.c
 * @author  Benedek Kupper
 * @brief   Simple DHCP server
 *
 * Copyright (c) 2018 Benedek Kupper
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "lwip/apps/dhcp_server.h"

#include "lwip/ip_addr.h"
#include "lwip/def.h"
#include "lwip/dns.h"
#include "lwip/udp.h"
#include "lwip/timeouts.h"
#include "lwip/debug.h"
#include "lwip/ip_addr.h"
#include "lwip/prot/dhcp.h"
#include "lwip/prot/dns.h"
#include "lwip/prot/iana.h"

#include <string.h>

#if LWIP_DHCP_SERVER /* don't build if not configured for use in lwipopts.h */

/**
 * @defgroup dhcp_server DHCP server
 * @ingroup apps
 *
 * This is simple DHCP server for the lwIP raw API.
 */

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct dhcp_ip4_option
{
  PACK_STRUCT_FLD_8(u8_t type);
  PACK_STRUCT_FLD_8(u8_t len);
  PACK_STRUCT_FLD_S(ip4_addr_p_t addr);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct dhcp_u32_option
{
  PACK_STRUCT_FLD_8(u8_t type);
  PACK_STRUCT_FLD_8(u8_t len);
  PACK_STRUCT_FLD_S(u32_t dw);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

struct dhcp_srv_entry
{
  ip4_addr_t ip4addr;
  u32_t lease;
  u8_t hwaddr[NETIF_MAX_HWADDR_LEN];
};

struct dhcp_srv_state
{
  struct udp_pcb *upcb;
  struct netif *netif;
  int entries;
  struct dhcp_srv_entry entry[DHCP_SERVER_MAX_CLIENTS];
};

static const u8_t hwaddr_free_val = 0xFF;
static struct dhcp_srv_state dss;

static struct dhcp_srv_entry*
dhcp_srv_get_entry(struct dhcp_msg *msg)
{
  int i;
  for (i = 0; i < dss.entries; i++) {
    if (memcmp(dss.entry[i].hwaddr, msg->chaddr, sizeof(dss.entry[0].hwaddr)) == 0) {
      return &dss.entry[i];
    }
  }
  return NULL;
}

static int
dhcp_srv_is_entry_free(struct dhcp_srv_entry *entry)
{
  u8_t i;
  for (i = 0; i < sizeof(dss.entry[0].hwaddr); i++) {
    if (entry->hwaddr[i] != hwaddr_free_val) {
      return 0;
    }
  }
  return 1;
}

static struct dhcp_srv_entry*
dhcp_srv_put_new_entry(struct dhcp_msg *msg)
{
  int i;
  for (i = 0; i < dss.entries; i++) {
    if (dhcp_srv_is_entry_free(&dss.entry[i])) {
      /* allocate free entry to this requester by saving its HW address */
      MEMCPY(dss.entry[i].hwaddr, msg->chaddr, sizeof(dss.entry[0].hwaddr));
      return &dss.entry[i];
    }
  }
  return NULL;
}

static void
dhcp_srv_free_entry(struct dhcp_srv_entry *entry)
{
  memset(entry->hwaddr, hwaddr_free_val, sizeof(dss.entry[0].hwaddr));
}

static void*
dhcp_srv_get_option(u8_t *options, u8_t option_type, u8_t option_size)
{
  u16_t head = 0, next = 2 + options[1];

  do {
    /* find the option by type */
    if (options[head] == option_type) {
      /* check if the size is correct */
      if (options[head + 1] == option_size) {
        /* return the pointer to the option data */
        return &options[head + 2];
      }
      break;
    }
    head = next;
    next += 2 + options[head + 1];
  }
  while (next <= DHCP_OPTIONS_LEN);

  return NULL;
}

static void
dhcp_srv_set_reply(struct dhcp_msg *msg, u8_t msg_type, struct dhcp_srv_entry *entry)
{
  struct dhcp_ip4_option* ip4_opt;
  struct dhcp_u32_option* u32_opt;
  int opt_len = 0;

  /* update message to reply */
  msg->op = DHCP_BOOTREPLY;
  msg->secs = 0;
  msg->flags = 0;
  ip4_addr_copy(msg->yiaddr, entry->ip4addr);
  msg->cookie = PP_HTONL(DHCP_MAGIC_COOKIE);

  /* options are rewritten */
  memset(&msg->options, 0, DHCP_OPTIONS_LEN);

  /* message type */
  msg->options[opt_len++] = DHCP_OPTION_MESSAGE_TYPE;
  msg->options[opt_len++] = 1;
  msg->options[opt_len++] = msg_type;

  /* subnet mask */
  ip4_opt = (void*)&msg->options[opt_len];
  ip4_opt->type = DHCP_OPTION_SUBNET_MASK;
  ip4_opt->len = sizeof(ip4_opt->addr);
  ip4_addr_copy(ip4_opt->addr, ip_2_ip4(dss.netif->netmask));
  opt_len += sizeof(struct dhcp_ip4_option);

  /* router */
  ip4_opt = (void*)&msg->options[opt_len];
  ip4_opt->type = DHCP_OPTION_ROUTER;
  ip4_opt->len = sizeof(ip4_opt->addr);
  ip4_addr_copy(ip4_opt->addr, ip_2_ip4(dss.netif->gw));
  opt_len += sizeof(struct dhcp_ip4_option);

  /* server ID */
  ip4_opt = (void*)&msg->options[opt_len];
  ip4_opt->type = DHCP_OPTION_SERVER_ID;
  ip4_opt->len = sizeof(ip4_opt->addr);
  ip4_addr_copy(ip4_opt->addr, ip_2_ip4(dss.netif->ip_addr));
  opt_len += sizeof(struct dhcp_ip4_option);

  /* lease time */
  u32_opt = (void*)&msg->options[opt_len];
  u32_opt->type = DHCP_OPTION_LEASE_TIME;
  u32_opt->len = sizeof(u32_opt->dw);
  u32_opt->dw = lwip_htonl(entry->lease);
  opt_len += sizeof(struct dhcp_u32_option);

#if DNS_LOCAL_HOSTLIST
  /* dns servers data */
  ip4_opt = (void*)&msg->options[opt_len];
  ip4_opt->type = DHCP_OPTION_DNS_SERVER;
  ip4_opt->len = sizeof(ip4_opt->addr);
  ip4_addr_copy(ip4_opt->addr, ip_2_ip4(dss.netif->ip_addr));
  opt_len += sizeof(struct dhcp_ip4_option);
#endif /* DNS_LOCAL_HOSTLIST */

  /* end */
  msg->options[opt_len++] = DHCP_OPTION_END;
}

static void
dhcp_srv_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
  struct pbuf *q;

  LWIP_UNUSED_ARG(arg);
  LWIP_UNUSED_ARG(addr);

  q = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct dhcp_msg), PBUF_RAM);

  if (q != NULL) {
    struct dhcp_msg *msg = q->payload;

    /* Copy request contents
     * TODO add support for copying from segmented pbuf */
    pbuf_take(q, p->payload, LWIP_MIN(p->len, sizeof(struct dhcp_msg)));

    if (msg->op == DHCP_BOOTREQUEST) {
      struct dhcp_srv_entry *entry = dhcp_srv_get_entry(msg);
      ip4_addr_p_t *reqaddr;

      switch (msg->options[2]) {

        case DHCP_DISCOVER:
          /* check if client is assigned already */
          if (entry == NULL) {
            /* otherwise try to get an unused address */
            entry = dhcp_srv_put_new_entry(msg);
          }
          if (entry == NULL) {
            break;
          }

          dhcp_srv_set_reply(msg, DHCP_OFFER, entry);

          udp_sendto(upcb, q, IP_ADDR_BROADCAST, port);
          break;

        case DHCP_REQUEST:
          /* if no offered entry, return */
          if (entry == NULL) {
            break;
          }

          /* get the requested address */
          reqaddr = dhcp_srv_get_option(msg->options,
                                        DHCP_OPTION_REQUESTED_IP,
                                        sizeof(ip4_addr_p_t));
          if (reqaddr == NULL) {
            break;
          }

          /* if they differ, return with error */
          if (!ip4_addr_cmp(&entry->ip4addr, reqaddr)) {
            dhcp_srv_free_entry(entry);
            break;
          }

          dhcp_srv_set_reply(msg, DHCP_ACK, entry);

          udp_sendto(upcb, q, IP_ADDR_BROADCAST, port);
          break;

        case DHCP_RELEASE:
          if (entry != NULL) {
            /* free the used entry */
            dhcp_srv_free_entry(entry);
          }
          break;

        default:
          break;
      }
    }
    pbuf_free(q);
  }
  pbuf_free(p);
}

/** @ingroup dhcp_server
 * Initialize DHCP server.
 * @param netif the interface acting as the DHCP server
 * @param addr_start first address of the DHCP-offered IP address range
 * @param addr_range amount of consecutive addresses to offer
 * @return OK if the UDP port setup succeeded
 */
err_t
dhcp_server_init(struct netif *netif, const ip4_addr_t *addr_start, u8_t addr_range)
{
  err_t ret;
  struct udp_pcb *pcb;

  LWIP_ASSERT("addr_range <= DHCP_SERVER_MAX_CLIENTS",
              addr_range <= LWIP_ARRAYSIZE(dss.entry));
  LWIP_ASSERT("netif->ip_addr != addr_start",
              !ip4_addr_cmp(&ip_2_ip4(netif->ip_addr), addr_start));

  pcb = udp_new_ip_type(IPADDR_TYPE_V4);
  if (pcb == NULL) {
    return ERR_MEM;
  }

  ret = udp_bind(pcb, IP_ANY_TYPE, LWIP_IANA_PORT_DHCP_SERVER);
  if (ret != ERR_OK) {
    udp_remove(pcb);
  }
  else {
    u8_t i;
    dss.netif = netif;
    dss.entries = addr_range;

    /* initialize entry IPs sequentially, HW addresses to free */
    for (i = 0; i < dss.entries; i++) {
      ip4_addr_set_u32(&dss.entry[i].ip4addr, ip4_addr_get_u32(addr_start) + lwip_htonl(i));
      dss.entry[i].lease = DHCP_SERVER_LEASE_TIME;
      memset(dss.entry[i].hwaddr, hwaddr_free_val, sizeof(dss.entry[0].hwaddr));
    }

    udp_recv(pcb, dhcp_srv_recv, NULL);
  }

  return ret;
}

#endif /* LWIP_DHCP_SERVER */
