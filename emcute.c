/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example application for demonstrating RIOT's MQTT-SN library
 *              emCute
 *
 * @}
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shell.h"
#include "msg.h"
#include "net/emcute.h"
#include "net/ipv6/addr.h"
#include "thread.h"
#include "periph/adc.h"
#include "xtimer.h"
#include "saul.h"
#include "saul_reg.h"


#ifndef EMCUTE_ID
#define EMCUTE_ID           ("gertrud")
#endif
#define EMCUTE_PRIO         (THREAD_PRIORITY_MAIN - 1)

#define NUMOFSUBS           (16U)
#define TOPIC_MAXLEN        (64U)

static char stack[THREAD_STACKSIZE_DEFAULT];
static msg_t queue[8];

static emcute_sub_t subscriptions[NUMOFSUBS];
static char topics[NUMOFSUBS][TOPIC_MAXLEN];
static bool message_received_flag = false;

/* Function: Main emcute thread */
static void *emcute_thread(void *arg)
{
    (void)arg;
    emcute_run(CONFIG_EMCUTE_DEFAULT_PORT, EMCUTE_ID);
    return NULL;    /* should never be reached */
}

/* Function: Callback for received messages */
static void on_pub(const emcute_topic_t *topic, void *data, size_t len)
{
    char *in = (char *)data;

    printf("### Got publication for topic '%s' [%i] ###\n", topic->name, (int)topic->id);
    printf("Message payload: %.*s\n", (int)len, in);
    puts("");

    /* Check if the message is from the "telegram/data" topic */
    if (strcmp(topic->name, "telegram/data") == 0) {
        printf("Message received from 'telegram/data'. Preparing reply...\n");
        message_received_flag = true;
    }

}

  // Include the temperature peripheral header

// static void pub_to_sensor_data(const emcute_topic_t *topic, void *data, size_t len) {
//     /* Mark unused parameters */
//     (void)topic;
//     (void)data;
//     (void)len;

//     /* Define the target topic */
//     const char *target_topic = "sensor/data";
//     emcute_topic_t t;
//     t.name = target_topic;

//     /* Retry mechanism for registering the topic */
//     int retry_count = 3; // Maximum retries
//     int delay_between_retries = 1; // Delay in seconds
//     int success = 0;

//     for (int i = 0; i < retry_count; i++) {
//         if (emcute_reg(&t) == EMCUTE_OK) {
//             success = 1;
//             printf("Successfully registered topic '%s' with ID %d\n", target_topic, t.id);
//             break;
//         } else {
//             printf("Error: Unable to obtain topic ID for '%s' (Attempt %d/%d)\n", target_topic, i + 1, retry_count);
//             xtimer_sleep(delay_between_retries);
//         }
//     }

//     if (!success) {
//         printf("Failed to register topic '%s' after %d attempts. Aborting publish.\n", target_topic, retry_count);
//         return;
//     }

//     /* Add a short wait to ensure the REGACK is processed */
//     xtimer_sleep(1);

//     /* Configure ADC */
//     int adc_line = 0; // ADC line number
//     int resolution = ADC_RES_10BIT; // 10-bit resolution (adjust as needed)
//     if (adc_init(adc_line) < 0) {
//         printf("Error: Unable to initialize ADC line %d\n", adc_line);
//         return;
//     }

//     /* Read temperature data from ADC */
//     int16_t adc_value = adc_sample(adc_line, resolution);
//     if (adc_value < 0) {
//         printf("Error: Failed to read ADC line %d\n", adc_line);
//         return;
//     }

//     /* Convert ADC value to temperature (example conversion; adjust for your sensor) */
//     float voltage = (adc_value / (float)(1 << resolution)) * 3.3; // Assuming 3.3V reference
//     int temperature = (int)(voltage * 100); // Example scaling (adjust for your sensor)

//     /* Prepare the message with temperature data */
//     char message[64];
//     snprintf(message, sizeof(message), "{\"temperature\": %d.%02d}", temperature / 100, temperature % 100);

//     /* Publish the temperature data */
//     if (emcute_pub(&t, message, strlen(message), EMCUTE_QOS_0) != EMCUTE_OK) {
//         printf("Error: Unable to publish data to topic '%s'\n", target_topic);
//     } else {
//         printf("Published temperature data to topic '%s': %s\n", target_topic, message);
//     }
// }

static void pub_to_sensor_data(const emcute_topic_t *topic, void *data, size_t len) {
    (void)topic;
    (void)data;
    (void)len;

    const char *target_topic = "sensor/data";
    emcute_topic_t t;
    t.name = target_topic;

    int retry_count = 3; 
    int delay_between_retries = 1; 
    int success = 0;

    for (int i = 0; i < retry_count; i++) {
        if (emcute_reg(&t) == EMCUTE_OK) {
            success = 1;
            printf("Successfully registered topic '%s' with ID %d\n", target_topic, t.id);
            break;
        } else {
            printf("Error: Unable to obtain topic ID for '%s' (Attempt %d/%d)\n", target_topic, i + 1, retry_count);
            xtimer_sleep(delay_between_retries);
        }
    }

    if (!success) {
        printf("Failed to register topic '%s' after %d attempts. Aborting publish.\n", target_topic, retry_count);
        return;
    }

    xtimer_sleep(1);

    /* Read temperature data using SAUL */
    saul_reg_t *dev = saul_reg_find_type(SAUL_SENSE_TEMP);
    if (dev == NULL) {
        puts("Error: No temperature sensor found");
        return;
    }

    phydat_t temp_data;
    if (saul_reg_read(dev, &temp_data) < 0) {
        puts("Error: Failed to read temperature sensor");
        return;
    }

    /* Format and publish the temperature data */
    char message[64];
    snprintf(message, sizeof(message), "{\"temperature\": %d.%02d}", temp_data.val[0] / 100, abs(temp_data.val[0] % 100));

    if (emcute_pub(&t, message, strlen(message), EMCUTE_QOS_0) != EMCUTE_OK) {
        printf("Error: Unable to publish data to topic '%s'\n", target_topic);
    } else {
        printf("Published temperature data to topic '%s': %s\n", target_topic, message);
    }
}


  
/* Main Function */
int main(void)
{
    puts("MQTT-SN example application\n");
    puts("Type 'help' to get started. Refer to the README.md for more information.");

    /* Initialize the message queue */
    msg_init_queue(queue, ARRAY_SIZE(queue));

    /* Initialize subscription buffers */
    memset(subscriptions, 0, (NUMOFSUBS * sizeof(emcute_sub_t)));

    /* Start the emcute thread */
    thread_create(stack, sizeof(stack), EMCUTE_PRIO, 0, emcute_thread, NULL, "emcute");

    /* Hardcoded connection to the broker */
    sock_udp_ep_t gw = { .family = AF_INET6, .port = 1883 };
    if (ipv6_addr_from_str((ipv6_addr_t *)&gw.addr.ipv6, "2600:1f18:3cfc:b60:9ba9:7acb:7f8:daeb") == NULL) {
        puts("Error parsing hardcoded IPv6 address");
        return 1;
    }

    if (emcute_con(&gw, true, NULL, NULL, 0, 0) != EMCUTE_OK) {
        printf("Error: Unable to connect to [%s]:%i\n", "2600:1f18:3cfc:b60:9ba9:7acb:7f8:daeb", 1883);
        return 1;
    }
    printf("Successfully connected to gateway at [%s]:%i\n", "2600:1f18:3cfc:b60:9ba9:7acb:7f8:daeb", 1883);

    /* Subscribe to 'telegram/data' */
    unsigned flags = EMCUTE_QOS_0;  // QoS level 0
    char *topic_name = "telegram/data";

    /* Find an empty subscription slot */
    unsigned i = 0;
    for (; (i < NUMOFSUBS) && (subscriptions[i].topic.id != 0); i++) {}
    if (i == NUMOFSUBS) {
        puts("Error: No memory to store new subscriptions");
        return 1;
    }
    subscriptions[i].cb = on_pub;  // Callback function for received messages
    strcpy(topics[i], topic_name);
    subscriptions[i].topic.name = topics[i];

    if (emcute_sub(&subscriptions[i], flags) != EMCUTE_OK) {
        printf("Error: Unable to subscribe to %s\n", topic_name);
        return 1;
    }

    printf("Now subscribed to '%s'\n", topic_name);

    /* Main thread continues running */
    while (1) {
        /* Ensure the main thread is alive */
        if (message_received_flag) {
             printf("Message received flag is set! Triggering response actions...\n");
            pub_to_sensor_data(NULL, NULL, 0);
            //resetting the flag
             message_received_flag = false;
        }
        xtimer_sleep(10);
    }

    /* This should never be reached */
    return 0;
}
