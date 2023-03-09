#include "keypad.h"

#include <memory.h>
#include <esp_log.h>
#include "esp_timer.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

/** \brief Keypad mapping array*/
static int8_t _keypad[16] = {0};
/** \brief Keypad configuration pions*/
static gpio_num_t _keypad_pins[8] = {0};

/** \brief Last isr time*/
int64_t time_old_isr = 0;

/** \brief Pressed keys queue*/

extern QueueHandle_t keypad_queue;

QueueHandle_t button_queue = NULL;

/**
 * @brief Handle keypad click
 * @param [in]args row number
 */
void intr_click_handler(void *args);

void keypad_execute(void *arg);

/**
 * @brief Enable rows'pin pullup resistor, and isr. Prepares
 * keypad to read pressed row number.
 */
void turnon_rows()
{
    for (int i = 4; i < 8; i++) /// Columns
    {
        gpio_set_pull_mode(_keypad_pins[i], GPIO_PULLDOWN_ONLY);
    }
    for (int i = 0; i < 4; i++) /// Rows
    {
        gpio_set_pull_mode(_keypad_pins[i], GPIO_PULLUP_ONLY);
        gpio_intr_enable(_keypad_pins[i]);
    }
}

/**
 * @brief Enable columns'pin pullup resistor, and disable rows isr and pullup resistor.
 * Prepares keypad to read pressed column number.
 */
void turnon_cols()
{
    for (int i = 0; i < 4; i++) /// Rows
    {
        gpio_intr_disable(_keypad_pins[i]);
        gpio_set_pull_mode(_keypad_pins[i], GPIO_PULLDOWN_ONLY);
    }
    for (int i = 4; i < 8; i++) /// Columns
    {
        gpio_set_pull_mode(_keypad_pins[i], GPIO_PULLUP_ONLY);
    }
}

esp_err_t keypad_initalize(gpio_num_t keypad_pins[8], int8_t keypad[16])
{
    memcpy(_keypad_pins, keypad_pins, 8 * sizeof(gpio_num_t));
    memcpy(_keypad, keypad, 16 * sizeof(int8_t));

    // for (int i = 0;i < 16;i++) {
    //     _keypad[i] = keypad[i];
    // }
    /** Maybe cause issues if try to desinstall this flag because it's global allocated
     * to all pins try to use gpio_isr_register instrad of gpio_install_isr_service **/
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_install_isr_service(0));
    for (int i = 0; i < 4; i++) /// Rows
    {
        gpio_intr_disable(keypad_pins[i]);
        gpio_set_direction(keypad_pins[i], GPIO_MODE_INPUT);
        gpio_set_intr_type(keypad_pins[i], GPIO_INTR_NEGEDGE);
        ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_isr_handler_add(_keypad_pins[i], (void *)intr_click_handler, (void *)i));
    }
    for (int i = 4; i < 8; i++)
    {
        gpio_set_direction(keypad_pins[i], GPIO_MODE_INPUT);
    }

    keypad_queue = xQueueCreate(5, sizeof(int8_t));
    if (keypad_queue == NULL)
        return ESP_ERR_NO_MEM;

    turnon_rows();

    button_queue = xQueueCreate(15, sizeof(int));
    xTaskCreate(keypad_execute, "keypad_execute", 2048, NULL, 3, NULL);

    return ESP_OK;
}

void intr_click_handler(void *args)
{
    int index = (int)(args);

    int64_t time_now_isr = esp_timer_get_time() / 1000;
    int64_t time_isr = (time_now_isr - time_old_isr);

    if (time_isr >= KEYPAD_DEBOUNCING)
    {
        xQueueSend(button_queue, (void *)&index, (TickType_t)0);
    }

    time_old_isr = time_now_isr;
}

void keypad_execute(void *arg)
{
    int pin;
    while (1)
    {
        if (xQueueReceive(button_queue, &pin, (TickType_t)5) == pdTRUE)
        {
            ESP_LOGI("ROW", "%d", _keypad_pins[pin]);
            turnon_cols();
            for (int j = 4; j < 8; j++)
            {
                if (!gpio_get_level(_keypad_pins[j]))
                {
                    xQueueSendFromISR(keypad_queue, &_keypad[pin * 4 + j - 4], NULL);
                    ESP_LOGI("COL", "%d", _keypad_pins[j]);
                    break;
                }
            }
            turnon_rows();
        }
    }
}

char keypad_getkey()
{
    char key;
    if (!uxQueueMessagesWaiting(keypad_queue)) /// if is empty, return teminator character
        return '\0';
    xQueueReceive(keypad_queue, &key, portMAX_DELAY);
    return key;
}

void keypad_delete()
{
    for (int i = 0; i < 8; i++)
    {
        gpio_isr_handler_remove(_keypad_pins[i]);
        gpio_set_direction(_keypad_pins[i], GPIO_MODE_DISABLE);
    }
    vQueueDelete(keypad_queue);
}