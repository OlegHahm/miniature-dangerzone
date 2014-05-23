/*
 * Copyright (C) 2014 Oliver Hahm <oliver.hahm@inria.fr>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

#include <stdio.h>
#include "posix_io.h"
#include "board_uart0.h"
#include "shell.h"
#include "shell_commands.h"

int main(void)
{
    puts("Link Layer Ping test application starting.");

    shell_t shell;
    (void) posix_open(uart0_handler_pid, 0);

    shell_init(&shell, NULL, UART0_BUFSIZE, uart0_readc, uart0_putc);

    shell_run(&shell);
    return 0;
}
