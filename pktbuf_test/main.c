#include <stdio.h>
#include <stdint.h>

#include "shell.h"
#include "net/gnrc/pktbuf.h"

int _pktbuf_stats(int argc, char **argv);
int _pktbuf_add(int argc, char **argv);
int _pktbuf_release(int argc, char **argv);

static uint8_t my_data[8192];

static shell_command_t shell_commands[] = {
    { "stats", "Show the current packet buffer stats", _pktbuf_stats},
    { "add", "Add data to the packet buffer", _pktbuf_add},
    { "release", "Releases data from the packet buffer", _pktbuf_release},
    { NULL, NULL, NULL }
};

int _pktbuf_stats(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    puts("printing packet buffer stats");
    gnrc_pktbuf_stats();
    return 0;
}

int _pktbuf_add(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <size> [next]\n", argv[0]);
        return 1;
    }
    int size = atoi(argv[1]);

    gnrc_pktsnip_t *snip = NULL;
    if (argc > 2) {
    }

    printf("Adding %u bytes to packet buffer\n", size);
    gnrc_pktsnip_t *new = gnrc_pktbuf_add(snip, my_data, size, 0);
    printf("New snip has address: %p\n", (void*)new);
    return 0;
}

int _pktbuf_release(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <snip>\n", argv[0]);
        return 1;
    }
    int address = strtol(argv[1], NULL, 16);
    printf("Releasing snip at address %x\n", address);
    gnrc_pktbuf_release((gnrc_pktsnip_t*) address);

    return 0;
}

int main(void)
{
    puts("This is a pktbuf test application");

    for (int i = 0; i < 8192; i++) {
        my_data[i] = i;
    }
    
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
