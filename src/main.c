#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"

#include "esp_mac.h"
#include "rom/ets_sys.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_now.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"

#include "cJSON.h"

#include "protocol_examples_common.h"

#define CONFIG_SEND_FREQUENCY 100
#define CONFIG_SERVER_IP "192.168.1.101"
#define CONFIG_SERVER_PORT 8080

static const char *TAG = "csi_recv_router";

int sockfd;
struct sockaddr_in servaddr;

static void init_socket()
{
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1)
  {
    ESP_LOGE(TAG, "Failed to create socket");
    return;
  }

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(CONFIG_SERVER_PORT);
  servaddr.sin_addr.s_addr = inet_addr(CONFIG_SERVER_IP);

  if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
  {
    ESP_LOGE(TAG, "Failed to connect to the server");
    close(sockfd);
    sockfd = -1;
    return;
  }
}

static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
  if (!info || !info->buf)
  {
    ESP_LOGW(TAG, "<%s> wifi_csi_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
    return;
  }

  // if (memcmp(info->mac, ctx, 6))
  // {
  //   return;
  // }

  static int s_count = 0;
  const wifi_pkt_rx_ctrl_t *rx_ctrl = &info->rx_ctrl;

  // if (!s_count)
  // {
  //   ESP_LOGI(TAG, "================ CSI RECV ================");
  //   ets_printf("type,seq,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,len,first_word,data\n");
  // }

  /** Only LLTF sub-carriers are selected. */
  info->len = 128;

  char data[1024];
  int offset = 0;

  offset += snprintf(data + offset, sizeof(data) - offset, "CSI_DATA,%d," MACSTR ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                     s_count++, MAC2STR(info->mac), rx_ctrl->rssi, rx_ctrl->rate, rx_ctrl->sig_mode,
                     rx_ctrl->mcs, rx_ctrl->cwb, rx_ctrl->smoothing, rx_ctrl->not_sounding,
                     rx_ctrl->aggregation, rx_ctrl->stbc, rx_ctrl->fec_coding, rx_ctrl->sgi,
                     rx_ctrl->noise_floor, rx_ctrl->ampdu_cnt, rx_ctrl->channel, rx_ctrl->secondary_channel,
                     rx_ctrl->timestamp, rx_ctrl->ant, rx_ctrl->sig_len, rx_ctrl->rx_state);

  if (sizeof(data) - offset > 0)
  {
    offset += snprintf(data + offset, sizeof(data) - offset, ",%d,%d,\"[%d", info->len, info->first_word_invalid, info->buf[0]);
  }

  for (int i = 1; i < info->len && sizeof(data) - offset > 0; i++)
  {
    offset += snprintf(data + offset, sizeof(data) - offset, ",%d", info->buf[i]);
  }

  if (sizeof(data) - offset > 0)
  {
    snprintf(data + offset, sizeof(data) - offset, "]\"\n");
  }

  // printf("%d\n", s_count);
  printf("%s", data);

  // if (sockfd != -1)
  // {
  //   send(sockfd, data, strlen(data), 0);
  // }
}

static void wifi_csi_init()
{
  /**
   * @brief In order to ensure the compatibility of routers, only LLTF sub-carriers are selected.
   */
  wifi_csi_config_t csi_config = {
      .lltf_en = true,
      .htltf_en = false,
      .stbc_htltf2_en = false,
      .ltf_merge_en = true,
      .channel_filter_en = true,
      .manu_scale = true,
      .shift = true,
  };

  static wifi_ap_record_t s_ap_info = {0};
  ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&s_ap_info));
  ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
  ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, s_ap_info.bssid));
  ESP_ERROR_CHECK(esp_wifi_set_csi(true));
}

static esp_err_t wifi_ping_router_start()
{
  static esp_ping_handle_t ping_handle = NULL;

  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
  ping_config.count = 0;
  ping_config.interval_ms = 1000 / CONFIG_SEND_FREQUENCY;
  ping_config.task_stack_size = 3072;
  ping_config.data_size = 1;

  esp_netif_ip_info_t local_ip;
  esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &local_ip);
  ESP_LOGI(TAG, "got ip:" IPSTR ", gw: " IPSTR, IP2STR(&local_ip.ip), IP2STR(&local_ip.gw));
  ping_config.target_addr.u_addr.ip4.addr = ip4_addr_get_u32(&local_ip.gw);
  ping_config.target_addr.type = ESP_IPADDR_TYPE_V4;

  esp_ping_callbacks_t cbs = {0};
  esp_ping_new_session(&ping_config, &cbs, &ping_handle);
  esp_ping_start(ping_handle);

  return ESP_OK;
}

// static void socket_recv_task(void *pvParameters)
// {
//   char recv_buf[1024];
//   while (1)
//   {
//     int len = recv(sockfd, recv_buf, sizeof(recv_buf) - 1, 0);
//     if (len > 0)
//     {
//       recv_buf[len] = 0; // Null-terminate the received data
//       ESP_LOGI(TAG, "Received data: %s", recv_buf);
//     }
//     else if (len == 0)
//     {
//       ESP_LOGI(TAG, "Connection closed");
//       break;
//     }
//     else
//     {
//       ESP_LOGE(TAG, "recv failed: %d", errno);
//       break;
//     }
//   }
//   vTaskDelete(NULL);
// }

void app_main()
{
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  /**
   * @brief This helper function configures Wi-Fi, as selected in menuconfig.
   *        Read "Establishing Wi-Fi Connection" section in esp-idf/examples/protocols/README.md
   *        for more information about this function.
   */
  ESP_ERROR_CHECK(example_connect());

  // init_socket();
  wifi_csi_init();
  wifi_ping_router_start();
  // xTaskCreate(socket_recv_task, "socket_recv_task", 4096, NULL, 5, NULL);
}