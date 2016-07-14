/*
 * Copyright (C) 2016 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       Basic ccn-lite relay example (produce and consumer via shell)
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#ifdef MODULE_TLSF
#include "tlsf-malloc.h"
#endif
#include "msg.h"
#include "shell.h"
#include "xtimer.h"
#include "net/gnrc/netapi.h"

#include "dow.h"
#include "ccnlriot.h"

/* main thread's message queue */
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#ifdef MODULE_TLSF
static uint32_t _tlsf_heap[TLSF_BUFFER];
#endif

char dow_registered_prefix[3];
bool dow_is_registered = false;
char dow_sensors[DOW_SENSOR_MAX_NR][3];
uint8_t dow_sensor_nr = 0;

static int _stats(int argc, char **argv);
static int _cs(int argc, char **argv);
static int _debug_cache_date(int argc, char **argv);
static int _start_dow(int argc, char **argv);
static int _sub_prefix(int argc, char **argv);
static int _add_sensor(int argc, char **argv);

const shell_command_t shell_commands[] = {
/*  {name, desc, cmd },                         */
    {"stats", "Print CCNL statistics", _stats},
    {"c", "Print CCNL content store", _cs},
    {"dc", "Create a content chunk and put it into cache for debug purposes",
        _debug_cache_date},
    {"start", "Start the application", _start_dow},
    {"prefix", "Subscribe a certain prefix", _sub_prefix},
    {"sensor", "Add a sensor with a certain prefix", _add_sensor},
    {NULL, NULL, NULL}
};

int _stats(int argc, char **argv) {
    (void) argc; (void) argv;
    printf("RX: %04" PRIu32 ", TX: %04" PRIu32 "\n", ccnl_relay.ifs[0].rx_cnt, ccnl_relay.ifs[0].tx_cnt);
    if (dow_sleeping) {
        printf("active: %u, sleeping: %u\n", (unsigned) dow_time_active, (unsigned) (dow_time_sleeping + (xtimer_now() - dow_ts_wakeup)));
    }
    else {
        printf("active: %u, sleeping: %u\n", (unsigned) (dow_time_active + (xtimer_now() - dow_ts_sleep)), (unsigned) dow_time_sleeping);
    }
    return 0;
}

int _cs(int argc, char **argv) {
    (void) argc; (void) argv;
#if DOW_DEBUG
    gnrc_netapi_get(ccnl_pid, NETOPT_CCN, CCNL_CTX_PRINT_CS, &ccnl_relay, sizeof(ccnl_relay));
#else
    printf("%u CS command\n", (unsigned) xtimer_now());
    //if ((dow_state != DOW_STATE_INACTIVE) && (dow_state != DOW_STATE_HANDOVER)) {
    if (dow_state != DOW_STATE_INACTIVE)  {
        gnrc_netapi_get(ccnl_pid, NETOPT_CCN, CCNL_CTX_PRINT_CS, &ccnl_relay, sizeof(ccnl_relay));
        //ccnl_cs_dump(&ccnl_relay);
        printf("%u DONE\n", (unsigned) xtimer_now());
    }
    else {
        puts("NOP");
    }
#endif
    return 0;
}

static int _debug_cache_date(int argc, char **argv)
{
    if ((argc < 3) || (strlen(argv[1]) > 8) || (strlen(argv[2]) > 8)) {
        puts("Usage: dc <ID> <timestamp>");
        return 1;
    }

    /* get data into cache by sending to loopback */
    LOG_DEBUG("main: put data into cache via loopback\n");
    size_t prefix_len = sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 9 + 9;
    char pfx[prefix_len];
    snprintf(pfx, prefix_len, "%s%s/%s/%s", CCNLRIOT_SITE_PREFIX, CCNLRIOT_TYPE_PREFIX, argv[1], argv[2]);
    LOG_INFO("main: DEBUG DATA: %s\n", pfx);
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (prefix == NULL) {
        LOG_ERROR("caching: We're doomed, WE ARE ALL DOOMED!\n");
    }
    else {
        ccnl_helper_create_cont(prefix, (unsigned char*) argv[2], strlen(argv[2]) + 1, true, false);
        free_prefix(prefix);
    }
    return 0;
}

static int _start_dow(int argc, char **argv)
{
    (void) argc; (void) argv;

    printf("Settings: %s D:%u X:", (DOW_DEPUTY ? "Deputy" : ""), DOW_D);
    if ((DOW_X) < 1) {
        printf("0.%u", (unsigned) (100 *  DOW_X));
    }
    else {
        printf("%u", (unsigned) DOW_X);
    }
    printf(" p:%u Y:%u CS:%u P-MDMR:%u Q:%u PER:%u KEEP_ALIVE:%u PSR:%u BC:%u\n",
           (unsigned) (100U * DOW_P), DOW_Y, CCNLRIOT_CACHE_SIZE,
           (unsigned) DOW_PRIO_CACHE, (unsigned) (100U * DOW_Q),
           (unsigned) DOW_PER, (unsigned) DOW_KEEP_ALIVE_PFX,
           (unsigned) DOW_PSR, (unsigned) DOW_BC_COUNT);

    dow_init();
    thread_yield_higher();
    return 0;
}

static int _sub_prefix(int argc, char **argv)
{
    if (dow_is_registered) {
        puts("Already registered, failing.");
        return 1;
    }
    if (argc < 2) {
        printf("Usage: %s <prefix>\n", argv[0]);
        return 1;
    }
    ccnl_helper_subsribe(argv[1][0]);

    return 0;
}

static int _add_sensor(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <sensor prefix>\n", argv[0]);
        return 1;
    }

    if (dow_sensor_nr >= DOW_SENSOR_MAX_NR) {
        puts("Max. number of sensors already registered.");
        return 1;
    }

    dow_sensors[dow_sensor_nr][0] = '/';
    dow_sensors[dow_sensor_nr][1] = argv[1][0];
    dow_sensors[dow_sensor_nr][2] = '\0';

    printf("added sensor %i: %s\n", dow_sensor_nr, dow_sensors[dow_sensor_nr]);

#if DOW_CACHE_REPL_STRAT
    if (!dow_is_registered) {
        ccnl_helper_subsribe(dow_sensors[dow_sensor_nr][1]);
    }
#endif

    dow_sensor_nr++;

    return 0;
}

int main(void)
{
#ifdef MODULE_TLSF
    tlsf_create_with_pool(_tlsf_heap, sizeof(_tlsf_heap));
#endif
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    printf("CCN caching started\n");

#if DOW_AUTOSTART
    dow_init();
    thread_yield_higher();
#endif
    /* start the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
