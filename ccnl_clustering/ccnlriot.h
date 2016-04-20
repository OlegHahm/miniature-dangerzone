#ifndef CCNLRIOT_H
#define CCNLRIOT_H

#define CCNLRIOT_NETIF          (3)

#define CCNLRIOT_CHANNEL        (12)

#define CCNLRIOT_INT_RETRIES    (10)

#define CCNLRIOT_CSMA_RETRIES   (3)

#define CCNLRIOT_CONSUMERS      (1)

#define CCNLRIOT_LOGLEVEL  (LOG_DEBUG)

#define CCNLRIOT_CHUNKNUMBERS   (10)

#define CCNLRIOT_PREFIX1    "/riot/peter/schmerzl"
#define CCNLRIOT_PREFIX2    "/start/the/riot"

#define CCNLRIOT_CONT       "#########-----------**********--------############"
//#define CCNLRIOT_CONT       "#####"

#define CCNLRIOT_BUF_SIZE (128)

#define USE_LONG        (1)
#define USE_BROADCAST   (0)
#define USE_AUTOSTART   (1)

#define CCNLRIOT_NUMBER_OF_NODES     (20)

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
extern uint8_t ccnlriot_id[CCNLRIOT_NUMBER_OF_NODES][CCNLRIOT_ADDRLEN];

void ccnlriot_routes_setup(void);
int ccnlriot_routes_add(char *pfx, uint8_t *relay_addr, size_t addr_len);
int ccnlriot_get_mypos(void);

int ccnlriot_stats(int argc, char **argv);

#endif /* CCNLRIOT_H */
