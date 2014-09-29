#ifndef DEMO_H
#define DEMO_H

#include "kernel.h"
#include "ipv6.h"

#define APP_VERSION "1.1"

#define RADIO_CHANNEL   (14)

#define MONITOR_STACK_SIZE  (KERNEL_CONF_STACKSIZE_MAIN)
#define RCV_BUFFER_SIZE     (32)

#define ICMP_DATA       "RIOT"

#define ICMP_TIMEOUT    (100)

#define IF_ID   (0)

/* shell command handlers */
/**
 * @brief   Shell command to send an IPv6 datagram
 */
void sixlowapp_send_ping(int argc, char **argv);

/**
 * @brief   Shell command for netcat
 */
void sixlowapp_netcat(int argc, char **argv);

/* monitoring thread */
void *sixlowapp_monitor(void *unused);

extern ipv6_addr_t sixlowapp_ip;
extern char addr_str[IPV6_MAX_ADDR_STR_LEN];
#endif /* DEMO_H */
