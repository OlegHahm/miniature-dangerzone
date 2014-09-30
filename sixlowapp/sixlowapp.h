#ifndef DEMO_H
#define DEMO_H

#include "kernel.h"
#include "ipv6.h"

#define APP_VERSION "1.1"

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

void sixlowapp_udp_send(ipv6_addr_t *dest, uint16_t port, char *payload, size_t len);

/* monitoring thread */
void *sixlowapp_monitor(void *unused);

void *_udp_server_loop(void *arg);

extern kernel_pid_t sixlowapp_udp_server_pid;
extern uint16_t sixlowapp_netcat_listen_port;
extern uint16_t sixlowapp_netcat_listen_port;

extern ipv6_addr_t sixlowapp_ip;
extern char addr_str[IPV6_MAX_ADDR_STR_LEN];
#endif /* DEMO_H */
