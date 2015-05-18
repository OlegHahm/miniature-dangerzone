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
timex_t interval;
vtimer_t vt;

int main(void)
{
    shell_t shell;
    ng_netreg_entry_t dump;
    size_t ifnum;
    unsigned res = 8;

    interval = timex_set(5, 0);

    puts("L2 routing app");
    if_id = ng_netif_get(&ifnum);
    ng_netapi_set(*if_id, NETCONF_OPT_SRC_LEN, 0, &res, sizeof(uint16_t));

    dump.demux_ctx = NG_NETREG_DEMUX_CTX_ALL;

    dump.pid = thread_create(_stack, sizeof(_stack), NG_PKTDUMP_PRIO,
                             CREATE_STACKTEST, _eventloop, NULL, "pktdump");
    ng_netreg_register(NG_NETTYPE_UNDEF, &dump);

    res = ng_netapi_get(*if_id, NETCONF_OPT_ADDRESS_LONG, 0, myId.uint8, ADDR_LEN_64B);
    printf("My ID is %s\n",
            ng_netif_addr_to_str(l2addr_str, sizeof(l2addr_str),
                myId.uint8, res));

    vtimer_set_msg(&vt, interval, dump.pid, ICN_SEND_INTEREST, NULL);
    /* start the shell */
    (void) posix_open(uart0_handler_pid, 0);
    shell_init(&shell, NULL, UART0_BUFSIZE, uart0_readc, uart0_putc);
    shell_run(&shell);

    return 0;
}
