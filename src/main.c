#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_system.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"

#include "smartconfig.h"
#include "wifi_csi.h"
#include "mqtt.h"

#define BOOT_BUTTON_PIN GPIO_NUM_0

void IRAM_ATTR gpio_isr_handler(void *arg)
{
  wifi_init();
  esp_restart();
}

void init()
{
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void app_main()
{

  nvs_handle_t nvs_handle;
  char broker_uri[128];
  size_t broker_uri_len = sizeof(broker_uri);
  ESP_ERROR_CHECK(nvs_open("wiguard", NVS_READONLY, &nvs_handle));
  err = nvs_get_str(nvs_handle, "broker_uri", broker_uri, &broker_uri_len);
  if (err == ESP_ERR_NVS_NOT_FOUND)
  {
    wifi_init();
  }
  else
  {
  }

  esp_rom_gpio_pad_select_gpio(BOOT_BUTTON_PIN);
  gpio_set_direction(BOOT_BUTTON_PIN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BOOT_BUTTON_PIN, GPIO_PULLUP_ONLY);
  gpio_set_intr_type(BOOT_BUTTON_PIN, GPIO_INTR_NEGEDGE);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(BOOT_BUTTON_PIN, gpio_isr_handler, NULL);

  xEventGroupWaitBits(s_wifi_event_group, ESPTOUCH_DONE_BIT, false, true, portMAX_DELAY);
  mqtt_app_start();

  xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
  wifi_csi_init();
  wifi_ping_router_start();
}