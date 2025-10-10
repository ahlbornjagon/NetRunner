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
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "regex.h"
#include "protobuf/pb_encode.h"
#include "protobuf/pb_decode.h"
#include "protobuf/pb_common.h"
#include "protobuf/uart.pb.h"

#define DEFAULT_SCAN_LIST_SIZE CONFIG_EXAMPLE_SCAN_LIST_SIZE

#ifdef CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP
#define USE_CHANNEL_BITMAP 1
#define CHANNEL_LIST_SIZE 3
static uint8_t channel_list[CHANNEL_LIST_SIZE] = {1, 6, 11};
#endif /*CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP*/

static const char *TAG = "scan";

QueueHandle_t _q_scanned_aps;
QueueHandle_t _q_processed_aps;

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

// static void print_auth_mode(int authmode)
// {
//     switch (authmode) {
//     case WIFI_AUTH_OPEN:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
//         break;
//     case WIFI_AUTH_OWE:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
//         break;
//     case WIFI_AUTH_WEP:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
//         break;
//     case WIFI_AUTH_WPA_PSK:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
//         break;
//     case WIFI_AUTH_WPA2_PSK:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
//         break;
//     case WIFI_AUTH_WPA_WPA2_PSK:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
//         break;
//     case WIFI_AUTH_ENTERPRISE:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
//         break;
//     case WIFI_AUTH_WPA3_PSK:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
//         break;
//     case WIFI_AUTH_WPA2_WPA3_PSK:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
//         break;
//     case WIFI_AUTH_WPA3_ENTERPRISE:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENTERPRISE");
//         break;
//     case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_ENTERPRISE");
//         break;
//     case WIFI_AUTH_WPA3_ENT_192:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
//         break;
//     default:
//         ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
//         break;
//     }
// }

// static void print_cipher_type(int pairwise_cipher, int group_cipher)
// {
//     switch (pairwise_cipher) {
//     case WIFI_CIPHER_TYPE_NONE:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
//         break;
//     case WIFI_CIPHER_TYPE_WEP40:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
//         break;
//     case WIFI_CIPHER_TYPE_WEP104:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
//         break;
//     case WIFI_CIPHER_TYPE_TKIP:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
//         break;
//     case WIFI_CIPHER_TYPE_CCMP:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
//         break;
//     case WIFI_CIPHER_TYPE_TKIP_CCMP:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
//         break;
//     case WIFI_CIPHER_TYPE_AES_CMAC128:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
//         break;
//     case WIFI_CIPHER_TYPE_SMS4:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
//         break;
//     case WIFI_CIPHER_TYPE_GCMP:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
//         break;
//     case WIFI_CIPHER_TYPE_GCMP256:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
//         break;
//     default:
//         ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
//         break;
//     }

//     switch (group_cipher) {
//     case WIFI_CIPHER_TYPE_NONE:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
//         break;
//     case WIFI_CIPHER_TYPE_WEP40:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
//         break;
//     case WIFI_CIPHER_TYPE_WEP104:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
//         break;
//     case WIFI_CIPHER_TYPE_TKIP:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
//         break;
//     case WIFI_CIPHER_TYPE_CCMP:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
//         break;
//     case WIFI_CIPHER_TYPE_TKIP_CCMP:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
//         break;
//     case WIFI_CIPHER_TYPE_SMS4:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
//         break;
//     case WIFI_CIPHER_TYPE_GCMP:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
//         break;
//     case WIFI_CIPHER_TYPE_GCMP256:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
//         break;
//     default:
//         ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
//         break;
//     }
// }

#ifdef USE_CHANNEL_BITMAP
static void array_2_channel_bitmap(const uint8_t channel_list[], const uint8_t channel_list_size, wifi_scan_config_t *scan_config) {

    for(uint8_t i = 0; i < channel_list_size; i++) {
        uint8_t channel = channel_list[i];
        scan_config->channel_bitmap.ghz_2_channels |= (1 << channel);
    }
}
#endif /*USE_CHANNEL_BITMAP*/


/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];

    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

#ifdef USE_CHANNEL_BITMAP
    wifi_scan_config_t *scan_config = (wifi_scan_config_t *)calloc(1,sizeof(wifi_scan_config_t));
    if (!scan_config) {
        ESP_LOGE(TAG, "Memory Allocation for scan config failed!");
        return;
    }
    array_2_channel_bitmap(channel_list, CHANNEL_LIST_SIZE, scan_config);
    esp_wifi_scan_start(scan_config, true);
    free(scan_config);

#else
    esp_wifi_scan_start(NULL, true);
#endif /*USE_CHANNEL_BITMAP*/

    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++) {
        if (!xQueueSend(_q_scanned_aps, &ap_info[i], portMAX_DELAY)){
            printf("The queue is full, gonna keep trying");
        };
    }
}

void app_start(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
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

void proccess_aps(void)
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
                printf("THE QUEUE IS FULL, SKIPPING");
            }
            memset(&processed_aps, 0, sizeof(processed_ap_t));
        }

    }
}

void send_uart(void)
{
    processed_ap_t processed_aps; 
    size_t message_length;


    while(1)
    {
        if (xQueueReceive(_q_processed_aps, &proccess_aps, portMAX_DELAY)){

        
            uint8_t buffer[256];
            bool status = false;
            uartMessage message = uartMessage_init_zero;
        
            strncpy(message.mac, (char*)processed_aps.mac, 6);

            strncpy(message.ssid, (char*)processed_aps.ssid, 33);

            message.channel = (uint32_t)processed_aps.channel;
            message.rssi = (uint32_t)processed_aps.rssi;
            message.authmode = (uint32_t)processed_aps.authmode;
            message.pairwise_cipher = (uint32_t)processed_aps.pairwise_cipher;
            message.groupwise_cipher = (uint32_t)processed_aps.groupwise_cipher;

            strncpy(message.country, (char*)processed_aps.country,3);

            pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
            message_length = stream.bytes_written;
            status = pb_encode(&stream, uartMessage_fields, &message);

            if(!status || message_length == 0)
            {
                printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
            }

            // Todo: Now we need to send this boi over the uart
            // Also need to fix the name of main entry, also kick off the freeRtos tasks

        }    
    }


}
