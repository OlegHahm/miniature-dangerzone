#ifndef CCNLRIOT_H
#define CCNLRIOT_H

#define CCNLRIOT_NETIF      (3)

#ifdef BOARD_NATIVE
#   define CCNLRIOT_ADDRLEN     (6)
#else
#   define CCNLRIOT_ADDRLEN     (2)
#endif

extern char ccnlriot_prefix[];

void ccnlriot_routes_setup(void);
int ccnlriot_routes_add(char *pfx, uint8_t *relay_addr, size_t addr_len);

#endif /* CCNLRIOT_H */
