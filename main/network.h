#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <esp_wifi.h>

void network_init();
void advertise_ap( bool onoff );
void network_scan( bool block );
void network_scan_access_points( uint8_t *count, wifi_ap_record_t **list );

/**
 *  set SSID for public network operation.
 */
void network_set_ssid( const char *ssid );

/**
 *  set password for public network operation.
 */
void network_set_pass( const char *pass );

#endif /*NETWORK_H*/
