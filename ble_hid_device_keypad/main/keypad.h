#ifndef KEYPAD_H
#define KEYPAD_H

#include <driver/gpio.h>
// #include <freertos/queue.h>

#define KEYPAD_DEBOUNCING 100   ///< time in ms
#define KEYPAD_STACKSIZE  10





/**
 * @brief Initialize Keypad settings and start it, setup up directions and isr and initialize
 * key pressed queue.
 * 
 * @param keypad_pins Keypad Connections Array following this template: 
 *  {R1, R2, R3, R4, C1, C2, C3, C4}
 * 
 * @return esp_err_t returns ESP_OK if succeful initialize
 */
esp_err_t keypad_initalize(gpio_num_t keypad_pins[8],int8_t keypad[16]);

/**
 * @brief Returns pressed key on keypad
 * 
 * @return Returns an pressed key on keypad, if key wasn't pressed returns an terminator character. 
 */
char keypad_getkey();

/**
 * @brief Delete Keyboard and free resources
 * 
 */
void keypad_delete(void);

#endif