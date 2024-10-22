#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>

#include "mqtt.h"

static const char *MQTT_TAG = "mqtts_example";

esp_mqtt_client_handle_t client;
EventGroupHandle_t s_mqtt_event_group;

const int MQTT_CONNECTED_BIT = BIT0;

/*
 * @brief Send CSI data to the MQTT broker
 */
void send_csi(esp_mqtt_client_handle_t client, const char *csi_data)
{
  esp_mqtt_client_publish(client, "wiguard/csi", csi_data, 0, 0, 0);
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  switch ((esp_mqtt_event_id_t)event_id)
  {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
    xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
    xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
    {
      ESP_LOGI(MQTT_TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
      ESP_LOGI(MQTT_TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
      ESP_LOGI(MQTT_TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
               strerror(event->error_handle->esp_transport_sock_errno));
    }
    else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
    {
      ESP_LOGI(MQTT_TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    }
    else
    {
      ESP_LOGW(MQTT_TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }
    break;
  default:
    ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
    break;
  }
}

void mqtt_app_start(void)
{
  s_mqtt_event_group = xEventGroupCreate();

  nvs_handle_t nvs_handle;
  char broker_uri[128];
  size_t broker_uri_len = sizeof(broker_uri);
  ESP_ERROR_CHECK(nvs_open("wiguard", NVS_READONLY, &nvs_handle));
  ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "broker_uri", broker_uri, &broker_uri_len));

  const esp_mqtt_client_config_t mqtt_cfg = {
      .broker = {
          .address.uri = broker_uri,
      },
      // WARN: ca certificate not validated
  };

  ESP_LOGI(MQTT_TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
  client = esp_mqtt_client_init(&mqtt_cfg);
  /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  esp_mqtt_client_start(client);
}