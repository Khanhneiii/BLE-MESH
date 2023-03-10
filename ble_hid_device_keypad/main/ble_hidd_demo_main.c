/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_hidd_prf_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "driver/gpio.h"
#include "hid_dev.h"

#include "esp32_button.h"
#include "keypad.h"


#define A_BTN 5
#define W_BTN 18
#define S_BTN 19
#define D_BTN 21

/**
 * Brief:
 * This example Implemented BLE HID device profile related functions, in which the HID device
 * has 4 Reports (1 is mouse, 2 is keyboard and LED, 3 is Consumer Devices, 4 is Vendor devices).
 * Users can choose different reports according to their own application scenarios.
 * BLE HID profile inheritance and USB HID class.
 */

/**
 * Note:
 * 1. Win10 does not support vendor report , So SUPPORT_REPORT_VENDOR is always set to FALSE, it defines in hidd_le_prf_int.h
 * 2. Update connection parameters are not allowed during iPhone HID encryption, slave turns
 * off the ability to automatically update connection parameters during encryption.
 * 3. After our HID device is connected, the iPhones write 1 to the Report Characteristic Configuration Descriptor,
 * even if the HID encryption is not completed. This should actually be written 1 after the HID encryption is completed.
 * we modify the permissions of the Report Characteristic Configuration Descriptor to `ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE_ENCRYPTED`.
 * if you got `GATT_INSUF_ENCRYPTION` error, please ignore.
 */

#define HID_DEMO_TAG "HID_DEMO"



/*Create my button queue*/
QueueHandle_t ButtonQueue;

QueueHandle_t keypad_queue;

typedef enum {
    W,
    A,
    S,
    D,
} BTN;


gpio_num_t pins[8] = {13, 12, 14, 27, 26, 25, 33, 32};

int8_t keypad[] = {
    HID_KEY_1, HID_KEY_2, HID_KEY_3, HID_KEY_A,
    HID_KEY_4, HID_KEY_5, HID_KEY_6, HID_KEY_B,
    HID_KEY_7, HID_KEY_8, HID_KEY_9, HID_KEY_C,
    HID_KEY_MULTIPLY, HID_KEY_0, HID_KEY_3, HID_KEY_D
};

/*end */

uint16_t hid_conn_id = 0;
static bool sec_conn = false;
static bool send_volum_up = false;
#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);

#define HIDD_DEVICE_NAME            "HID"
static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

    // ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY  = 0x00,
    // ///Allow both scan req from White List devices only and connection req from anyone
    // ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY,
    // ///Allow both scan req from anyone and connection req from White List devices only
    // ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST,
    // ///Allow scan and connection requests from White List devices only
    // ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST,

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch(event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                //esp_bd_addr_t rand_addr = {0x04,0x11,0x11,0x11,0x11,0x05};
                esp_ble_gap_set_device_name(HIDD_DEVICE_NAME);
                esp_ble_gap_config_adv_data(&hidd_adv_data);

            }
            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH:
	     break;
		case ESP_HIDD_EVENT_BLE_CONNECT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
            hid_conn_id = param->connect.conn_id;
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            sec_conn = false;
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->vendor_write.data, param->vendor_write.length);
        }
        default:
            break;
    }
    return;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
     case ESP_GAP_BLE_SEC_REQ_EVT:
        for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
             ESP_LOGD(HID_DEMO_TAG, "%x:",param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
	 break;
     case ESP_GAP_BLE_AUTH_CMPL_EVT:
        sec_conn = true;
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(HID_DEMO_TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        for (int i = 0;i < 6;i++) {
            printf("%d ",bd_addr[0]);
        }
        ESP_LOGI(HID_DEMO_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(HID_DEMO_TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if(!param->ble_security.auth_cmpl.success) {
            ESP_LOGE(HID_DEMO_TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    default:
        break;
    }
}

void button_task(void *pvPara) {
    button_event_t ev;
    QueueHandle_t button_events = button_init(PIN_BIT(5) | PIN_BIT(18) | PIN_BIT(19)  | PIN_BIT(21)); 
    BTN btn;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    while (1) {
        if (xQueueReceive(button_events, &ev, 1000/portTICK_PERIOD_MS)) {
            if (ev.event == BUTTON_DOWN) {
                switch (ev.pin)
                {
                case 5:
                    btn = W;
                    break;
                case 18:
                    btn = A;
                    break;
                case 19:
                    btn = S;
                    break;
                case 21:
                    btn = D;
                    break;
                }
                xQueueSend(ButtonQueue,(void *)&btn,(TickType_t)0);
            }
        }
    }

}

void hid_demo_task(void *pvParameters)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    // BTN btn;
    uint8_t key_val = 0 ;
    keypad_initalize(pins,keypad);
    while(1) {
        // vTaskDelay(50 / portTICK_PERIOD_MS);445687c999
        if (xQueueReceive(keypad_queue,&key_val,(TickType_t)5)) {
            ESP_LOGI("KEYPAD","Key: %d",key_val);
            // switch (btn)
            // {
            // case W:
            //     key_val = HID_KEY_W;
            //     break;
            // case A:
            //     key_val = HID_KEY_A;
            //     break;
            // case S:
            //     key_val = HID_KEY_S;
            //     break;
            // case D:
            //     key_val = HID_KEY_D;
            //     break;
            // }
                // 123456789*03abcd3693258112312222223333444455678903*abcd

            esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_val, 1);
            vTaskDelay(50/portTICK_PERIOD_MS);
            key_val = HID_KEY_RESERVED;
            esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_val, 1);
            vTaskDelay(50/portTICK_PERIOD_MS);
        }
    }
}

// void IRAM_ATTR isr_handler(void *arg) {
//     // ESP_LOGI("ISR","Interrupt");
//     int pin = (int)arg;
//     BTN btn;
//     switch (pin)
//     {
//     case 5:
//         btn = A;
//         break;
//     case 18:
//         btn = W;
//         break;
//     case 19:
//         btn = S;
//         break;
//     case 21:
//         btn = D;
//         break;
//     }
//     while(gpio_get_level(pin) == 0);
//     xQueueSend(ButtonQueue,(void *)&btn,(TickType_t)0);
// }


void app_main(void)
{
    esp_err_t ret;

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s initialize controller failed\n", __func__);
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable controller failed\n", __func__);
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);
        return;
    }

    if((ret = esp_hidd_profile_init()) != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed\n", __func__);
    }

    ///register the callback function to the gap module
    esp_ble_gap_register_callback(gap_event_handler);
    esp_hidd_register_callbacks(hidd_event_callback);

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    // gpio_config_t io_conf = {
    //     .pin_bit_mask = (1ULL<<A_BTN) | (1ULL<<W_BTN) | (1ULL<<S_BTN) | (1ULL<<D_BTN),
    //     .mode = GPIO_MODE_INPUT,
    //     .pull_up_en = GPIO_PULLUP_ENABLE,
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //     .intr_type = GPIO_INTR_NEGEDGE,
    // };

    ButtonQueue = xQueueCreate(10,sizeof(BTN));
    // gpio_config(&io_conf);
    // gpio_set_intr_type(A_BTN,GPIO_INTR_NEGEDGE);
    // gpio_set_intr_type(W_BTN,GPIO_INTR_NEGEDGE);
    // gpio_set_intr_type(S_BTN,GPIO_INTR_NEGEDGE);
    // gpio_set_intr_type(D_BTN,GPIO_INTR_NEGEDGE);
    // gpio_install_isr_service(0);
    // gpio_isr_handler_add(A_BTN,isr_handler,(void *)5);
    // gpio_isr_handler_add(W_BTN,isr_handler,(void *)18);
    // gpio_isr_handler_add(S_BTN,isr_handler,(void *)19);
    // gpio_isr_handler_add(D_BTN,isr_handler,(void *)21);
    xTaskCreate(&hid_demo_task, "hid_task", 2048, NULL, 5, NULL);

    // xTaskCreate(&button_task,"button_task",2048,NULL,5,NULL);
    
}
