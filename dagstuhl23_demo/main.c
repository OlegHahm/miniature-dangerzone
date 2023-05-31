/*
 * Copyright (C) 2019 Javier FILEIV <javier.fileiv@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file        main.c
 * @brief       Example using MQTT Paho package from RIOT
 *
 * @author      Javier FILEIV <javier.fileiv@gmail.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "timex.h"
#include "ztimer.h"
#include "shell.h"
#include "thread.h"
#include "mutex.h"
#include "paho_mqtt.h"
#include "MQTTClient.h"
#include "saul_reg.h"
#include "periph/gpio.h"
#include "periph/rtc.h"
#include "net/ntp_packet.h"
#include "net/sntp.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

static char _publisher_stack[THREAD_STACKSIZE_MAIN];
kernel_pid_t publisher_pid = KERNEL_PID_UNDEF;

#define BUF_SIZE                        1024
#define MQTT_VERSION_v311               4       /* MQTT v3.1.1 version is 4 */
#define COMMAND_TIMEOUT_MS              4000

#ifndef DEFAULT_MQTT_CLIENT_ID
#define DEFAULT_MQTT_CLIENT_ID          "RIOT"RIOT_BOARD
#endif

#ifndef DEFAULT_MQTT_USER
#define DEFAULT_MQTT_USER               "dagstuhl"
#endif

#ifndef DEFAULT_MQTT_PWD
#define DEFAULT_MQTT_PWD                "JointheRIOT"
#endif

#define DEFAULT_MQTT_HOST               "mqtt.dahahm.de"
/**
 * @brief Default MQTT port
 */
#define DEFAULT_MQTT_PORT               1883

#define DEFAULT_MQTT_TOPIC              "cmnd/delock/POWER"

#define TEMP_MQTT_TOPIC                 "dahahm/sensors/dagstuhl"

/**
 * @brief Keepalive timeout in seconds
 */
#define DEFAULT_KEEPALIVE_SEC           10

#ifndef MAX_LEN_TOPIC
#define MAX_LEN_TOPIC                   100
#endif

#ifndef MAX_TOPICS
#define MAX_TOPICS                      4
#endif

#define IS_CLEAN_SESSION                1
#define IS_RETAINED_MSG                 0

#define _DEFAULT_TIMEOUT (500000LU)
#define NTP_SERVER  "2001:470:7347:c888:a45d:54ff:fe4f:cdd7"

static MQTTClient client;
static Network network;
static int topic_cnt = 0;
static char _topic_to_subscribe[MAX_TOPICS][MAX_LEN_TOPIC];

static unsigned get_qos(const char *str)
{
    int qos = atoi(str);

    switch (qos) {
    case 1:     return QOS1;
    case 2:     return QOS2;
    default:    return QOS0;
    }
}

static void _on_msg_received(MessageData *data)
{
    printf("paho_mqtt_example: message received on topic"
           " %.*s: %.*s\n",
           (int)data->topicName->lenstring.len,
           data->topicName->lenstring.data, (int)data->message->payloadlen,
           (char *)data->message->payload);
}

static int _cmd_discon(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    topic_cnt = 0;
    int res = MQTTDisconnect(&client);
    if (res < 0) {
        printf("mqtt_example: Unable to disconnect\n");
    }
    else {
        printf("mqtt_example: Disconnect successful\n");
    }

    NetworkDisconnect(&network);
    return res;
}

static void _get_time(void)
{
    uint32_t timeout = _DEFAULT_TIMEOUT;

    sock_udp_ep_t server = { .port = NTP_PORT, .family = AF_INET6 };
    ipv6_addr_t *addr = (ipv6_addr_t *)&server.addr;


    if (ipv6_addr_from_str(addr, NTP_SERVER) == NULL) {
        puts("error: malformed address");
        return;
    }

    if (sntp_sync(&server, timeout) < 0) {
        puts("Error in synchronization");
        return;
    }
#if defined(MODULE_NEWLIB) || defined(MODULE_PICOLIBC)
    struct tm *tm;
    time_t time = (time_t)(sntp_get_unix_usec() / US_PER_SEC);

    tm = gmtime(&time);
    tm->tm_hour += 2;
    printf("%04i-%02i-%02i %02i:%02i:%02i UTC (%i us)\n",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec,
           (int)sntp_get_offset());
#ifndef BOARD_NATIVE
    if (rtc_set_time(tm) == -1) {
        puts("rtc: error setting time");
        return;
    }
#endif
#endif
}

static int _con(void)
{
    int ret = -1;

    char *remote_ip = DEFAULT_MQTT_HOST;
    int port = DEFAULT_MQTT_PORT;
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = MQTT_VERSION_v311;

    data.clientID.cstring = DEFAULT_MQTT_CLIENT_ID;
    data.username.cstring = DEFAULT_MQTT_USER;
    data.password.cstring = DEFAULT_MQTT_PWD;
    data.keepAliveInterval = DEFAULT_KEEPALIVE_SEC;
    data.cleansession = IS_CLEAN_SESSION;
    data.willFlag = 0;

    printf("mqtt_example: Trying to connect to %s, port: %d\n",
            remote_ip, port);
    ret = NetworkConnect(&network, remote_ip, port);
    if (ret < 0) {
        printf("mqtt_example: Unable to connect\n");
        return ret;
    }

    printf("user:%s clientId:%s password:%s\n", data.username.cstring,
             data.clientID.cstring, data.password.cstring);
    ret = MQTTConnect(&client, &data);
    if (ret < 0) {
        printf("mqtt_example: Unable to connect client %d\n", ret);
        _cmd_discon(0, NULL);
        return ret;
    }
    else {
        printf("mqtt_example: Connection successfully\n");
    }

    return (ret > 0) ? 0 : 1;
}

static int _pub(char *topic, char *msg, enum QoS qos)
{
    MQTTMessage message;
    message.qos = qos;
    message.retained = IS_RETAINED_MSG;
    message.payload = msg;
    message.payloadlen = strlen(message.payload);

    int rc;
    if ((rc = MQTTPublish(&client, topic, &message)) < 0) {
        printf("mqtt_example: Unable to publish (%d)\n", rc);
        _cmd_discon(0, NULL);
        _con();
    }
    else {
        printf("mqtt_example: Message (%s) has been published to topic %s "
               "with QOS %d\n",
               (char *)message.payload, topic, (int)message.qos);
    }

    return rc;
}

static int _cmd_pub(int argc, char **argv)
{
    enum QoS qos = QOS0;

    if (argc < 2) {
        printf("usage: %s <string msg> [QoS level]\n", argv[0]);
        return 1;
    }
    if (argc == 3) {
        qos = get_qos(argv[2]);
    }

    return _pub(DEFAULT_MQTT_TOPIC, argv[1], qos);
}

static int _cmd_sub(int argc, char **argv)
{
    enum QoS qos = QOS0;

    if (argc < 2) {
        printf("usage: %s <topic name> [QoS level]\n", argv[0]);
        return 1;
    }

    if (argc >= 3) {
        qos = get_qos(argv[2]);
    }

    if (topic_cnt > MAX_TOPICS) {
        printf("mqtt_example: Already subscribed to max %d topics,"
                "call 'unsub' command\n", topic_cnt);
        return -1;
    }

    if (strlen(argv[1]) > MAX_LEN_TOPIC) {
        printf("mqtt_example: Not subscribing, topic too long %s\n", argv[1]);
        return -1;
    }
    strncpy(_topic_to_subscribe[topic_cnt], argv[1], strlen(argv[1]));

    printf("mqtt_example: Subscribing to %s\n", _topic_to_subscribe[topic_cnt]);
    int ret = MQTTSubscribe(&client,
              _topic_to_subscribe[topic_cnt], qos, _on_msg_received);
    if (ret < 0) {
        printf("mqtt_example: Unable to subscribe to %s (%d)\n",
               _topic_to_subscribe[topic_cnt], ret);
        _cmd_discon(0, NULL);
    }
    else {
        printf("mqtt_example: Now subscribed to %s, QOS %d\n",
               argv[1], (int) qos);
        topic_cnt++;
    }
    return ret;
}

static int _cmd_unsub(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage %s <topic name>\n", argv[0]);
        return 1;
    }

    int ret = MQTTUnsubscribe(&client, argv[1]);

    if (ret < 0) {
        printf("mqtt_example: Unable to unsubscribe from topic: %s\n", argv[1]);
        _cmd_discon(0, NULL);
    }
    else {
        printf("mqtt_example: Unsubscribed from topic:%s\n", argv[1]);
        topic_cnt--;
    }
    return ret;
}

static int _print_time(char *out)
{
#ifndef BOARD_NATIVE
    struct tm t;
    if (rtc_get_time(&t) == 0) {
        return sprintf(out, "%02i-%02i-%02i %02i:%02i:%02i\n",
                       (t.tm_year + 1900) - 2000, t.tm_mon + 1, t.tm_mday,
                       t.tm_hour, t.tm_min, t.tm_sec
                      );
    }
#else
    (void) out;
#endif
    return -1;
}

static void _dahahm_publish(int sensor_data)
{
    char s_val[256U];
    _print_time(s_val);
    sprintf(s_val + 17, ".000000;5;50.0;");
    sprintf(s_val + 32, "%02i.%02i", sensor_data / 100, sensor_data % 100);
    sprintf(s_val + 37, ";990.18;1017.7779718583027;4;-60;1016;4;2791;170;1796;c196a3d861e3");
    _pub(TEMP_MQTT_TOPIC, s_val, QOS0);
}

static void *_publisher(void *_)
{
    (void) _;
    saul_reg_t *sensor = saul_reg_find_type(SAUL_SENSE_TEMP);
    phydat_t sensor_data;
    int sensor_read_result;
    while(sensor != NULL)
    {
        sensor_read_result = saul_reg_read(sensor, &sensor_data);
        if (sensor_read_result > 0)
        {
            _dahahm_publish(sensor_data.val[0]);
        }
        msg_t m;
        if (ztimer_msg_receive_timeout(ZTIMER_MSEC, &m, 5 * MS_PER_SEC) >= 0)
        {
            _pub(DEFAULT_MQTT_TOPIC, "toggle", QOS0);
        }
    }
    return NULL;
}

static const shell_command_t shell_commands[] =
{
    { "pub",    "publish something",                  _cmd_pub    },
    { "sub",    "subscribe topic",                    _cmd_sub    },
    { "unsub",  "unsubscribe from topic",             _cmd_unsub  },
    { NULL,     NULL,                                 NULL        }
};

bool toggle = false;

#if defined (BTN0_PIN) || defined (BTN1_PIN) || defined (BTN2_PIN) || defined (BTN3_PIN)
static void cb(void *arg)
{
    printf("Pressed BTN%d\n", (int)arg);
    toggle = true;
    msg_t m;
    msg_send_int(&m, publisher_pid);
}
#endif


static unsigned char buf[BUF_SIZE];
static unsigned char readbuf[BUF_SIZE];

int main(void)
{
    if (IS_USED(MODULE_GNRC_ICMPV6_ECHO)) {
        msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    }
#ifdef BTN0_PIN
    if (gpio_init_int(BTN0_PIN, BTN0_MODE, GPIO_FALLING, cb, NULL) < 0) {
        puts("[FAILED] init BTN0!");
        return 1;
    }
#endif
#ifdef MODULE_LWIP
    /* let LWIP initialize */
    ztimer_sleep(ZTIMER_MSEC, 1 * MS_PER_SEC);
#endif

    NetworkInit(&network);

    MQTTClientInit(&client, &network, COMMAND_TIMEOUT_MS, buf, BUF_SIZE,
                   readbuf,
                   BUF_SIZE);
    printf("Running Dagstuhl MQTT example. Have fun!\n");

    MQTTStartTask(&client);

    gnrc_netif_ipv6_wait_for_global_address(NULL, 2 * MS_PER_SEC);
    _get_time();
    _con();
    publisher_pid = thread_create(_publisher_stack, sizeof(_publisher_stack),
                                  THREAD_PRIORITY_MAIN-1,
                                  THREAD_CREATE_STACKTEST, _publisher, NULL,
                                  "publisher");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run_forever(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
