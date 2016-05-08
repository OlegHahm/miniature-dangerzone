#ifndef CCNLRIOT_H
#define CCNLRIOT_H

//#define LOG_LEVEL           (LOG_DEBUG)
#include "ccn-lite-riot.h"

#define CCNLRIOT_NETIF          (3)

#define CCNLRIOT_CHANNEL        (12)

#define CCNLRIOT_INT_RETRIES    (3)

#define CCNLRIOT_CSMA_RETRIES   (3)

#define CCNLRIOT_CONSUMERS      (1)

#define CCNLRIOT_LOGLEVEL  (LOG_DEBUG)

#define CCNLRIOT_CHUNKNUMBERS   (10)

#define CCNLRIOT_ALL_PREFIX     "/*"
#define CCNLRIOT_STORE_PREFIX   "/store"
#define CCNLRIOT_SITE_PREFIX    "/siteA"
#define CCNLRIOT_TYPE_PREFIX    "/typeX"
#define CCNLRIOT_DONE_PREFIX    "/done"

#define CCNLRIOT_CONT_ACK       "ACK"

/* 10kB buffer for the heap should be enough for everyone - except native */
#ifdef CPU_NATIVE
#define TLSF_BUFFER     (40960 / sizeof(uint32_t))
#elif defined(CPU_SAMD21)
#define TLSF_BUFFER     (10240 / sizeof(uint32_t))
#else
#define TLSF_BUFFER     (45960 / sizeof(uint32_t))
#endif

#define CCNLRIOT_BUF_SIZE (128)
#define CCNLRIOT_PFX_LEN   (50)

#define USE_LONG        (1)
#define USE_BROADCAST   (0)
#define USE_AUTOSTART   (1)

#define CCNLRIOT_NUMBER_OF_NODES     (20)

#ifdef CPU_SAMD21
#define CCNLRIOT_CACHE_SIZE     (5)
#else
#define CCNLRIOT_CACHE_SIZE     (80)
#endif

#define CCNLRIOT_TIMEOUT            (0)
#define CCNLRIOT_RECEIVED_CHUNK     (1)
#define CCNLRIOT_LAST_CN            (2)
#define CCNLRIOT_NO_WAIT            (3)

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

int ccnl_helper_int(unsigned char *prefix, unsigned *chunknum, bool no_wait);
int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt);
int ccnlriot_consumer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt);
struct ccnl_content_s *ccnl_helper_create_cont(struct ccnl_prefix_s *prefix,
                                               unsigned char *value, ssize_t
                                               len, bool cache);
void ccnl_helper_send_all_data(void);
void ccnl_helper_clear_pit_for_own(void);

#endif /* CCNLRIOT_H */
