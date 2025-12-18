#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define UART_PORT       UART_NUM_2
#define TXD_PIN         GPIO_NUM_17
#define RXD_PIN         GPIO_NUM_16
#define BUF_SIZE        256

/* RGB LED pins */
#define LED_R           GPIO_NUM_21
#define LED_G           GPIO_NUM_22
#define LED_B           GPIO_NUM_23

static const char *TAG = "RS485";

/* Queue handle */
static QueueHandle_t rs485_evt_queue;

/* RS485 event type */
typedef enum {
    RS485_IDLE = 0,
    RS485_TX_DONE
} rs485_event_t;

/* ---------------- RGB LED Helpers ---------------- */
static void rgb_off(void)
{
    gpio_set_level(LED_R, 0);
    gpio_set_level(LED_G, 0);
    gpio_set_level(LED_B, 0);
}

static void rgb_red(void)
{
    rgb_off();
    gpio_set_level(LED_R, 1);
}

static void rgb_green(void)
{
    rgb_off();
    gpio_set_level(LED_G, 1);
}

/* ---------------- RS485 Task ---------------- */
void rs485_task(void *arg)
{
    const char *msg = "Hello RS485\r\n";
    rs485_event_t evt;

    while (1)
    {
        uart_write_bytes(UART_PORT, msg, strlen(msg));
        uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));

        ESP_LOGI(TAG, "Sent: %s", msg);

        /* Notify LED task */
        evt = RS485_TX_DONE;
        xQueueSend(rs485_evt_queue, &evt, 0);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---------------- RGB LED Task ---------------- */
void rgb_led_task(void *arg)
{
    rs485_event_t evt;

    /* Default state */
    rgb_red();

    while (1)
    {
        if (xQueueReceive(rs485_evt_queue, &evt, portMAX_DELAY))
        {
            switch (evt)
            {
                case RS485_TX_DONE:
                    rgb_green();                      // TX active
                    vTaskDelay(pdMS_TO_TICKS(200));   // short blink
                    rgb_red();                        // back to idle
                    break;

                default:
                    rgb_red();
                    break;
            }
        }
    }
}

/* ---------------- app_main ---------------- */
void app_main(void)
{
    /* GPIO config for RGB LED */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_R) |
                        (1ULL << LED_G) |
                        (1ULL << LED_B),
    };
    gpio_config(&io_conf);

    rgb_off();

    /* UART config */
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* Optional for true RS485 hardware mode */
    // uart_set_mode(UART_PORT, UART_MODE_RS485_HALF_DUPLEX);

    uart_driver_install(UART_PORT, BUF_SIZE, BUF_SIZE, 0, NULL, 0);

    ESP_LOGI(TAG, "RS485 + RGB LED FreeRTOS Ready");

    /* Create queue */
    rs485_evt_queue = xQueueCreate(5, sizeof(rs485_event_t));

    /* Create tasks */
    xTaskCreate(rs485_task, "rs485_task", 4096, NULL, 10, NULL);
    xTaskCreate(rgb_led_task, "rgb_led_task", 2048, NULL, 9, NULL);
}
