#include "net/af.h"
#include "net/ipv6/addr.h"
#include "net/sock/tcp.h"
#include "shell.h"

#define MAIN_QUEUE_SIZE     (8)

static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

uint8_t buf[128];
sock_tcp_t sock;

int _tcp_send(int argc, char *argv[])
{
    (void) argc;
    int res;
    sock_tcp_ep_t remote = SOCK_IPV6_EP_ANY;

    remote.port = 12345;
    ipv6_addr_from_str((ipv6_addr_t *)&remote.addr, argv[1]);
    printf("Trying to connect to [%s]:%u\n", argv[1], remote.port);
    if (sock_tcp_connect(&sock, &remote, 0, 0) < 0) {
        puts("Error connecting sock");
        return 1;
    }
    puts("Sending \"Hello!\"");
    if ((res = sock_tcp_write(&sock, "Hello!", sizeof("Hello!"))) < 0) {
        puts("Errored on write");
    }
    else {
        if ((res = sock_tcp_read(&sock, &buf, sizeof(buf),
                                 SOCK_NO_TIMEOUT)) <= 0) {
            puts("Disconnected");
        }
        printf("Read: \"");
        for (int i = 0; i < res; i++) {
            printf("%c", buf[i]);
        }
        puts("\"");
    }
    sock_tcp_disconnect(&sock);
    return res;
}

static const shell_command_t shell_commands[] = {
    { "tcp", "Send a string via TCP to a predefined port", _tcp_send },
    { NULL, NULL, NULL }
};


int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
