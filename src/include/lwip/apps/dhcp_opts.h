/**
  ******************************************************************************
  * @file    dhcp_opts.h
  * @author  Benedek Kupper
  * @brief   Simple DHCP server options
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
#ifndef LWIP_HDR_DHCP_OPTS_H
#define LWIP_HDR_DHCP_OPTS_H

#include "lwip/opt.h"

/**
 * @defgroup dhcp_server_opts Options
 * @ingroup dhcp_server
 * @{
 */

/** Set this to 1 to support DHCP server */
#if !LWIP_IPV4 || !defined LWIP_DHCP_SERVER || defined __DOXYGEN__
#define LWIP_DHCP_SERVER                    0
#endif

/** The maximum number of DHCP clients the server can handle */
#if !defined DHCP_SERVER_MAX_CLIENTS || defined __DOXYGEN__
#define DHCP_SERVER_MAX_CLIENTS             5
#endif

/** The lease time [seconds] of each DHCP-assigned address */
#if !defined DHCP_SERVER_LEASE_TIME || defined __DOXYGEN__
#define DHCP_SERVER_LEASE_TIME              (60 * 60)
#endif

/** @} */

#endif /* LWIP_HDR_DHCP_OPTS_H */
