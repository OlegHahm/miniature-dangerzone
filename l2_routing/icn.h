#ifndef ICN_H
#define ICN_H

#include "net/ng_ieee802154.h"

#define ICN_SEND_INTEREST   (4711)
#define ICN_SEND_BACKGROUND (4712)
#define ICN_RESEND_INTEREST (4713)

typedef enum {
    ICN_INTEREST    = 1,
    ICN_CONTENT     = 2,
    ICN_BACKGROUND  = 3
} icn_packet_type_t;

typedef struct {
    eui64_t *sender;
    eui64_t *receiver;
} icn_link_t;

typedef struct {
    eui64_t *id;
    eui64_t *dst;
    eui64_t *nextHop;
} icn_routing_entry_t;

typedef struct {
    icn_packet_type_t type;
    uint16_t seq;
    char payload[100];
} icn_pkt_t;

//=========================== prototypes ======================================

void icn_initContent(eui64_t *lastHop, uint16_t seq);
void icn_initInterest(uint16_t seq);
void icn_initBackground(void);
void icn_send(eui64_t *dst, ng_pktsnip_t *pkt);
unsigned _linkIsScheduled(eui64_t *dst);
eui64_t* _routeLookup(eui64_t *dst);

//=========================== initialization ==================================

#define TIMED_SENDING       (1)
#define INTEREST_INTERVAL   { .seconds = 0, .microseconds = 100000}
#define RETRY_INTERVAL      { .seconds = 1, .microseconds = 000000}
#define BACKGROUND_INTERVAL { .seconds = 0, .microseconds = 100000}
#define L2_RETRIES          (3)
#define FLOW_CONTROL        (1)
#define FLOW_THR            (4)
#define BACKGROUND          (1)

#define NUMBER_OF_CHUNKS    (100)
#define ADDR_LEN_64B    (8U)

#define CONTENT_STORE   (&node_ids[0])
#define HAS_CONTENT     (memcmp(&myId, CONTENT_STORE, ADDR_LEN_64B) == 0)
#define WANT_CONTENT    (memcmp(&myId, &(node_ids[7]), ADDR_LEN_64B) == 0)
#define BACKGROUND_NODE ((memcmp(&myId, &(node_ids[3]), ADDR_LEN_64B) == 0) || \
        (memcmp(&myId, &(node_ids[5]), ADDR_LEN_64B) == 0) || (memcmp(&myId, \
                &(node_ids[6]), ADDR_LEN_64B) == 0))

#define NUMBER_OF_NODES     (10)

//#define NODE_01     { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xdb, 0xa3, 0x78}} // m3-210
#define NODE_01     { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xda, 0xc0, 0x81}} // m3-210
#define NODE_02     { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xdd, 0x94, 0x81}} // m3-234
#define NODE_03     { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xdb, 0xa4, 0x78}} // m3-260
//#define NODE_04     { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x02, 0xd7, 0x38, 0x60}} // m3-222
//#define NODE_05     { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xd9, 0xb3, 0x83}} // m3-248
#define NODE_04 	{ .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xd7, 0xb1, 0x80}} // m3-224
#define NODE_05 	{ .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xde, 0xb8, 0x81}} // m3-246
//#define NODE_06 	{ .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xd6, 0xb5, 0x82}} // m3-218
#define NODE_06 	{ .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xd5, 0x95, 0x67}} // m3-214
//#define NODE_07	    { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x02, 0xdb, 0x37, 0x61}} // m3-240
#define NODE_07	    { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x02, 0xd6, 0x38, 0x61}} // m3-230
//#define NODE_08	    { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x02, 0xdc, 0x28, 0x61}} // m3-272
//#define NODE_08 	{ .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xdc, 0xa6, 0x82}} // m3-6
#define NODE_08 	{ .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xdb, 0x99, 0x83}} // m3-8
#define NODE_09	    { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xd9, 0xa9, 0x68}} // m3-278
#define NODE_10	    { .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xda, 0x98, 0x75}} // m3-284

#define NODE_11 	{ .uint8 = {0x05, 0x43, 0x32, 0xff, 0x02, 0xdc, 0x28, 0x61}} // m3-272
#define NODE_12 	{ .uint8 = {0x05, 0x43, 0x32, 0xff, 0x03, 0xda, 0x87, 0x72}} // m3-10

#define SSF_INT_SIZE    (17) // NUMBER_OF_NODES + (NUMBER_OF_LINKS * 2)

#define RRT_SIZE        (11)

extern eui64_t node_ids[NUMBER_OF_NODES];
#endif /* ICN_H */
