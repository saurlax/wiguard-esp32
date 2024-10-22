#pragma once
/* FreeRTOS event group to signal when we are connected & ready to make a request */
extern EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
extern const int WIFI_CONNECTED_BIT;
extern const int ESPTOUCH_DONE_BIT;

void initialise_wifi(void);