#include <stdio.h>
#include <string.h>
#include "vtimer.h"
#include "thread.h"

#include "sixlowpan.h"
#include "rpl.h"
#include "rpl_dodag.h"

#define TR_WD_STACKSIZE     (256)

#ifdef MODULE_NATIVENET
#define TRANSCEIVER TRANSCEIVER_NATIVE
#else
#define TRANSCEIVER TRANSCEIVER_CC1100
#endif

char tr_wd_stack[TR_WD_STACKSIZE];

void wakeup_thread(void)
{
    while (1) {
        if (thread_getstatus(transceiver_pid) == STATUS_SLEEPING) {
            vtimer_usleep(500 * 1000);

            if (thread_getstatus(transceiver_pid) == STATUS_SLEEPING) {
                thread_wakeup(transceiver_pid);
            }
        }

        vtimer_usleep(1000 * 1000);
    }
}

void init(char *str)
{
    transceiver_command_t tcmd;
    msg_t m;
    uint8_t chan = 10;

    char command;
    uint16_t r_addr;

    int res = sscanf(str, "init %c %hu", &command, &r_addr);

    if (res < 1) {
        printf("Usage: init address\n");
        printf("\tr\tinitialize as root\n");
        printf("\tn\tinitialize as node router\n");
        printf("\taddress must be an 8 bit integer\n");
    }

    uint8_t state;

    if ((command == 'n') || (command == 'r')) {
        printf("INFO: Initialize as %s on address %d\n", ((command == 'n') ? "node" : "root"), r_addr);
        if (r_addr > 255) {
            printf("ERROR: address not an 8 bit integer\n");
            return;
        }

        state = rpl_init(TRANSCEIVER, r_addr);

        if (state != SIXLOWERROR_SUCCESS) {
            printf("Error initializing RPL\n");
        }

        if (command == 'r') {
            rpl_init_root();
        }
    }
    else {
        printf("ERROR: Unknown command '%c'\n", command);
        return;
    }

    /* TODO: check if this works as intended */
    ipv6_addr_t std_addr, prefix, tmp;
    ipv6_addr_init(&std_addr, 0xABCD, 0xEF12, 0, 0, 0x1034, 0x00FF, 0xFE00, r_addr);
    ipv6_addr_init_prefix(&prefix, &std_addr, 64);
    plist_add(&prefix, 64, NDP_OPT_PI_VLIFETIME_INFINITE, 0, 1, ICMPV6_NDP_OPT_PI_FLAG_AUTONOM);
    ipv6_init_iface_as_router();
    /* add global address */
    ipv6_addr_set_by_eui64(&tmp, &std_addr);
    ipv6_iface_add_addr(&tmp, IPV6_ADDR_TYPE_GLOBAL, NDP_ADDR_STATE_PREFERRED, 0, 0);
            
    /* set channel to 10 */
    tcmd.transceivers = TRANSCEIVER;
    tcmd.data = &chan;
    m.type = SET_CHANNEL;
    m.content.ptr = (void *) &tcmd;

    msg_send_receive(&m, &m, transceiver_pid);

    destiny_init_transport_layer();
    puts("Destiny initialized");
    /* start transceiver watchdog */
    thread_create(tr_wd_stack, TR_WD_STACKSIZE, PRIORITY_MAIN - 3, CREATE_STACKTEST, wakeup_thread, "TX/RX WD");
}

void loop(char *unused)
{
    (void) unused;

    rpl_routing_entry_t *rtable;
    char addr_str[IPV6_MAX_ADDR_STR_LEN];

    while (1) {
        rtable = rpl_get_routing_table();
        rpl_dodag_t *mydodag = rpl_get_my_dodag();

        if (mydodag == NULL) {
            vtimer_usleep(20 * 1000 * 1000);
            continue;
        }

        printf("---------------------------\n");
        printf("OUTPUT\n");
        printf("my rank: %d\n", mydodag->my_rank);
        printf("my preferred parent:\n");
        printf("%s\n", ipv6_addr_to_str(addr_str, (&mydodag->my_preferred_parent->addr)));
        printf("parent lifetime: %d\n", mydodag->my_preferred_parent->lifetime);
        printf("---------------------------$\n");

        for (int i = 0; i < RPL_MAX_ROUTING_ENTRIES; i++) {
            if (rtable[i].used) {
                printf("%s\n", ipv6_addr_to_str(addr_str, (&rtable[i].address)));
                puts("next hop");
                printf("%s\n", ipv6_addr_to_str(addr_str, (&rtable[i].next_hop)));
                printf("entry %d lifetime %d\n", i, rtable[i].lifetime);

                if (!rpl_equal_id(&rtable[i].address, &rtable[i].next_hop)) {
                    puts("multi-hop");
                }

                printf("---------------------------$\n");
            }
        }

        printf("########################\n");
        vtimer_usleep(20 * 1000 * 1000);
    }


}

void table(char *unused)
{
    (void) unused;

    rpl_routing_entry_t *rtable;
    char addr_str[IPV6_MAX_ADDR_STR_LEN];
    rtable = rpl_get_routing_table();
    printf("---------------------------\n");
    printf("OUTPUT\n");
    printf("---------------------------\n");

    for (int i = 0; i < RPL_MAX_ROUTING_ENTRIES; i++) {
        if (rtable[i].used) {
            printf("%s\n", ipv6_addr_to_str(addr_str, (&rtable[i].address)));
            printf("entry %d lifetime %d\n", i, rtable[i].lifetime);

            if (!rpl_equal_id(&rtable[i].address, &rtable[i].next_hop)) {
                puts("multi-hop");
            }

            printf("--------------\n");
        }
    }

    printf("$\n");
}

void dodag(char *unused)
{
    (void) unused;

    char addr_str[IPV6_MAX_ADDR_STR_LEN];
    printf("---------------------------\n");
    rpl_dodag_t *mydodag = rpl_get_my_dodag();

    if (mydodag == NULL) {
        printf("Not part of a dodag\n");
        printf("---------------------------$\n");
        return;
    }

    printf("Part of Dodag:\n");
    printf("%s\n", ipv6_addr_to_str(addr_str, (&mydodag->dodag_id)));
    printf("my rank: %d\n", mydodag->my_rank);
    printf("my preferred parent:\n");
    printf("%s\n", ipv6_addr_to_str(addr_str, (&mydodag->my_preferred_parent->addr)));
    printf("---------------------------$\n");
}
