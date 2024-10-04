#pragma once
#include <esp_wifi_types.h>
#include <esp_netif_types.h>

void wifi_csi_init();
void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info);
esp_err_t wifi_ping_router_start();