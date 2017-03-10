#include "log.h"
#include "msg.h"
#include "xtimer.h"
#include "ccnl-pkt-ndntlv.h"

#include "dow.h"
#include "ccnlriot.h"
#include "ccnl_helper.h"


static char _prefix_str[CCNLRIOT_PFX_LEN];
#if DOW_CACHE_REPL_STRAT
static bool _first_time_full = false;
#endif

static bool _ccnl_helper_handle_content(struct ccnl_relay_s *relay, struct ccnl_pkt_s *pkt);
static void _remove_pit(struct ccnl_relay_s *relay, int num);
static ccnl_helper_removed_t _unflag_cs(struct ccnl_relay_s *relay, char *source);
static bool _check_in_range(uint32_t start, uint32_t val, uint32_t interval, uint32_t max);
static void _send_ack(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_prefix_s *pfx, int num);

/**
 * @brief local callback to handle incoming content chunks
 *
 * @note  Gets called from CCNL thread context
 *
 * @returns 1   if chunk is handled and no further processing should happen
 * @returns 0   otherwise
 **/
int ccnlriot_consumer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    (void) from;
    (void) relay;

    if (dow_state == DOW_STATE_STOPPED) {
        LOG_DEBUG("ccnl_helper: we're in stopped, do nothing\n");
        free_packet(pkt);
        return 1;
    }

    uint32_t cont_id = (uint32_t) strtol((char*)pkt->pfx->comp[2], NULL, 16);
    if (cont_id > dow_highest_id) {
        dow_highest_id = cont_id;
    }

    LOG_DEBUG("%" PRIu32 " ccnl_helper: local consumer for prefix: %s\n", xtimer_now().ticks32,
              ccnl_prefix_to_path_detailed(_prefix_str, pkt->pfx, 1, 0, 0));
    memset(_prefix_str, 0, CCNLRIOT_PFX_LEN);

#if DOW_DEPUTY
    /* XXX: might be unnecessary du to mutex now */
    /* if we're currently transferring our cache to the new deputy, we do not touch the content store */
    if (dow_state == DOW_STATE_HANDOVER) {
        LOG_DEBUG("ccnl_helper: we're in handover state, cannot touch content store right now\n");
        free_packet(pkt);
        return 1;
    }
#endif

    /* check if prefix is for ALL and contains an ACK */
    if ((ccnl_prefix_cmp(ccnl_helper_all_pfx, NULL, pkt->pfx, CMP_MATCH) >= 1) &&
        (strncmp((char*) pkt->content, CCNLRIOT_CONT_ACK, strlen(CCNLRIOT_CONT_ACK)) == 0)) {
        dow_content_t *cc = (dow_content_t*) pkt->content;
        LOG_DEBUG("ccnl_helper: content number is %i\n", cc->num);
        if (cc->num >= 0) {
            _remove_pit(relay, cc->num);
        }

        LOG_DEBUG("ccnl_helper: received ACK, flag the content\n");
        msg_t m = { .type = DOW_MSG_RECEIVED_ACK };
        msg_try_send(&m, dow_pid);
        free_packet(pkt);
        return 1;
    }

    struct ccnl_interest_s *i = NULL;
#if DOW_INT_INT
    /* if we don't have this content, we check if we have a matching PIT entry */
    bool is_dup = _cont_is_dup(pkt);
    if (!is_dup) {
        i = relay->pit;
        while (i) {
            if (ccnl_prefix_cmp(i->pkt->pfx, NULL, pkt->pfx, CMP_EXACT) == 0) {
                LOG_DEBUG("ccnl_helper: found matching interest, nothing to do\n");
                if (from->faceid == i->from->faceid) {
                    LOG_DEBUG("ccnl_helper: not bouncing chunk\n");
                    struct ccnl_face_s *loopback_face = ccnl_get_face_or_create(&ccnl_relay, -1, NULL, 0);
                    i->pending->face = loopback_face;
                }
                break;
            }
            i = i->next;
        }
    }
#endif

    /* for the non-deputy approach this shouldn't happen anyway, since content
     * dissemination is not interest driven
     * we don't have a PIT entry for this */
    if (!i) {
#if DOW_DEBUG
        dow_state = DOW_STATE_DEPUTY;
#endif
        /* if we're not deputy (or becoming it), we assume that this is our
         * own content */
        if (dow_state != DOW_STATE_DEPUTY) {
            char my_id[9];
            snprintf(my_id, sizeof(my_id), "%08lX", (unsigned long) dow_my_id);
#if !DOW_DEBUG
            if (memcmp(my_id, pkt->pfx->comp[2], 8) != 0) {
                LOG_DEBUG("ccnl_helper: not my content, ignore it\n");
                free_packet(pkt);
                return 1;
            }
#endif
            /* cache it */
            if (relay->max_cache_entries != 0) {
                LOG_DEBUG("ccnl_helper: adding content to cache\n");
#if DOW_INT_INT
                struct ccnl_prefix_s *new = ccnl_prefix_dup(pkt->pfx);
#endif
                struct ccnl_pkt_s *tmp = pkt;
                struct ccnl_content_s *c = ccnl_content_new(&ccnl_relay, &tmp);
                if (!c) {
                    LOG_ERROR("ccnl_helper: we're doomed, WE'RE ALL DOOMED\n");
                    return 0;
                }
                /* this is our own content, so we know that it is always the newest */
                _unflag_cs(relay, my_id);
                c->flags |= CCNL_CONTENT_FLAGS_USER1;
                c->flags |= CCNL_CONTENT_FLAGS_USER2;
                if (ccnl_content_add2cache(relay, c) == NULL) {
                    LOG_WARNING("ccnl_helper:  adding to cache failed, discard packet\n");
                    ccnl_free(c);
                    free_packet(pkt);
                }
#if DOW_INT_INT
                /* inform potential waiters in order to send out interest for own content.
                 * hence, this is only necessary in DoW mode */
                msg_t m = { .type = DOW_MSG_RECEIVED };
                m.content.ptr = (void*)new;
                msg_try_send(&m, dow_pid);
#endif
            }
            return 1;
        }
        else {
#if DOW_DEPUTY || DOW_PER
            int res = 1;
            /* create an interest if we're waiting for *, because otherwise
             * our PIT entry won't match */
            if (pkt->contlen == sizeof(dow_content_t)) {
                LOG_DEBUG("ccnl_helper: seems to be the right content\n");
                dow_content_t *cc = (dow_content_t*) pkt->content;
                LOG_DEBUG("%" PRIu32 " ccnl_helper: rcvd prefix: %s\n", xtimer_now(),
                          ccnl_prefix_to_path_detailed(_prefix_str, pkt->pfx, 1, 0, 0));
                LOG_DEBUG("ccnl_helper: content number is %i\n", cc->num);
                /* if we receive content, it's either because
                 *  - we asked for * -> num >= 0
                 *  - through loopback for content we generated ourselves
                 *  - we asked for it, because the generating node sent us an interest
                 */
                if (cc->num >= 0) {
                    _remove_pit(relay, cc->num);
                }
#if DOW_INT_INT
                if (is_dup) {
                    LOG_DEBUG("ccnl_helper: we already have this content, do nothing\n");
                }
                else {
                    if (!_ccnl_helper_handle_content(relay, pkt)) {
                        res = 0;
                    }
                }
#else
                /* check if we're registered for a certain prefix and compare the first character of it */
                /* XXX: use memcmp */

                if (dow_manual_id) {
                    if (_check_in_range(dow_my_id, cont_id, CCNLRIOT_CACHE_SIZE-1, dow_num_src)) {
                        if (!_ccnl_helper_handle_content(relay, pkt)) {
                            res = 0;
                        }
                    }
                }
                else if ((dow_is_registered && (pkt->pfx->comp[1][0] == dow_registered_prefix[1])) || DOW_DO_CACHE)) {
                    if (!_ccnl_helper_handle_content(relay, pkt)) {
                        res = 0;
                    }
                }
                else {
                    return 0;
                }
#endif

                if (cc->num >= 0) {
                    /* in case we're waiting for * chunks, try to send a message */
                    LOG_DEBUG("ccnl_helper: inform waiters\n");
                    msg_t m = { .type = DOW_MSG_RECEIVED };
                    msg_try_send(&m, dow_pid);
                }

                cc->num = -1;
                return res;
            }
            else {
                LOG_WARNING("ccnl_helper: content length is %i, was expecting %i\n", pkt->buf->datalen, sizeof(dow_content_t));
            }
#else
            /* check if we're registered for a certain prefix and compare the first character of it */
            /* XXX: use memcmp */
            if (dow_manual_id) {
                if (_check_in_range(dow_my_id, cont_id, CCNLRIOT_CACHE_SIZE-1, dow_num_src)) {
                    if (!_ccnl_helper_handle_content(relay, pkt)) {
                        return 0;
                    }
                    return 1;
                }
                free_packet(pkt);
                return 1;
            }
            else if ((dow_is_registered && (pkt->pfx->comp[1][0] == dow_registered_prefix[1])) || DOW_DO_CACHE) {
                if (!_ccnl_helper_handle_content(relay, pkt)) {
                    return 0;
                }
                return 1;
            }
#endif
        }
    }
    return 0;
}

/**
 * @brief local callback to handle incoming interests
 *
 * @note  Gets called from CCNL thread context
 *
 * @returns 1   if interest is handled and no further processing should happen
 * @returns 0   otherwise
 **/
int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    (void) from;
    int res = 0;

    if (dow_state == DOW_STATE_STOPPED) {
        LOG_DEBUG("ccnl_helper: we're in stopped, do nothing\n");
        free_packet(pkt);
        return 1;
    }

    LOG_DEBUG("%" PRIu32 " ccnl_helper: local producer for prefix: %s\n",
              xtimer_now().ticks32, ccnl_prefix_to_path_detailed(_prefix_str, pkt->pfx, 1, 0, 0));
    memset(_prefix_str, 0, CCNLRIOT_PFX_LEN);

#if DOW_INT_INT
    /* check if we have a PIT entry for this interest and a corresponding entry
     * in the content store */
    struct ccnl_interest_s *i = relay->pit;
    struct ccnl_content_s *c = relay->contents;
    while (i) {
        if (ccnl_prefix_cmp(i->pkt->pfx, NULL, pkt->pfx, CMP_EXACT) == 0) {
            LOG_DEBUG("ccnl_helper: found matching interest, let's check if we "
                      "have the content, too\n");

            break;
        }
        i = i->next;
    }
    if (i) {
        while (c) {
            if (ccnl_prefix_cmp(c->pkt->pfx, NULL, pkt->pfx, CMP_EXACT) == 0) {
                LOG_DEBUG("ccnl_helper: indeed, we have the content, too -> we "
                          "will serve it, remove PIT entry. (Pending interests "
                          "for own data: %u)\n", (unsigned) dow_prevent_sleep);
                /* inform ourselves that the interest was bounced back */
                msg_t m = { .type = DOW_MSG_RECEIVED_INT };
                msg_try_send(&m, dow_pid);

                ccnl_interest_remove(relay, i);
                dow_prevent_sleep--;
                /* go to sleep if we're not currently in handover mode */
                if (dow_state != DOW_STATE_HANDOVER) {
                    LOG_DEBUG("ccnl_helper: going back to sleep in %u microseconds (%i)\n",
                              DOW_KEEP_ALIVE_PERIOD, (int) dow_pid);
                    xtimer_set_msg(&_sleep_timer, DOW_KEEP_ALIVE_PERIOD, &_sleep_msg, dow_pid);
                }
                return 0;
            }
            c = c->next;
        }
        if (!c) {
            LOG_DEBUG("ccnl_helper: nope, we cannot serve this\n");
        }
    }
#endif

    /* avoid interest flooding */
    if (dow_state == DOW_STATE_INACTIVE) {
        LOG_DEBUG("ccnl_helper: pretend to be sleeping\n");
        free_packet(pkt);
        return 1;
    }

#if DOW_APMDMR
    if ((ccnl_prefix_cmp(ccnl_helper_all_pfx, NULL, pkt->pfx, CMP_MATCH) < 1) && (pkt->pfx->compcnt > 1)) {
        char p = pkt->pfx->comp[1][0];
        LOG_DEBUG("ccnl_helper: increase counter for %c (%s)\n", p, pkt->pfx->comp[1]);
        _prefix_seen_interest(p);
    }
#endif

    /* check if this is a for any data */
    if (((ccnl_prefix_cmp(ccnl_helper_all_pfx, NULL, pkt->pfx, CMP_MATCH) >= 1) ||
        (dow_is_registered && ccnl_prefix_cmp(ccnl_helper_my_pfx, NULL, pkt->pfx, CMP_MATCH) >= 2)) &&
#if DOW_PSR
        (ccnl_helper_flagged_cache >= (CCNLRIOT_CACHE_SIZE - 1))) {
#else
        (1)) {
#endif
        if ((dow_state == DOW_STATE_DEPUTY) || (dow_state == DOW_STATE_HANDOVER)) {

            dow_my_prefix_interest_count++;
            /* make sure interest contains a chunknumber */
            if ((pkt->pfx->chunknum == NULL) || (*(pkt->pfx->chunknum) == -1)) {
                LOG_WARNING("ccnl_helper: no chunknumber? what a fool!\n");
                free_packet(pkt);
                res = 1;
                goto out;
            }

#if DOW_DEPUTY
            /* check if this is a handover request */
            /* change state (will be done for each requested chunk, we don't care) */
            /* TODO: implement timeout to prevent that we stay in HANDOVER forever */
            LOG_INFO("\n\nccnl_helper: change to state HANDOVER\n\n");
            dow_state = DOW_STATE_HANDOVER;
#endif

            /* find corresponding chunk in store */
            struct ccnl_content_s *cit = relay->contents;
            LOG_DEBUG("ccnl_helper: received %s request for chunk number %i\n",
                      CCNLRIOT_ALL_PREFIX, *(pkt->pfx->chunknum));
#if DOW_RAND_SEEDING
            int pos = DOW_RANDOM_CHUNK;
#else
            int pos = *(pkt->pfx->chunknum);
#endif
            int i = 0;
            struct ccnl_content_s *failsafe = NULL;
            (void) failsafe;
            while ((i < pos) && (i < CCNLRIOT_CACHE_SIZE) && (cit != NULL)) {
                if (!dow_is_registered || (ccnl_prefix_cmp(ccnl_helper_my_pfx, NULL, cit->pkt->pfx, CMP_MATCH) >= 2)) {
                    failsafe = cit;
                    i++;
                }
                cit = cit->next;
            }
            LOG_DEBUG("ccnl_helper: try to publish chunk #%i (pos: %i)\n", i, pos);

#if !DOW_DEPUTY
            if ((i >= CCNLRIOT_CACHE_SIZE) || (cit == NULL)) {
                if (failsafe != NULL) {
                    LOG_WARNING("ccnl_helper: using failsafe!\n");
                    cit = failsafe;
                }
                else {
                    LOG_INFO("ccnl_helper: couldn't find anything matching, discard interest\n");
                    free_packet(pkt);
                    res = 1;
                    goto out;
                }
            }
#endif

            /* if we reached the end of the store, we send an ACK */
            if ((i >= CCNLRIOT_CACHE_SIZE) || (cit == NULL)) {
                LOG_INFO("ccnl_helper: reached end of content store, send ack\n");
                _send_ack(relay, from, ccnl_helper_all_pfx, i);

                /* we can go back to sleep now */
                /* TODO: delay this */
                msg_t m = { .type = DOW_MSG_INACTIVE };
                msg_send(&m, dow_pid);
                free_packet(pkt);
                res = 1;
                goto out;
            }

            /* otherwise we rewrite the name before we pass it down */

            /* save old prefix, so we can free its memory */
            struct ccnl_prefix_s *old = pkt->pfx;

            /* now create a new one */
            struct ccnl_prefix_s *new = ccnl_prefix_dup(cit->pkt->pfx);
            pkt->pfx = new;

            LOG_DEBUG("ccnl_helper: setting content num to %i for %p\n", i, (void*) cit->pkt->content);
            dow_content_t *cc = (dow_content_t*) cit->pkt->content;
            cc->num = i;

            LOG_DEBUG("%" PRIu32 " ccnl_helper: publish content for prefix: %s\n", xtimer_now().ticks32,
                      ccnl_prefix_to_path_detailed(_prefix_str, pkt->pfx, 1, 0, 0));
            memset(_prefix_str, 0, CCNLRIOT_PFX_LEN);

            /* set content number */
            if (cit == NULL) {
                LOG_ERROR("ccnl_helper: The world is a bad place, this should have never happened\n");
                res = 1;
                goto out;
            }

            /* free the old prefix */
            free_prefix(old);
            res = 0;
            goto out;
        }
        free_packet(pkt);
        res = 1;
        goto out;
    }
#if !DOW_INT_INT
    else {
        LOG_DEBUG("ccnl_helper: we cannot serve this, discard interest\n");
        free_packet(pkt);
        res = 1;
        goto out;
    }
#endif

    if (dow_state == DOW_STATE_HANDOVER) {
        LOG_DEBUG("ccnl_helper: in handover we handle nothing else\n");
        free_packet(pkt);
        res = 1;
        goto out;
    }

out:
    return res;
}

#if DOW_CACHE_REPL_STRAT
/**
 * @brief Caching strategy: oldest representative
 * Always cache at least one value per name and replace always the oldest value
 * for this name if cache is full. If no older value for this name exist,
 * replace the oldest value for any name that has different version in the
 * cache. If no such entry exists in the cache, replace the oldest value for
 * any name.
 */
int cs_oldest_representative(struct ccnl_relay_s *relay, struct ccnl_content_s *c)
{
    if (!_first_time_full) {
        ccnl_helper_reset();
        _first_time_full = true;
    }
    struct ccnl_content_s *c2, *oldest = NULL, *oldest_same = NULL,
                          *oldest_unflagged = NULL, *oldest_unregistered = NULL;
    long oldest_unregistered_ts = 0;
    long oldest_ts = 0;
    long oldest_same_ts = 0;
    long oldest_unflagged_ts = 0;

    unsigned count_user_flag2 = 0;

    for (c2 = relay->contents; c2; c2 = c2->next) {
        if (c->pkt->pfx->compcnt < 4) {
            LOG_WARNING("ccnl_helper: invalid prefix found in cache, skipping\n");
            continue;
        }
        if (c2->flags & CCNL_CONTENT_FLAGS_USER2) {
            count_user_flag2++;
        }
        long c2_ts = strtol((char*) c2->pkt->pfx->comp[3], NULL, 16);
        if (!(c2->flags & CCNL_CONTENT_FLAGS_STATIC)) {
            if (ccnl_prefix_cmp(c->pkt->pfx, NULL, c2->pkt->pfx, CMP_MATCH) >= 3) {
                if ((oldest_same_ts == 0) || (c2_ts < oldest_same_ts)) {
                    oldest_same_ts = c2_ts;
                    oldest_same = c2;
                }
            }
            if (!(c2->flags & CCNL_CONTENT_FLAGS_USER1)) {
                if ((oldest_unflagged_ts == 0) || (c2_ts < oldest_unflagged_ts)) {
                    oldest_unflagged_ts = c2_ts;
                    oldest_unflagged = c2;
                }
            }
            if ((oldest_ts == 0) || (c2_ts < oldest_ts)) {
                oldest_ts = c2_ts;
                oldest = c2;
            }
            if ((dow_is_registered && (c2->pkt->pfx->comp[1][0] != dow_registered_prefix[1]))) {
                if ((oldest_unregistered_ts == 0) || (c2_ts < oldest_unregistered_ts)) {
                    oldest_unregistered_ts = c2_ts;
                    oldest_unregistered = c2;
                }
            }
        }
    }
    long c_ts = strtol((char*) c->pkt->pfx->comp[3], NULL, 16);
    if (oldest_same_ts > c_ts) {
        LOG_DEBUG("New value is older than oldest value for this ID, skipping\n");
        return 1;
    }
    if (oldest_same) {
        mutex_unlock(&(relay->cache_write_lock));
        LOG_DEBUG("ccnl_helper: remove oldest entry for this prefix from cache\n");
        ccnl_content_remove(relay, oldest_same);
        mutex_lock(&(relay->cache_write_lock));
        ccnl_helper_flagged_cache = count_user_flag2;
        return 1;
    }
    if (oldest_unflagged) {
        mutex_unlock(&(relay->cache_write_lock));
        LOG_DEBUG("ccnl_helper: remove oldest unflagged entry from cache\n");
        ccnl_content_remove(relay, oldest_unflagged);
        mutex_lock(&(relay->cache_write_lock));
        ccnl_helper_flagged_cache = count_user_flag2;
        return 1;
    }
    if (oldest_unregistered) {
        mutex_unlock(&(relay->cache_write_lock));
        LOG_DEBUG("ccnl_helper: remove oldest entry that doesn't match our prefix from cache\n");
        ccnl_content_remove(relay, oldest_unregistered);
        mutex_lock(&(relay->cache_write_lock));
        ccnl_helper_flagged_cache = count_user_flag2;
        return 1;
    }
    if (oldest_ts > c_ts) {
        LOG_DEBUG("New value is older than oldest value, skipping\n");
        return 1;
    }
    if (oldest) {
        mutex_unlock(&(relay->cache_write_lock));
        LOG_DEBUG("ccnl_helper: remove oldest overall entry from cache\n");
        ccnl_content_remove(relay, oldest);
        mutex_lock(&(relay->cache_write_lock));
        ccnl_helper_flagged_cache = count_user_flag2;
        return 1;
    }
    return 1;
}
#endif

/**
 * @brief remove the PIT entry for the given chunk number for *
 */
static void _remove_pit(struct ccnl_relay_s *relay, int num)
{
    struct ccnl_interest_s *i = relay->pit;
    while (i) {
        if ((ccnl_prefix_cmp(ccnl_helper_all_pfx, NULL, i->pkt->pfx, CMP_MATCH) >= 1) &&
            (*(i->pkt->pfx->chunknum) == num)) {
            LOG_DEBUG("ccnl_helper: remove matching PIT entry\n");
            /* XXX: Do this properly! */
            ccnl_interest_remove(relay, i);
            return;
        }
        i = i->next;
    }
}

static ccnl_helper_removed_t _unflag_cs(struct ccnl_relay_s *relay, char *source)
{
    ccnl_helper_removed_t unflagged = CCNL_HELPER_REMOVED_NOTHING;
    struct ccnl_content_s *c = relay->contents;
    while (c) {
        if ((c->pkt->pfx->compcnt > 3) && (memcmp(source, c->pkt->pfx->comp[2],
                                                  DOW_CONT_LEN) == 0)) {
            if (c->flags & CCNL_CONTENT_FLAGS_USER1) {
                unflagged |= CCNL_HELPER_REMOVED_NEWEST_FLAG;
                c->flags &= ~CCNL_CONTENT_FLAGS_USER1;
            }
            if (c->flags & CCNL_CONTENT_FLAGS_STATIC) {
                unflagged |= CCNL_HELPER_REMOVED_PRIO_FLAG;
                c->flags &= ~CCNL_CONTENT_FLAGS_STATIC;
            }
        }
        c = c->next;
    }
    return unflagged;
}

static bool _check_in_range(uint32_t start, uint32_t val, uint32_t interval, uint32_t max)
{
    if (start > val) {
        return (((max - start) + val) < interval);
    }
    else {
        return ((val - start) < interval);
    }
}

static bool _ccnl_helper_handle_content(struct ccnl_relay_s *relay, struct ccnl_pkt_s *pkt)
{
    struct ccnl_content_s *c = ccnl_content_new(relay, &pkt);
    if (!c) {
        LOG_WARNING("DOOMED! DOOMED! DOOMED!\n");
        return false;
    }
    /* if someone is waiting for this content - should not hpapen!? */
    ccnl_content_serve_pending(relay, c);

    /* check if we have younger content for this source */
    struct ccnl_content_s *newest = NULL;
    /* get timestamp for incoming chunk */
    long c_ts = strtol((char*) c->pkt->pfx->comp[3], NULL, 16);
    long cit_ts;
    struct ccnl_content_s *cit;
    ccnl_helper_removed_t unflagged = CCNL_HELPER_REMOVED_NOTHING;
    for (cit = relay->contents; cit; cit = cit->next) {
        if (cit->pkt->pfx->compcnt < 4) {
            LOG_WARNING("ccnl_helper: invalid prefix found in cache, skipping\n");
            continue;
        }
        if (ccnl_prefix_cmp(cit->pkt->pfx, NULL, c->pkt->pfx, CMP_MATCH) >= 3) {
            /* unflag everything and flag the newest once we're done */
            if (cit->flags & CCNL_CONTENT_FLAGS_USER1) {
                cit->flags &= ~CCNL_CONTENT_FLAGS_USER1;
                unflagged |= CCNL_HELPER_REMOVED_NEWEST_FLAG;
            }
            if (cit->flags & CCNL_CONTENT_FLAGS_STATIC) {
                cit->flags &= ~CCNL_CONTENT_FLAGS_STATIC;
                unflagged |= CCNL_HELPER_REMOVED_PRIO_FLAG;
            }
            cit_ts = strtol((char*) cit->pkt->pfx->comp[3], NULL, 16);
            if (cit_ts > c_ts) {
                newest = cit;
                break;
            }
        }
    }
    /* no newer entry found, flag this one */
    if (newest == NULL) {
        (void) unflagged;
#if DOW_PRIO_CACHE
        /* if no newest flag was removed, we haven't had any value for this source */
        if (!(unflagged & CCNL_HELPER_REMOVED_NEWEST_FLAG) &&
            (dow_prio_cache_cnt < DOW_PRIO_CACHE)) {
            dow_prio_cache_cnt++;
            c->flags |= CCNL_CONTENT_FLAGS_STATIC;
        }
        else if (unflagged & CCNL_HELPER_REMOVED_PRIO_FLAG) {
            c->flags |= CCNL_CONTENT_FLAGS_STATIC;
        }
#endif
        c->flags |= CCNL_CONTENT_FLAGS_USER1;
    }
    /* if a newer entry was found, restore its flags */
    else {
        newest->flags |= CCNL_CONTENT_FLAGS_USER1;
#if DOW_PRIO_CACHE
        if (unflagged & CCNL_HELPER_REMOVED_PRIO_FLAG) {
            newest->flags |= CCNL_CONTENT_FLAGS_STATIC;
        }
#endif
    }

    if (relay->max_cache_entries != 0) { // it's set to -1 or a limit
        LOG_DEBUG("ccnl_helper:  adding content to cache\n");
        c->flags |= CCNL_CONTENT_FLAGS_USER2;
        if (ccnl_content_add2cache(relay, c) == NULL) {
            LOG_DEBUG("ccnl_helper:  adding to cache failed, discard packet\n");
            ccnl_free(c);
            free_packet(pkt);
            return false;
        }
    }
    else {
        LOG_DEBUG("ccnl_helper: content not added to cache\n");
        ccnl_free(c);
        free_packet(pkt);
        return false;
    }
    return true;
}

/**
 * @brief send an acknowledgement
 *
 * @param[in] relay     ccn-lite relay to use
 * @param[in] face      the face to use for sending
 * @param[in] pfx       the prefix to be acknowledged
 * @param[in] num       the chunk number to be acknowledged
 **/
static void _send_ack(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_prefix_s *pfx, int num)
{
    struct ccnl_content_s *c =
        ccnl_helper_create_cont(pfx, (unsigned char*)
                                CCNLRIOT_CONT_ACK,
                                strlen(CCNLRIOT_CONT_ACK) + 1, false, false);
    if (c == NULL) {
        return;
    }
    dow_content_t *cc = (dow_content_t*) c->pkt->content;
    cc->num = num;
    ccnl_face_enqueue(relay, from, ccnl_buf_new(c->pkt->buf->data,
                                                c->pkt->buf->datalen));

    free_packet(c->pkt);
    ccnl_free(c);
}


