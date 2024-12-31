#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_mac.h"

#include "smartconfig.h"

static const char *SMARTCONFIG_TAG = "smartconfig";

static void smartconfig_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data)
{
  if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
  {
    ESP_LOGI(SMARTCONFIG_TAG, "Scan done");
  }
  else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
  {
    ESP_LOGI(SMARTCONFIG_TAG, "Found channel");
  }
  else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
  {
    ESP_LOGI(SMARTCONFIG_TAG, "Got SSID and password");

    smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
    wifi_config_t wifi_config;
    uint8_t ssid[33] = {0};
    uint8_t password[65] = {0};
    uint8_t rvd_data[128] = {0};

    memset(&wifi_config, 0, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
    memcpy(ssid, evt->ssid, sizeof(evt->ssid));
    memcpy(password, evt->password, sizeof(evt->password));
    ESP_LOGI(SMARTCONFIG_TAG, "SSID:%s", ssid);
    ESP_LOGI(SMARTCONFIG_TAG, "PASSWORD:%s", password);
    if (evt->type == SC_TYPE_ESPTOUCH_V2)
    {
      ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
      ESP_LOGI(SMARTCONFIG_TAG, "RVD_DATA:%s", rvd_data);
      // rvd_data is excepted to be a mqtt broker uri
      nvs_handle_t nvs_handle;
      ESP_ERROR_CHECK(nvs_open("wiguard", NVS_READWRITE, &nvs_handle));
      ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "broker_uri", (char *)rvd_data));
      ESP_ERROR_CHECK(nvs_commit(nvs_handle));
      nvs_close(nvs_handle);
    }

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();
  }
  else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
  {
    ESP_LOGI(SMARTCONFIG_TAG, "smartconfig over");
    esp_smartconfig_stop();
  }
}

void smartconfig_init()
{
  ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler, NULL));
}

void smartconfig_start()
{
  ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2));
  smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
}