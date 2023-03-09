// Copyright 2017-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>

#include "driver/gpio.h"
#include "board.h"
#include "ble_mesh_fast_prov_common.h"

#define TAG "BOARD"

struct _led_state led_state = {
    LED_OFF, LED_OFF, 2, "led_2"
};

void board_output_number(esp_ble_mesh_output_action_t action, uint32_t number)
{
    ESP_LOGI(TAG, "Board output number %d", number);
}

void board_prov_complete(void)
{
    board_led_operation(2, LED_OFF);
}

void board_led_operation(uint8_t pin, uint8_t onoff)
{
        if (onoff == led_state.previous) {
            ESP_LOGW(TAG, "led %s is already %s",
                     led_state.name, (onoff ? "on" : "off"));
            return;
        }
        esp_err_t gpio_err = gpio_set_level(GPIO_NUM_2, onoff);
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

esp_err_t board_init(void)
{
    board_led_init();
    return ESP_OK;
}
