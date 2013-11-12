#include "sixlowpan/ip.h"

/* prints current IPv6 adresses */
void ip(char *unused)
{
    (void) unused;
    ipv6_iface_print_addrs();
}
