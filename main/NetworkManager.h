#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdbool.h>
#include <esp_wifi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  modes that WiFi station can be in
 */
#define STA_MODE_LIST \
		F(NONE), \
		F(CONNECTED), \
		F(PUBLIC)

enum {
#define F(st) STA_MODE_ ## st
STA_MODE_LIST
#undef F
};

/**
 *  modes that WiFi soft AP can be in
 */
#define AP_MODE_LIST \
		F(NONE), \
		F(CONNECTED), \
		F(PORTAL)

enum {
#define F(st) AP_MODE_ ## st
AP_MODE_LIST
#undef F
};

char *get_base_mac();

/**
 *  must be called before any other network functions.
 *  kicks off the manager thread that is responsible for things like applying
 *  configuration changes, recovering from disconnects, etc.
 */
esp_err_t network_init();

esp_err_t network_set_sta_mode( int mode );

esp_err_t network_set_ap_mode( int mode );

/**
 *  turn soft AP advertisement on or off;
 */
void network_advertise_ap( bool onoff );

/**
 *  do a synchronous scan of available APs
 */
esp_err_t network_scan_access_points( uint8_t *count, wifi_ap_record_t **list );

char *network_get_sta_ip();

#ifdef __cplusplus
}
#endif

#endif /*NETWORK_MANAGER_H*/
