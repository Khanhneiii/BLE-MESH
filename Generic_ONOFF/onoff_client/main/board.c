/* board.c - Board-specific hooks */

/*
 * Copyright (c) 2017 Intel Corporation
 * Additional Copyright (c) 2018 Espressif Systems (Shanghai) PTE LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"

#include "iot_button.h"
#include "board.h"

#define TAG "BOARD"

#define BUTTON_IO_NUM           0
#define BUTTON_ACTIVE_LEVEL     0

extern void example_ble_mesh_send_gen_onoff_set(void);

struct _led_state led_state = {
    LED_OFF, LED_OFF, 2, "led_2"
};


void board_led_operation(uint8_t pin, uint8_t onoff)
{
        if (onoff == led_state.previous) {
            ESP_LOGW(TAG, "led %s is already %s",
                     led_state.name, (onoff ? "on" : "off"));
            return;
        }
        gpio_set_level(pin, onoff);
        led_state.previous = onoff;
        return;
    ESP_LOGE(TAG, "LED is not found!");
}

static void board_led_init(void)
{
        gpio_reset_pin(led_state.pin);
        gpio_set_direction(led_state.pin, GPIO_MODE_OUTPUT);
        gpio_set_level(led_state.pin, LED_OFF);
        led_state.previous = LED_OFF;
}

static void button_tap_cb(void* arg)
{
    ESP_LOGI(TAG, "tap cb (%s)", (char *)arg);

    example_ble_mesh_send_gen_onoff_set();
}

static void board_button_init(void)
{
    button_handle_t btn_handle = iot_button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, button_tap_cb, "RELEASE");
    }
}

void board_init(void)
{
    board_led_init();
    board_button_init();
}
