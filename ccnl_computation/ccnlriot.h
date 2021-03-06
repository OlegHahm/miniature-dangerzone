#ifndef CCNLRIOT_H
#define CCNLRIOT_H

#include "ccn-lite-riot.h"

#define CCNLRIOT_NETIF          (3)

#define CCNLRIOT_CHANNEL        (25)

#define CCNLRIOT_INT_RETRIES    (3)
#define CCNLRIOT_INT_TIMEOUT    (100000)

#define CCNLRIOT_ALL_PREFIX     "/*"
#define CCNLRIOT_SITE_PREFIX    "/s"
#define CCNLRIOT_TYPE_PREFIX    "/t"
#define CCNLRIOT_BEACON_PREFIX  "/#"

#define CCNLRIOT_CONT_ACK       "ACK"

#ifdef CPU_NATIVE
#define TLSF_BUFFER     (409600 / sizeof(uint32_t))
#elif defined(CPU_SAMD21)
#define TLSF_BUFFER     (10240 / sizeof(uint32_t))
#else
#define TLSF_BUFFER     (45960 / sizeof(uint32_t))
#endif

#define CCNLRIOT_BUF_SIZE (128)
#define CCNLRIOT_PFX_LEN   (50)

#define USE_LONG        (1)

#ifdef CPU_SAMD21
#define CCNLRIOT_CACHE_SIZE     (5)
#else
#define CCNLRIOT_CACHE_SIZE     (80)
#endif

#define CCNLRIOT_TIMEOUT            (0)
#define CCNLRIOT_RECEIVED_CHUNK     (1)
#define CCNLRIOT_LAST_CN            (2)
#define CCNLRIOT_NO_WAIT            (3)

#ifdef BOARD_NATIVE
#   define CCNLRIOT_ADDRLEN     (6)
#else
#  if USE_LONG
#    define CCNLRIOT_ADDRLEN     (8)
#  else
#    define CCNLRIOT_ADDRLEN     (2)
#  endif
#endif

extern struct ccnl_prefix_s *ccnl_helper_all_pfx;
extern struct ccnl_prefix_s *ccnl_helper_my_pfx;
extern uint8_t ccnl_helper_flagged_cache;

void ccnl_helper_reset(void);

void ccnl_helper_init(void);

void ccnl_helper_subsribe(char c);

bool ccnl_helper_prefix_under_represented(char p);

int ccnlriot_stats(int argc, char **argv);

int ccnl_helper_int(struct ccnl_prefix_s *prefix, unsigned *chunknum, bool wait_for_int);
int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt);
int ccnlriot_consumer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt);

void ccnl_helper_publish_content(void);

int cs_oldest_representative(struct ccnl_relay_s *relay, struct ccnl_content_s *c);

struct ccnl_interest_s *ccnl_helper_create_int(struct ccnl_prefix_s *prefix);
struct ccnl_content_s *ccnl_helper_create_cont(struct ccnl_prefix_s *prefix,
                                               unsigned char *value, ssize_t
                                               len, bool cache, bool send);
void ccnl_helper_clear_pit_for_own(void);

#endif /* CCNLRIOT_H */
