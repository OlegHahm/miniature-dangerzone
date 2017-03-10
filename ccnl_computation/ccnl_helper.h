#ifndef CCNL_HELPER_H
#define CCNL_HELPER_H

typedef enum {
    CCNL_HELPER_REMOVED_NOTHING     = (0x00),
    CCNL_HELPER_REMOVED_NEWEST_FLAG = (0x01),
    CCNL_HELPER_REMOVED_PRIO_FLAG   = (0x02)
} ccnl_helper_removed_t;

/* prototypes from CCN-lite */
void free_packet(struct ccnl_pkt_s *pkt);
struct ccnl_prefix_s *ccnl_prefix_dup(struct ccnl_prefix_s *prefix);

#endif /* CCNL_HELPER_H */
