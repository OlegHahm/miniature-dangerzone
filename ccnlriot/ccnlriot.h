#ifndef CCNLRIOT_H
#define CCNLRIOT_H

#define CCNLRIOT_NETIF      (3)

#define USE_LONG    (1)

enum {
    SITE_LILLE,
    SITE_GRENOBLE,
    SITE_PARIS
};

#define CCNLRIOT_SITE  (SITE_PARIS)

#ifdef BOARD_NATIVE
#   define CCNLRIOT_ADDRLEN     (6)
#else
#  if USE_LONG
#    define CCNLRIOT_ADDRLEN     (8)
#  else
#    define CCNLRIOT_ADDRLEN     (2)
#  endif
#endif

extern char ccnlriot_prefix1[];
extern char ccnlriot_prefix2[];

void ccnlriot_routes_setup(void);
int ccnlriot_routes_add(char *pfx, uint8_t *relay_addr, size_t addr_len);

#endif /* CCNLRIOT_H */
