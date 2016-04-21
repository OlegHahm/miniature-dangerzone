#ifndef CCNLRIOT_H
#define CCNLRIOT_H

#include "ccn-lite-riot.h"

#define CCNLRIOT_NETIF          (3)

#define CCNLRIOT_CHANNEL        (12)

#define CCNLRIOT_INT_RETRIES    (1)

#define CCNLRIOT_CSMA_RETRIES   (3)

#define CCNLRIOT_CONSUMERS      (1)

#define CCNLRIOT_LOGLEVEL  (LOG_WARNING)

#define CCNLRIOT_CHUNKNUMBERS   (10)

#define CCNLRIOT_STORE_PREFIX   "/store"
#define CCNLRIOT_SITE_PREFIX    "/siteA"
#define CCNLRIOT_TYPE_PREFIX    "/typeX"

#define CCNLRIOT_CONT_ACK       "ACK"

/* 10kB buffer for the heap should be enough for everyone - except native */
#ifndef CPU_NATIVE
#define TLSF_BUFFER     (10240 / sizeof(uint32_t))
#else
#define TLSF_BUFFER     (40960 / sizeof(uint32_t))
#endif

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

extern uint8_t ccnlriot_id[CCNLRIOT_NUMBER_OF_NODES][CCNLRIOT_ADDRLEN];

int ccnlriot_stats(int argc, char **argv);

void ccnl_helper_int(unsigned value, size_t len);
int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt);
struct ccnl_content_s *ccnl_helper_create_cont(struct ccnl_prefix_s *prefix,
                                               unsigned char *value, ssize_t
                                               len, bool cache);

#endif /* CCNLRIOT_H */
