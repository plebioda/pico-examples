#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H
#include <stdint.h>

void sntp_set_system_time(uint32_t sec);

#define SNTP_SET_SYSTEM_TIME(s) sntp_set_system_time(s)

#define SNTP_SUPPORT 1
#define SNTP_SERVER_DNS 1
#define SNTP_GET_SERVERS_FROM_DHCP 1
#define SNTP_UPDATE_DELAY 15000
#define SNTP_DEBUG (LWIP_DBG_ON | LWIP_DBG_TYPES_ON | LWIP_DBG_TRACE)

// Generally you would define your own explicit list of lwIP options
// (see https://www.nongnu.org/lwip/2_1_x/group__lwip__opts.html)
//
// This example uses a common include to avoid repetition
#include "lwipopts_examples_common.h"

#if !NO_SYS
#define TCPIP_THREAD_STACKSIZE 1024
#define DEFAULT_THREAD_STACKSIZE 1024
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define TCPIP_MBOX_SIZE 8
#define LWIP_TIMEVAL_PRIVATE 0

// not necessary, can be done either way
#define LWIP_TCPIP_CORE_LOCKING_INPUT 1
#endif

#define PPP_SUPPORT 1
#define LWIP_INCLUDED_POLARSSL_SHA1 1

#endif
