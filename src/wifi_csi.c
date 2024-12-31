#include <esp_wifi_types.h>
#include <esp_netif_types.h>
#include <ping/ping_sock.h>

#include "rom/ets_sys.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_now.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"

#include "wifi_csi.h"
#include "mqtt.h"

// The frequency of sending ping packets to the router
#define CONFIG_PING_FREQUENCY 20
// The size of the buffer for storing CSI data
// Only send the data when the buffer is full
#define CONFIG_CSI_BUFFER_SIZE 2048

static const char *WIFI_CSI_TAG = "wifi_csi";
static char csi_buffer[CONFIG_CSI_BUFFER_SIZE];
static int csi_buffer_len = 0;

// Callback function to receive CSI data.
void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
  if (!info || !info->buf)
  {
    ESP_LOGW(WIFI_CSI_TAG, "<%s> wifi_csi_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
    return;
  }

  static int s_count = 0;
  const wifi_pkt_rx_ctrl_t *rx_ctrl = &info->rx_ctrl;

  // Only LLTF sub-carriers are selected.
  info->len = 128;

  char csi[1024];
  size_t csi_len = 0;

  csi_len += snprintf(csi + csi_len, sizeof(csi) - csi_len, "CSI_DATA,%d," MACSTR ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                      s_count++, MAC2STR(info->mac), rx_ctrl->rssi, rx_ctrl->rate, rx_ctrl->sig_mode,
                      rx_ctrl->mcs, rx_ctrl->cwb, rx_ctrl->smoothing, rx_ctrl->not_sounding,
                      rx_ctrl->aggregation, rx_ctrl->stbc, rx_ctrl->fec_coding, rx_ctrl->sgi,
                      rx_ctrl->noise_floor, rx_ctrl->ampdu_cnt, rx_ctrl->channel, rx_ctrl->secondary_channel,
                      rx_ctrl->timestamp, rx_ctrl->ant, rx_ctrl->sig_len, rx_ctrl->rx_state);

  csi_len += snprintf(csi + csi_len, sizeof(csi) - csi_len, ",%d,%d,\"[%d", info->len, info->first_word_invalid, info->buf[0]);
  for (int i = 1; i < info->len; i++)
  {
    csi_len += snprintf(csi + csi_len, sizeof(csi) - csi_len, ",%d", info->buf[i]);
  }
  csi_len += snprintf(csi + csi_len, sizeof(csi) - csi_len, "]\"\n");

  if (csi_buffer_len + csi_len < sizeof(csi_buffer))
  {
    strcat(csi_buffer, csi);
    csi_buffer_len += csi_len;
  }
  else
  {
    send_csi(client, csi_buffer);
    strcpy(csi_buffer, csi);
    csi_buffer_len = csi_len;
  }
}

esp_ping_handle_t ping_handle = NULL;

// Initialize CSI data reception.
void wifi_csi_init()
{
  // In order to ensure the compatibility of routers, only LLTF sub-carriers are selected.
  wifi_csi_config_t csi_config = {
      .lltf_en = true,
      .htltf_en = false,
      .stbc_htltf2_en = false,
      .ltf_merge_en = true,
      .channel_filter_en = true,
      .manu_scale = true,
      .shift = true,
  };

  wifi_ap_record_t s_ap_info = {0};
  ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&s_ap_info));
  ESP_LOGI(WIFI_CSI_TAG, "got bssid: " MACSTR, MAC2STR(s_ap_info.bssid));
  ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
  ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, s_ap_info.bssid));
  ESP_ERROR_CHECK(esp_wifi_set_csi(true));

  // Initialize the ping configuration.
  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
  ping_config.count = 0;
  ping_config.interval_ms = 1000 / CONFIG_PING_FREQUENCY;
  ping_config.task_stack_size = 3072;
  ping_config.data_size = 1;

  esp_netif_ip_info_t local_ip;
  esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &local_ip);
  ESP_LOGI(WIFI_CSI_TAG, "got ip:" IPSTR ", gw: " IPSTR, IP2STR(&local_ip.ip), IP2STR(&local_ip.gw));
  ping_config.target_addr.u_addr.ip4.addr = ip4_addr_get_u32(&local_ip.gw);
  ping_config.target_addr.type = ESP_IPADDR_TYPE_V4;

  esp_ping_callbacks_t cbs = {0};
  esp_ping_new_session(&ping_config, &cbs, &ping_handle);
}

esp_err_t wifi_csi_start()
{
  return esp_ping_start(ping_handle);
}

esp_err_t wifi_csi_stop()
{
  return esp_ping_stop(ping_handle);
}