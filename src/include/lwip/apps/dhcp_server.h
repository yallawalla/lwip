/**
  ******************************************************************************
  * @file    dhcp_server.h
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
#ifndef LWIP_HDR_DHCP_SERVER_H
#define LWIP_HDR_DHCP_SERVER_H

#include "dhcp_opts.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#if LWIP_DHCP_SERVER /* don't build if not configured for use in lwipopts.h */

err_t dhcp_server_init(struct netif *netif, const ip4_addr_t *addr_start, u8_t addr_range);

#endif

#ifdef __cplusplus
}
#endif

#endif /* LWIP_HDR_DHCP_SERVER_H */
