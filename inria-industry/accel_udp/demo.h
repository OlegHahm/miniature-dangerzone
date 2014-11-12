#ifndef DEMO_H
#define DEMO_H

#include "kernel.h"
#include "radio/types.h"
#include "socket_base/socket.h"

#define APP_VERSION "0.5"

/**
 * Basic network configuration (addresses, channel, etc)
 */
#define NODE_ADDRESS            3 // 0x29
#define LEVEL3
#define NODE_MODE               "n"     // normal node
#define RADIO_CHANNEL           (11)
#define GW_ADDRESS              "23"
#define APPLICATION_PORT        (511)

/**
 * Set this value to 1 when you want to send the RIOT-TV information
 * via UDP instead of printing it to the std out.
 */
#define VIZ_REMOTE              1

/**
 * List of addresses (as strings) of nodes interested in sensor events
 */
#define OBSERVER_NUMOF          (1U)
#define OBSERVER_NODES          {1} // and the watch

/**
 * List a number of nodes that are ignored from this node
 */
#define IGNORE_NUMOF            0
#define IGNORE_NODES            {}

/**
 * Define the number of times each UDP packet is transmitted
 */
#define RETRANSMISSIONS         1

/**
 * General UDP socket configuration
 */
#define MONITOR_STACK_SIZE      (KERNEL_CONF_STACKSIZE_MAIN)
#define RCV_BUFFER_SIZE         (32)

/**
 * @brief   Which interface should be used for 6LoWPAN
 */
#define IF_ID   (0)

/* RPL shell command handlers */
/**
 * @brief   Shell command to initializes RPL and Destiny
 */
void rpl_udp_init(int argc, char **argv);

/**
 * @brief   Shell command to set node's ID
 */
void rpl_udp_set_id(int argc, char **argv);

/**
 * @brief   Loops through the routing table
 */
void rpl_udp_loop(int argc, char **argv);

/**
 * @brief   Shows the routing table
 */
void rpl_udp_table(int argc, char **argv);

/**
 * @brief   Shows the dodag
 */
void rpl_udp_dodag(int argc, char **argv);

/* helper command handlers */
void rpl_udp_ip(int argc, char **argv);

void rpl_udp_ignore(uint16_t addr);
void rpl_udp_ignore_cmd(int argc, char **argv);

/* monitoring thread */
void *rpl_udp_monitor(void *unused);

extern radio_address_t id;
extern ipv6_addr_t std_addr;
extern char addr_str[IPV6_MAX_ADDR_STR_LEN];
#endif /* DEMO_H */
