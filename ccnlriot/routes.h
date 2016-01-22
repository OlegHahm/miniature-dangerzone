#ifndef ROUTES_H
#define ROUTES_H

#include "ccnlriot.h"

#define CCNLRIOT_NUMBER_OF_NODES     (19)

#define NODE_SHORT_LOCAL   {0x4d, 0x0a}

#if (CCNLRIOT_SITE == LILLE)

#define NODE_SHORT_1 {0xaa, 0x02}
#define NODE_SHORT_2 {0xae, 0x02}
#define NODE_SHORT_9 {0xaa, 0x22}
#define NODE_SHORT_10 {0xa9, 0x12}
#define NODE_SHORT_12 {0xa7, 0x22}
#define NODE_SHORT_13 {0xaa, 0x06}
#define NODE_SHORT_14 {0x77, 0x06}
#define NODE_SHORT_15 {0xaf, 0x12}
#define NODE_SHORT_16 {0xaa, 0x16}
#define NODE_SHORT_17 {0xaa, 0x12}
#define NODE_SHORT_18 {0xab, 0x2a}
#define NODE_SHORT_19 {0xaa, 0x16}
#define NODE_SHORT_20 {0xac, 0x02}
#define NODE_SHORT_21 {0xad, 0x16}
#define NODE_SHORT_22 {0xa7, 0x16}
#define NODE_SHORT_23 {0xab, 0x06}
#define NODE_SHORT_24 {0xa7, 0x22}
#define NODE_SHORT_25 {0xa9, 0x26}
#define NODE_SHORT_26 {0x8f, 0x22}  

#define NODE_LONG_1 {0x36, 0x32, 0x48, 0x33, 0x46, 0xdf, 0xaa, 0x02} 
#define NODE_LONG_2 {0x36, 0x32, 0x48, 0x33, 0x46, 0xdb, 0xae, 0x02} 
#define NODE_LONG_3 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd5, 0xaf, 0x12} 
#define NODE_LONG_4 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd9, 0xa6, 0x02} 
#define NODE_LONG_5 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd9, 0xac, 0x26} 
#define NODE_LONG_6 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd7, 0xaa, 0x22} 
#define NODE_LONG_7 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd7, 0x6f, 0x16} 
#define NODE_LONG_9 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd8, 0xaa, 0x22} 
#define NODE_LONG_10 {0x36, 0x32, 0x48, 0x33, 0x46, 0xdb, 0xa9, 0x12} 
#define NODE_LONG_11 {0x36, 0x32, 0x48, 0x33, 0x46, 0xda, 0xa8, 0x26} 
#define NODE_LONG_12 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd4, 0xa7, 0x22} 
#define NODE_LONG_13 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd9, 0xaa, 0x06} 
#define NODE_LONG_14 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd4, 0x77, 0x06} 
#define NODE_LONG_15 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd8, 0xaf, 0x12} 
#define NODE_LONG_16 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd5, 0xaa, 0x16} 
#define NODE_LONG_17 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd4, 0xaa, 0x12} 
#define NODE_LONG_18 {0x36, 0x32, 0x48, 0x33, 0x46, 0xdb, 0xab, 0x2a} 
#define NODE_LONG_19 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd8, 0xaa, 0x16} 
#define NODE_LONG_20 {0x36, 0x32, 0x48, 0x33, 0x46, 0xde, 0xac, 0x02} 
#define NODE_LONG_21 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd4, 0xad, 0x16} 
#define NODE_LONG_22 {0x36, 0x32, 0x48, 0x33, 0x46, 0xdb, 0xa7, 0x16} 
#define NODE_LONG_23 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd5, 0xab, 0x06} 
#define NODE_LONG_24 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd8, 0xa7, 0x22} 
#define NODE_LONG_25 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd6, 0xa9, 0x26} 
#define NODE_LONG_26 {0x36, 0x32, 0x48, 0x33, 0x46, 0xd9, 0x8f, 0x22} 

#elif (CCNLRIOT_SITE == PARIS)


#endif

#if USE_LONG
uint8_t ccnlriot_id[CCNLRIOT_NUMBER_OF_NODES][CCNLRIOT_ADDRLEN] = {
    NODE_LONG_1,
    NODE_LONG_2,
//    NODE_LOCAL,
    NODE_LONG_9,
    NODE_LONG_10,
    NODE_LONG_12,
    NODE_LONG_13,
    NODE_LONG_14,
    NODE_LONG_15,
    NODE_LONG_16,
    NODE_LONG_17,
    NODE_LONG_18,
    NODE_LONG_19,
    NODE_LONG_20,
    NODE_LONG_21,
    NODE_LONG_22,
    NODE_LONG_23,
    NODE_LONG_24,
    NODE_LONG_25,
    NODE_LONG_26
};

#else

uint8_t ccnlriot_id[CCNLRIOT_NUMBER_OF_NODES][CCNLRIOT_ADDRLEN] = {
    NODE_SHORT_1,
    NODE_SHORT_2,
//    NODE_SHORT_LOCAL,
    NODE_SHORT_9,
    NODE_SHORT_10,
    NODE_SHORT_12,
    NODE_SHORT_13,
    NODE_SHORT_14,
    NODE_SHORT_15,
    NODE_SHORT_16,
    NODE_SHORT_17,
    NODE_SHORT_18,
    NODE_SHORT_19,
    NODE_SHORT_20,
    NODE_SHORT_21,
    NODE_SHORT_22,
    NODE_SHORT_23,
    NODE_SHORT_24,
    NODE_SHORT_25,
    NODE_SHORT_26
};

#endif

#endif /* ROUTES_H */