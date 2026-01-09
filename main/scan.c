/* Scan Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
    This example shows how to scan for available set of APs.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "regex.h"
#include "protobuf/pb_encode.h"
#include "protobuf/pb_decode.h"
#include "protobuf/pb_common.h"
#include "protobuf/uart.pb.h"
#include <arpa/inet.h>

#define DEFAULT_SCAN_LIST_SIZE CONFIG_EXAMPLE_SCAN_LIST_SIZE

#ifdef CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP
#define USE_CHANNEL_BITMAP 1
#define CHANNEL_LIST_SIZE 3
static uint8_t channel_list[CHANNEL_LIST_SIZE] = {1, 6, 11};
#endif /*CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP*/                                                                                   

#define UART_NUM UART_NUM_2
#define TXD_PIN 17
#define RXD_PIN 16
#define BAUDRATE 115200
#define BUF_SIZE 1024


static const char *TAG = "scan";

QueueHandle_t _q_scanned_aps;
QueueHandle_t _q_processed_aps;
QueueHandle_t uart_queue;

typedef struct {
    uint8_t mac[6];
    uint8_t ssid[33];
    uint8_t channel;
    int8_t rssi;
    wifi_auth_mode_t authmode;
    wifi_cipher_type_t pairwise_cipher;
    wifi_cipher_type_t groupwise_cipher;
    char country[3];
} processed_ap_t;

int init_uart(void)
{

    esp_err_t err;

    // ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE*2, BUF_SIZE*2, 10, &uart_queue, 0));
    err = uart_driver_install(UART_NUM, BUF_SIZE*2, BUF_SIZE*2, 10, &uart_queue, 0);
    ESP_LOGI(TAG, "uart_driver_install: %s", esp_err_to_name(err));
    if (err != ESP_OK) return err;



    uart_config_t uart_config = {
        .baud_rate = BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = UART_HW_FLOWCTRL_DISABLE,
    };

    err = uart_param_config(UART_NUM, &uart_config);
    ESP_LOGI(TAG, "uart_param_config: %s", esp_err_to_name(err));
    if (err != ESP_OK) return err;

    err = uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI(TAG, "uart_set_pin: %s", esp_err_to_name(err));
    if (err != ESP_OK) return err;

    // ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    // ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    return ESP_OK;
    // return 0;
};

void send_uart(void *pvParameters)
{
    processed_ap_t processed_aps; 
    size_t message_length;

    while(1)
    {
        if (xQueueReceive(_q_processed_aps, &processed_aps, portMAX_DELAY)){

            uint8_t buffer[256];
            bool status = false;
            uartMessage message = uartMessage_init_zero;
            
            // MAC is always 6 bytes (binary)
            message.mac.size = 6;
            memcpy(message.mac.bytes, processed_aps.mac, 6);

            // SSID is null-terminated string
            size_t ssid_len = strlen((char *)processed_aps.ssid);
            message.ssid.size = ssid_len;
            memcpy(message.ssid.bytes, processed_aps.ssid, ssid_len);

            // Country is 3 bytes but might not be null-terminated
            message.country.size = 3;
            memcpy(message.country.bytes, processed_aps.country, 3);

            message.channel = (uint32_t)processed_aps.channel;
            message.rssi = (uint32_t)processed_aps.rssi;
            message.authmode = (uint32_t)processed_aps.authmode;
            message.pairwise_cipher = (uint32_t)processed_aps.pairwise_cipher;
            message.groupwise_cipher = (uint32_t)processed_aps.groupwise_cipher;

            pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
            status = pb_encode(&stream, uartMessage_fields, &message);
            message_length = stream.bytes_written;
            ESP_LOGI(TAG, "MESSAGE LENGTH IS : %d", message_length);

            if(!status)
            {
                ESP_LOGI(TAG, "Encoding failed: %s", PB_GET_ERROR(&stream));
            }
            else
            {
                // Create frame with length header + message
                uint8_t frame[4 + message_length];
                uint32_t len = (uint32_t)message_length;
                
                // Copy length header into frame
                memcpy(frame, &len, 4);
                
                // Copy message data into frame starting at index 4
                memcpy(frame + 4, buffer, message_length);
                
                // Check yo self
                ESP_LOGI(TAG, "Length bytes: %02x %02x %02x %02x", 
                         ((uint8_t*)&len)[0], ((uint8_t*)&len)[1], 
                         ((uint8_t*)&len)[2], ((uint8_t*)&len)[3]);
                
                // Write all da bytes
                int bytes_sent = uart_write_bytes(UART_NUM, (const char *)frame, 4 + message_length);
                
                if (bytes_sent == 4 + message_length) {
                    ESP_LOGI(TAG, "Sent %d bytes successfully ", bytes_sent);
                } else {
                    ESP_LOGI(TAG, "Failed to send all bytes. Sent %d of %d", bytes_sent, 4 + message_length);
                }
                
            }
        }    
    }
}

void proccess_aps(void *pvParameters)
{

    wifi_ap_record_t raw_ap;
    processed_ap_t processed_aps;

    while(1){
        if(xQueueReceive(_q_scanned_aps, &raw_ap, portMAX_DELAY )){
            memcpy(processed_aps.mac, raw_ap.bssid, 6 );
            memcpy(processed_aps.ssid, raw_ap.ssid, 33);
            processed_aps.channel = raw_ap.primary;
            processed_aps.rssi = raw_ap.rssi;
            processed_aps.authmode = raw_ap.authmode;
            processed_aps.pairwise_cipher = raw_ap.pairwise_cipher;
            processed_aps.groupwise_cipher = raw_ap.group_cipher;
            memcpy(processed_aps.country, raw_ap.country.cc,3);
            if (xQueueSend(_q_processed_aps, &processed_aps, portMAX_DELAY) != 1)
            {
                ESP_LOGI(TAG, "THE QUEUE IS FULL, SKIPPING");
            }
            memset(&processed_aps, 0, sizeof(processed_ap_t));
        }

    }
}


#ifdef USE_CHANNEL_BITMAP
static void array_2_channel_bitmap(const uint8_t channel_list[], const uint8_t channel_list_size, wifi_scan_config_t *scan_config) {

    for(uint8_t i = 0; i < channel_list_size; i++) {
        uint8_t channel = channel_list[i];
        scan_config->channel_bitmap.ghz_2_channels |= (1 << channel);
    }
}
#endif /*USE_CHANNEL_BITMAP*/


/* Initialize Wi-Fi as sta and set scan method */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));




    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());


}

static void wifi_scan(void)
{

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];

    memset(ap_info, 0, sizeof(ap_info));
    esp_wifi_scan_start(NULL, true);


    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++) {
        if (!xQueueSend(_q_scanned_aps, &ap_info[i], portMAX_DELAY)){
            ESP_LOGI(TAG,"The queue is full, gonna keep trying");
        }
        else{
            ESP_LOGI(TAG, "Sent AP Info: %.32s", (char *)ap_info[i].ssid);

        }
    }
}

void app_main(void)
{
    // Initialize UART
    if(init_uart() != ESP_OK){
        ESP_LOGI(TAG, "UART Init Failed");
        return;
    }
    else{
        ESP_LOGI(TAG, "UART Initialized");

    }

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "NVS FLASH Init failed");

        return;
    }
    _q_scanned_aps = xQueueCreate(10, sizeof(wifi_ap_record_t));
    _q_processed_aps = xQueueCreate(10, sizeof(processed_ap_t));

    ESP_LOGI(TAG, "Starting Tasks");
    xTaskCreate(&proccess_aps, "Process APs", 4096, NULL, 5, NULL);
    xTaskCreate(&send_uart, "Send UART", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG,"Tasks Started");

    wifi_init();
    while(1){
        wifi_scan();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
    // Array to hold AP information
    // ==============================================================
    // uint8_t bssid[6]                     ->      MAC Address
    // uint8_t ssid[33]                     ->      SSID
    // uint8_t primary                      ->      Channel
    // int8_t rss                           ->      Strength
    // wifi_auth_mode_t authmode            ->      Authmode 
    // wifi_cipher_type_t pairwise_cipher   ->      Pairwise cypher
    // wifi_cipher_type_t group_cipher      ->      Groupwise Cypher




