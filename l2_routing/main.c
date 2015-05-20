#include <stdio.h>
#include "kernel_types.h"
#include "shell.h"
#include "shell_commands.h"
#include "posix_io.h"
#include "board_uart0.h"
#include "net/ng_pktdump.h"
#include "net/ng_netbase.h"
#include "net/ng_nomac.h"
#include "l2.h"
#include "icn.h"

static char _stack[NG_PKTDUMP_STACKSIZE];
kernel_pid_t *if_id;
timex_t interval = INTEREST_INTERVAL;
vtimer_t periodic_vt;

int main(void)
{
    shell_t shell;
    ng_netreg_entry_t dump;
    size_t ifnum;
    unsigned res = 8;
    uint16_t seq_nr = 0;
    bool rx_irq = true;
    uint8_t retries = 7;

    puts("L2 routing app");
    if_id = ng_netif_get(&ifnum);
    ng_netapi_set(*if_id, NETCONF_OPT_SRC_LEN, 0, &res, sizeof(uint16_t));

    dump.demux_ctx = NG_NETREG_DEMUX_CTX_ALL;

    dump.pid = thread_create(_stack, sizeof(_stack), NG_PKTDUMP_PRIO,
                             CREATE_STACKTEST, _eventloop, NULL, "pktdump");
    ng_netreg_register(NG_NETTYPE_UNDEF, &dump);

    ng_netapi_set(*if_id, NETCONF_OPT_RX_START_IRQ, 0, &rx_irq, sizeof(bool));
    ng_netapi_set(*if_id, NETCONF_OPT_RETRANS, 0, &retries, sizeof(uint8_t));
    res = ng_netapi_get(*if_id, NETCONF_OPT_ADDRESS_LONG, 0, myId.uint8, ADDR_LEN_64B);

    printf("My ID is %s\n",
            ng_netif_addr_to_str(l2addr_str, sizeof(l2addr_str),
                myId.uint8, res));

    if (WANT_CONTENT) {
        vtimer_set_msg(&periodic_vt, interval, dump.pid, ICN_SEND_INTEREST, &seq_nr);
    }
    /* start the shell */
    (void) posix_open(uart0_handler_pid, 0);
    shell_init(&shell, NULL, UART0_BUFSIZE, uart0_readc, uart0_putc);
    shell_run(&shell);

    return 0;
}
