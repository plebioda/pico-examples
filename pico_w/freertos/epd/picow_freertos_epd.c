/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "pico/time.h"
#include "pico/types.h"

#include "hardware/rtc.h"
#include "hardware/exception.h"
#include "hardware/flash.h"

#include "lwip/ip4_addr.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/sntp.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "EPD_2in66.h"
#include "GUI_Paint.h"

#include "bme280_i2c_dev.h"
#include "bme68x_i2c_dev.h"
#include "bsec_handler.h"
#include "util.h"

#include <time.h>

#ifndef RUN_FREERTOS_ON_CORE
#define RUN_FREERTOS_ON_CORE 0
#endif

#define TEST_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

// #define TASK_CORE_NETWORK 0
#define TASK_CORE_DEV 0
// #define TASK_CORE_EPD   1

#define BME280_TASK 0
#define BME68X_TASK 1
#define BSEC_TASK 2
#define DEV_TASK BSEC_TASK

#define MQTT_QUEUE_LENGTH 20
#define EPD_QUEUE_LENGTH 20
#define DIFF_SEC_1970_2036 ((u32_t)2085978496L)

static QueueHandle_t xMqttQueue = NULL;
static QueueHandle_t xEpdQueue = NULL;

#define EPD_TASK_QUEUE_WAIT_MS 200
#define EPD_TASK_QUEUE_WAIT_TICKS (portTICK_PERIOD_MS * EPD_TASK_QUEUE_WAIT_MS)

static bool do_update = false;

void sntp_set_system_time(u32_t sec)
{
    if (do_update)
    {
        time_t ut = (time_t)sec;

        struct tm tm;
        printf("sntp_set_system_time: sec=%lu\n", sec);
        printf("sntp_set_system_time: sec - 2036=%lu\n", sec - DIFF_SEC_1970_2036);
        gmtime_r(&ut, &tm);
        tm.tm_hour = tm.tm_hour + 1;
        if (tm.tm_isdst)
        {
            tm.tm_hour = tm.tm_hour + 1;
        }

        printf("tm = %d.%d.%d %d:%d:%d\n", tm.tm_mday, tm.tm_mon, tm.tm_year, tm.tm_hour, tm.tm_min, tm.tm_sec);

        datetime_t datetime = {
            .year = tm.tm_year + 1900,
            .month = tm.tm_mon,
            .day = tm.tm_mday,
            .dotw = tm.tm_wday,
            .hour = tm.tm_hour,
            .min = tm.tm_min,
            .sec = tm.tm_sec};

        char str_buf[256];
        printf("datetime = %s\n", datetime_str(str_buf, 256, &datetime));

        bool success = rtc_set_datetime(&datetime);
        if (!success)
        {
            printf("error: rtc_set_datetime failed\n");
        }
    }
}

static void init_i2c0()
{
    // I2C is "open drain", pull ups to keep signal high when no data is being sent
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
}

void bme280_dev_task_fn(__unused void *params)
{
    init_i2c0();

    struct bme280_i2c_dev bme280_i2c_dev;
    int8_t ret = bme280_i2c_dev_init(&bme280_i2c_dev, i2c0, BME280_I2C_ADDR_PRIM);
    if (ret)
    {
        printf("bme280_i2c_dev_init returned %d\n", ret);
        goto err_bme280_i2c_dev_init;
    }

    struct bme280_data old_bme280_data = BME280_DATA_INIT_VALUE;
    struct bme280_data cur_bme280_data = BME280_DATA_INIT_VALUE;
    while (true)
    {
        ret = bme280_i2c_dev_read_data(&bme280_i2c_dev, &cur_bme280_data);
        if (!ret)
        {

            printf("bme280_data: T: %.2lf C H: %.2lf %% P: %.2lf hPA\n",
                   cur_bme280_data.temperature,
                   cur_bme280_data.humidity,
                   BME280_hPA(cur_bme280_data.pressure));

            bool entry_changed = ((cur_bme280_data.humidity != old_bme280_data.humidity) ||
                                  (cur_bme280_data.temperature != old_bme280_data.temperature) ||
                                  (cur_bme280_data.pressure != old_bme280_data.pressure));

            if (entry_changed)
            {
                xQueueSend(xMqttQueue, &cur_bme280_data, 0);
                xQueueSend(xEpdQueue, &cur_bme280_data, 0);
            }

            old_bme280_data = cur_bme280_data;
        }
        else
        {
            printf("bme280_i2c_dev_read_data failed!\n");
        }

        vTaskDelay(1000);
    }
err_bme280_i2c_dev_init:
    i2c_deinit(i2c0);
    return;
}

#define BSEC_STATE_DELAY_SEC 10

struct bsec_handler_state default_bsec_handler_state = {
    .serialized_state = {
        0x00, 0x08, 0x04, 0x01, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00,
        0x2d, 0x00, 0x01, 0x20, 0x68, 0xaf, 0xb4, 0x40, 0x27, 0xd6, 0xb4, 0x40, 0xb6, 0x30, 0xab, 0x40,
        0x84, 0x01, 0xab, 0x40, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x22, 0x0c, 0x00, 0x02,
        0x20, 0xba, 0x25, 0x67, 0x42, 0xe8, 0xb3, 0x89, 0x42, 0x10, 0x00, 0x03, 0x20, 0x33, 0x35, 0xaa,
        0x40, 0xed, 0x0d, 0xe3, 0x41, 0xfb, 0xa0, 0x07, 0x42, 0x16, 0x00, 0x05, 0x20, 0x70, 0x7b, 0xe5,
        0x52, 0x3a, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2f, 0x10, 0x51, 0x01, 0x00, 0x0c,
        0x00, 0x09, 0x20, 0x70, 0x7b, 0xe5, 0x52, 0x3a, 0x01, 0x00, 0x00, 0x08, 0x00, 0x0a, 0x20, 0xd8,
        0x96, 0xe3, 0x41, 0xa5, 0xa5, 0xa5, 0xa5, 0x10, 0xe7, 0x00, 0x00
    },
    .serialized_state_size = 139
};

#define FLASH_TARGET_OFFSET (256 * 1024)

struct flash_layout {
    struct bsec_handler_state bsec_handler_state;
    struct sha1 bsec_handler_state_sha1;
};

union flash {
    struct flash_layout layout;
    uint8_t page[FLASH_PAGE_SIZE];
};

union flash *flash = (union flash *)(XIP_BASE + FLASH_TARGET_OFFSET);
union flash flash_mirror;

void bsec_dev_task_fn(__unused void *params)
{
    memcpy(&flash_mirror, flash, sizeof(flash_mirror));
    struct bsec_handler_state bsec_handler_state;
    int8_t ret;
    struct sha1 bsec_handler_state_sha1;
    ret = sha1_get(&bsec_handler_state_sha1, (const void *)&flash->layout.bsec_handler_state, sizeof(flash->layout.bsec_handler_state));
    if (ret)
    {
        printf("sha1_get for bsec_handler_state from flash returned %d\n", ret);
        return;
    }

    if (memcmp(&bsec_handler_state_sha1, &flash->layout.bsec_handler_state_sha1, sizeof(bsec_handler_state_sha1)))
    {
        printf("SHA1 mismatch, using default state\n");
        memcpy(&bsec_handler_state, &default_bsec_handler_state, sizeof(bsec_handler_state));
    }
    else
    {
        printf("using bsec_handler_state from flash\n");
        memcpy(&bsec_handler_state, &flash->layout.bsec_handler_state, sizeof(bsec_handler_state));
    }

    init_i2c0();

    struct bme68x_i2c_dev bme68x_i2c_dev;
    ret = bme68x_i2c_dev_init(&bme68x_i2c_dev, i2c0, BME68X_I2C_ADDR_LOW);
    if (ret)
    {
        printf("bme68x_i2c_dev_init returned %d\n", ret);
        return;
    }

    struct bsec_handler bsec_handler;
    ret = bsec_handler_init(&bsec_handler, &bme68x_i2c_dev);
    if (ret)
    {
        printf("bsec_handler_init returned %d\n", ret);
        return;
    }

    ret = bsec_handler_set_state(&bsec_handler, &bsec_handler_state);
    if (ret)
    {
        printf("bsec_handler_subscribe_all returned %d\n", ret);
        return;
    }

    ret = bsec_handler_subscribe_all(&bsec_handler, BSEC_SAMPLE_RATE_CONTINUOUS);
    ;
    if (ret)
    {
        printf("bsec_handler_subscribe_all returned %d\n", ret);
        return;
    }

    int64_t bsec_get_state_next_timestamp_ns = 0;

    int64_t start_absolute_time_ns = (int64_t)us_to_ns(to_us_since_boot(get_absolute_time()));
    int64_t next_call_timestamp_ns = 0U;
    while (true)
    {
        int64_t absolute_time_ns = (int64_t)us_to_ns(to_us_since_boot(get_absolute_time()));
        int64_t relative_time_ns = absolute_time_ns - start_absolute_time_ns;

        if (relative_time_ns >= next_call_timestamp_ns)
        {
            ret = bsec_handler_run(&bsec_handler, relative_time_ns, &next_call_timestamp_ns);
            if (ret)
            {
                printf("bsec_handler_run returned %d\n", ret);
            }
        }

        if (relative_time_ns >= bsec_get_state_next_timestamp_ns)
        {
            ret = bsec_handler_get_state(&bsec_handler, &bsec_handler_state);
            if (ret)
            {
                printf("bsec_handler_get_state returned %d\n", ret);
            }
            else
            {
                print_bsec_handler_state(&bsec_handler_state);
                struct sha1 sha1;
                ret = sha1_get(&sha1, (const void *)&bsec_handler_state, sizeof(bsec_handler_state));
                if (ret) {
                    printf("get_sha1 returned %d\n", ret);
                }
                else
                {
                    memcpy(&flash_mirror.layout.bsec_handler_state, &bsec_handler_state, sizeof(flash_mirror.layout.bsec_handler_state));
                    memcpy(&flash_mirror.layout.bsec_handler_state_sha1, &sha1, sizeof(flash_mirror.layout.bsec_handler_state_sha1));

                    // flash_range_erase(FLASH_TARGET_OFFSET, FLASH_PAGE_SIZE);
                    // flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&flash_mirror, FLASH_PAGE_SIZE);

                    char buff[256];
                    printf("bsec_hander_state, sha1 = %s\n", sha1_to_str(buff, sizeof(buff), &sha1));
                }
            }
            bsec_get_state_next_timestamp_ns += sec_to_ns(BSEC_STATE_DELAY_SEC);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void bme68x_dev_task_fn(__unused void *params)
{
    init_i2c0();

    struct bme68x_i2c_dev bme68x_i2c_dev;
    int8_t ret = bme68x_i2c_dev_init(&bme68x_i2c_dev, i2c0, BME68X_I2C_ADDR_LOW);
    if (ret)
    {
        printf("bme68x_i2c_dev_init returned %d\n", ret);
        goto err_bme68x_i2c_dev_init;
    }

    // struct bme68x_data old_bme68x_data = BME68X_DATA_INIT_VALUE;
    struct bme68x_data cur_bme68x_data = BME68X_DATA_INIT_VALUE;
    while (true)
    {
        ret = bme68x_i2c_dev_read_data(&bme68x_i2c_dev, &cur_bme68x_data);
        if (!ret)
        {

            printf("bme68x_data: T: %.2lf C H: %.2lf %% P: %.2lf hPA G: %.2lf Ohm\n",
                   cur_bme68x_data.temperature,
                   cur_bme68x_data.humidity,
                   BME68X_hPA(cur_bme68x_data.pressure),
                   cur_bme68x_data.gas_resistance);

            // bool entry_changed = (
            //     (cur_bme68x_data.humidity != old_bme68x_data.humidity) ||
            //     (cur_bme68x_data.temperature != old_bme68x_data.temperature) ||
            //     (cur_bme68x_data.pressure != old_bme68x_data.pressure)
            // );

            // old_bme68x_data = cur_bme68x_data;
        }
        else
        {
            printf("bme68x_i2c_dev_read_data failed!\n");
        }

        vTaskDelay(1000);
    }
err_bme68x_i2c_dev_init:
    i2c_deinit(i2c0);
    return;
}

void epd_task_fn(__unused void *params)
{
    printf("Starting EPD task\n");
    if (DEV_Module_Init() != 0)
    {
        printf("DEV_Module_init failed\n");
        return;
    }

    printf("e-Paper Init and Clear...\r\n");
    EPD_2IN66_Init();
    EPD_2IN66_Clear();
    DEV_Delay_ms(500);

    // Create a new image cache
    UBYTE *BlackImage;
    /* you have to edit the startup_stm32fxxx.s file and set a big enough heap size */
    UWORD Imagesize = ((EPD_2IN66_WIDTH % 8 == 0) ? (EPD_2IN66_WIDTH / 8) : (EPD_2IN66_WIDTH / 8 + 1)) * EPD_2IN66_HEIGHT;
    if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
        return;
    }
    printf("Paint_NewImage\r\n");
    Paint_NewImage(BlackImage, EPD_2IN66_WIDTH, EPD_2IN66_HEIGHT, 270, WHITE);

    printf("show image for array\r\n");
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);

    Paint_DrawString_EN(10, 0, "rpi pico w", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(10, 20, "bme280 + ePaper display demo", &Font12, WHITE, BLACK);

    EPD_2IN66_Display(BlackImage);
    DEV_Delay_ms(4000);

    printf("EPD_2IN66_DisplayPart\r\n");
    EPD_2IN66_Init_Partial();

    Paint_SelectImage(BlackImage);

    struct bme280_data old_bme280_data = BME280_DATA_INIT_VALUE;
    struct bme280_data cur_bme280_data = BME280_DATA_INIT_VALUE;
    datetime_t old_datetime = DATETIME_INIT_VALUE;
    datetime_t cur_datetime = DATETIME_INIT_VALUE;
    printf("Starting EPD queue\n");
    while (true)
    {
        bool entry_new = (xQueueReceive(xEpdQueue, &cur_bme280_data, EPD_TASK_QUEUE_WAIT_TICKS) == pdTRUE);
        bool entry_changed = entry_new ? (
                                             (cur_bme280_data.humidity != old_bme280_data.humidity) ||
                                             (cur_bme280_data.temperature != old_bme280_data.temperature) ||
                                             (cur_bme280_data.pressure != old_bme280_data.pressure))
                                       : false;

        bool datetime_valid = 0; // rtc_get_datetime(&cur_datetime);
        bool datetime_changed = (datetime_valid && old_datetime.sec != cur_datetime.sec);

        if (entry_changed || datetime_changed)
        {
            Paint_ClearWindows(0, 40, 300, 152, WHITE);
            Paint_ClearWindows(0, 60, 300, 152, WHITE);
            Paint_ClearWindows(0, 80, 300, 152, WHITE);
            Paint_ClearWindows(0, 100, 300, 152, WHITE);
            Paint_ClearWindows(0, 120, 300, 152, WHITE);

            char str_buf[64];
            printf("datetime: %s\n", datetime_str(str_buf, 64, &cur_datetime));

            Paint_DrawString_EN(0, 40, datetime_str_date(str_buf, 64, &cur_datetime), &Font16, WHITE, BLACK);
            Paint_DrawString_EN(0, 60, datetime_str_time(str_buf, 64, &cur_datetime), &Font20, WHITE, BLACK);

            printf("epd: bme280_data: T: %.2lf C H: %.2lf %% P: %.2lf hPA\n",
                   cur_bme280_data.temperature,
                   cur_bme280_data.humidity,
                   BME280_hPA(cur_bme280_data.pressure));

            sprintf(str_buf, "Temp    : %0.2f C", cur_bme280_data.temperature);
            Paint_DrawString_EN(0, 80, str_buf, &Font20, WHITE, BLACK);

            sprintf(str_buf, "Relh    : %0.2f %%", cur_bme280_data.humidity);
            Paint_DrawString_EN(0, 100, str_buf, &Font20, WHITE, BLACK);

            sprintf(str_buf, "Pressure: %0.2f hPa", BME280_hPA(cur_bme280_data.pressure));
            Paint_DrawString_EN(0, 120, str_buf, &Font20, WHITE, BLACK);

            EPD_2IN66_Display(BlackImage);
        }

        old_bme280_data = cur_bme280_data;
        old_datetime = cur_datetime;
    }
}

static void mqtt_publish_request_cb(void *arg, err_t result)
{
    printf("mqtt_publish_request_cb: result = %d\n", result);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    bool *is_connected_p = (bool *)arg;
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        printf("MQTT connected to %s\n", MQTT_SERVER_ADDR);
        *is_connected_p = true;
    }
    else
    {
        printf("MQTT connection failed\n");
    }
}

void network_task_fn(__unused void *params)
{
    if (cyw43_arch_init())
    {
        printf("failed to initialise\n");
        return;
    }
    cyw43_arch_enable_sta_mode();
    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("failed to connect.\n");
        exit(1);
    }
    else
    {
        printf("Connected.\n");
    }

    uint8_t mac[6];
    cyw43_hal_get_mac(0, mac);
    char mac_str[12];
    snprintf(mac_str, 12, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // sntp_setservername(0, "0.pl.pool.ntp.org");
    // sntp_setservername(1, "1.pl.pool.ntp.org");
    // sntp_setservername(2, "2.pl.pool.ntp.org");
    // sntp_setservername(3, "3.pl.pool.ntp.org");
    // sntp_init();

    mqtt_client_t *mqtt_client = mqtt_client_new();
    if (!mqtt_client)
        goto err_mqtt_client;

    struct mqtt_connect_client_info_t mqtt_client_info;
    memset(&mqtt_client_info, 0, sizeof(mqtt_client_info));

    mqtt_client_info.client_id = "pico-w";
    mqtt_client_info.client_user = MQTT_USER;
    mqtt_client_info.client_pass = MQTT_PASS;

    ip_addr_t mqtt_server_addr;
    ip4_addr_set_u32(&mqtt_server_addr, ipaddr_addr(MQTT_SERVER_ADDR));

    bool is_connected = false;
    err_t err = mqtt_client_connect(
        mqtt_client,
        &mqtt_server_addr,
        MQTT_PORT,
        mqtt_connection_cb,
        &is_connected,
        &mqtt_client_info);

    if (err != ERR_OK)
    {
        printf("MQTT connection failed %d\n", err);
        goto err_mqtt_client;
    }

    struct bme280_data bme280_data;
    while (true)
    {
        if (is_connected)
        {
            xQueueReceive(xMqttQueue, &bme280_data, portMAX_DELAY);

            char str_buf[256];

            snprintf(str_buf, sizeof(str_buf), "%.2lf", bme280_data.temperature);
            err_t err = mqtt_publish(mqtt_client, "pico-w/temperature", str_buf, strlen(str_buf), 2, 0, mqtt_publish_request_cb, NULL);
            if (err != ERR_OK)
            {
                printf("mqtt_publish returned %d\n", err);
            }

            snprintf(str_buf, sizeof(str_buf), "%.2lf", bme280_data.humidity);
            err = mqtt_publish(mqtt_client, "pico-w/relative_humidity", str_buf, strlen(str_buf), 2, 0, mqtt_publish_request_cb, NULL);
            if (err != ERR_OK)
            {
                printf("mqtt_publish returned %d\n", err);
            }

            snprintf(str_buf, sizeof(str_buf), "%.2lf", bme280_data.pressure * 0.01);
            err = mqtt_publish(mqtt_client, "pico-w/pressure", str_buf, strlen(str_buf), 2, 0, mqtt_publish_request_cb, NULL);
            if (err != ERR_OK)
            {
                printf("mqtt_publish returned %d\n", err);
            }
        }
        else
        {
            vTaskDelay(100);
        }
    }

err_mqtt_client:
    cyw43_arch_deinit();
}

void vLaunch(void)
{
    xMqttQueue = xQueueCreate(MQTT_QUEUE_LENGTH, sizeof(struct bme280_data));
    if (!xMqttQueue)
    {
        printf("Creating MQTT queue failed\n");
        return;
    }

    xEpdQueue = xQueueCreate(MQTT_QUEUE_LENGTH, sizeof(struct bme280_data));
    if (!xEpdQueue)
    {
        printf("Creating EPD queue failed\n");
        goto err_epd_queue;
    }

#ifdef TASK_CORE_NETWORK
    TaskHandle_t network_task;
    xTaskCreate(network_task_fn, "network_task", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY, &network_task);
#endif

#ifdef TASK_CORE_EPD
    TaskHandle_t epd_task;
    xTaskCreate(epd_task_fn, "epd_task", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY, &epd_task);
#endif

#ifdef TASK_CORE_DEV
    TaskHandle_t dev_task;
#if DEV_TASK == BME280_TASK
    xTaskCreate(bme280_dev_task_fn, "bme280_dev_task", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY, &dev_task);
#elif DEV_TASK == BME68X_TASK
    xTaskCreate(bme68x_dev_task_fn, "bme68x_dev_task", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY, &dev_task);
#elif DEV_TASK == BSEC_TASK
    xTaskCreate(bsec_dev_task_fn, "bsec_dev_task", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY, &dev_task);

#endif
#endif

#if NO_SYS && configUSE_CORE_AFFINITY && configNUM_CORES > 1
    // we must bind the main task to one core (well at least while the init is called)
    // (note we only do this in NO_SYS mode, because cyw43_arch_freertos
    // takes care of it otherwise)
#ifdef TASK_CORE_NETWORK
    vTaskCoreAffinitySet(network_task, TASK_CORE_NETWORK);
#endif
#ifdef TASK_CORE_EPD
    vTaskCoreAffinitySet(&epd_task, TASK_CORE_EPD);
#endif
#ifdef TASK_CORE_DEV
    vTaskCoreAffinitySet(&dev_task, TASK_CORE_DEV);
#endif
#endif

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
err_epd_queue:
    // TODO remove queue
    return;
}

void _hardfault_handler()
{
    // printf("!!! hardfault !!!\n");
}

int main(void)
{
    stdio_init_all();

    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, _hardfault_handler);
    rtc_init();

    /* Configure the hardware ready to run the demo. */
    const char *rtos_name;
#if (portSUPPORT_SMP == 1)
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

#if (portSUPPORT_SMP == 1) && (configNUM_CORES == 2)
    printf("Starting %s on both cores:\n", rtos_name);
    vLaunch();
#elif (RUN_FREE_RTOS_ON_CORE == 1)
    printf("Starting %s on core 1:\n", rtos_name);
    multicore_launch_core1(vLaunch);
    while (true)
        ;
#else
    printf("Starting %s on core 0:\n", rtos_name);
    vLaunch();
#endif
    return 0;
}
