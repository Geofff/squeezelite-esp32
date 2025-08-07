#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"

#include "card_reader.h"
#include "nvs_utilities.h"

static const char *TAG = "card_reader";

#define CARD_READER_SERVER_URL_NVS_KEY "card_reader_url"
#define DEFAULT_CARD_READER_SERVER_URL "http://your-server-endpoint/api/card"

// Mocked card reader state
static int card_present_count = 0;
static const char* MOCKED_UID = "0xDECAFBAD";

// Forward declarations
static void card_reader_task(void *pvParameters);
static void send_uid_post_request(const char* uid);
esp_err_t _http_event_handler(esp_http_client_event_t *evt);

void card_reader_init(void) {
    ESP_LOGI(TAG, "Initializing card reader");
    xTaskCreate(card_reader_task, "card_reader_task", 4096, NULL, 5, NULL);
}

static void card_reader_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds

        // Mock behavior: "detect" a card every 3 checks
        if (++card_present_count >= 3) {
            card_present_count = 0;
            ESP_LOGI(TAG, "Card detected, UID: %s", MOCKED_UID);
            send_uid_post_request(MOCKED_UID);
        } else {
            ESP_LOGI(TAG, "No card detected");
        }
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Log the response data
                ESP_LOGI(TAG, "Response: %.*s", evt->data_len, (char*)evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

static void send_uid_post_request(const char* uid) {
    char* server_url = config_alloc_get_str(CARD_READER_SERVER_URL_NVS_KEY, DEFAULT_CARD_READER_SERVER_URL, NULL);
    if (server_url == NULL) {
        ESP_LOGE(TAG, "Failed to get server URL");
        return;
    }

    ESP_LOGI(TAG, "Sending UID to server: %s", server_url);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "uid", uid);
    const char *post_data = cJSON_Print(root);

    esp_http_client_config_t config = {
        .url = server_url,
        .event_handler = _http_event_handler,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free((void*)post_data);
    free(server_url);
}
