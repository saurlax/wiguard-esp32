#pragma once
#include "mqtt_client.h"

extern esp_mqtt_client_handle_t client;
extern EventGroupHandle_t s_mqtt_event_group;

extern const int MQTT_CONNECTED_BIT;

void mqtt_app_start(void);
void send_csi(esp_mqtt_client_handle_t client, const char *csi_data);