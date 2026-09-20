
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>

#include "network.h"

#define AP_NAME_LENGTH 20  // "AirCyclerConnectXXX\0"

static const char *LOGTAG __attribute__((unused)) = "NET";

static char ap_name[AP_NAME_LENGTH] = {0};
static esp_netif_t *netif_ap_handle = NULL;
static esp_netif_t *netif_sta_handle = NULL;

char *get_ap_name()
{
	if (!ap_name[0]) {
		uint8_t baseMac[6];
		ESP_ERROR_CHECK(esp_efuse_mac_get_default(baseMac));
		snprintf(ap_name, sizeof(ap_name), "AirCyclerConnect%X%02X", baseMac[4] & 0xf, baseMac[5]);
	}
	return ap_name;
}

static void
ip_event_handler( void *ctx, esp_event_base_t event_base, int32_t event_id, void* event_data )
{
	ESP_LOGI(LOGTAG, "IP event %d %d", event_base, event_id);
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
#if 0
		static int once = 1;
		if (once) {
			once = 0;

			ESP_LOGI(LOGTAG, "Start SNTP");
			esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
			//config.server_from_dhcp = true;
			config.smooth_sync = true;
			config.start = true;
			config.sync_cb = time_cb;
			esp_err_t e = esp_netif_sntp_init(&config);
			if (e != ESP_OK) {
				ESP_LOGE(LOGTAG, "esp_netif_sntp_init = %d", e);
			}
		}
#endif
	}
}

static void
wifi_event_handler( void *ctx, esp_event_base_t event_base, int32_t event_id, void* event_data )
{
	ESP_LOGI(LOGTAG, "WiFi event %d %d", event_base, event_id);

#if 0
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
		uint16_t n;
		esp_wifi_scan_get_ap_num(&n);
		ESP_LOGI(LOGTAG, "Found %d Wifi AP", (int)n);
		if (n > MAX_AP_RECORDS)
			n = MAX_AP_RECORDS;

		esp_wifi_scan_get_ap_records(&n, ap_list);
		ap_count = n;
		ap_scan_running = 0;
/*
		for (int i = 0; i < n; i++) {
			ESP_LOGI(LOGTAG, "%d. %s rssi %d", i, aps[i].ssid, (int)aps[i].rssi);
		}
*/
	}
#endif
}

void network_init()
{
	ESP_LOGI(LOGTAG, "network_init");

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

	netif_ap_handle = esp_netif_create_default_wifi_ap();
	netif_sta_handle = esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

	/*
	 *  configure the AP
	 */
	wifi_config_t ap_config;
	memset(&ap_config, 0, sizeof(ap_config));
    strcpy((char*)ap_config.ap.ssid, get_ap_name());
    //strcpy((char*)ap_config.ap.password, "xxxxxxxx");
    //ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
	ap_config.ap.ssid_hidden = 1;
    ap_config.ap.max_connection = 8;  // max number of connected devices
    //ap_config.ap.beacon_interval = 100;
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());

	/*
	 *  configure the Station
	 */
	wifi_config_t sta_config;
	memset(&sta_config, 0, sizeof(sta_config));
    sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    sta_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

#if 0
	const char *ssid = config_get_wifi_ssid();
	const char *pw = config_get_wifi_pw();
	if (ssid[0] && pw[0]) {
		sta_enable = 1;
		ESP_LOGI(LOGTAG, "Wifi SSID %s", ssid);
		strcpy((char*)sta_config.sta.ssid, ssid);
		strcpy((char*)sta_config.sta.password, pw);
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));
		ESP_ERROR_CHECK(esp_netif_set_hostname(netif_sta_handle, get_device_name()));
		ESP_ERROR_CHECK(esp_wifi_connect());
	} else {
		checkAdd(CCODE_WIFI_NO_CONFIG);
		sta_enable = 0;
	}
#endif
}

void advertise_ap( bool onoff )
{
	wifi_config_t ap_config;
	ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_AP, &ap_config));
	ap_config.ap.ssid_hidden = !onoff;
	ESP_LOGI(LOGTAG, "Set AP hidden %d", ap_config.ap.ssid_hidden);
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
}

