
#include "NetworkManager.h"

#include <esp_log.h>
#include <esp_mac.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <netinet/in.h>

#include "Config.h"

#define HEX_MAC_LENGTH 18
#define MAX_AP_RECORDS 10
#define DEVICE_ID_LENGTH 18
#define AP_NAME_LENGTH 20  // "AirCyclerConnectXXX\0"

enum {
	STA_STATUS_DOWN = 0,
	STA_STATUS_UP,
	STA_STATUS_CONN
};

static const char *LOGTAG __attribute__((unused)) = "NET";

static char device_mac[HEX_MAC_LENGTH] = {0};
static char device_name[DEVICE_ID_LENGTH] = {0};
static TaskHandle_t nm_task_handle = NULL;      // network manager task
static bool nm_task_run = false;                // enable task to run
static esp_netif_t *netif_ap_handle = NULL;
static esp_netif_t *netif_sta_handle = NULL;
static int sta_mode = STA_MODE_NONE;
static int sta_mode_req = STA_MODE_NONE;
static int sta_status = STA_STATUS_DOWN;
static char ap_name[AP_NAME_LENGTH] = {0};
static int ap_mode = AP_MODE_NONE;
static int ap_mode_req = AP_MODE_NONE;
static uint8_t ap_count = 0;                    // AP count from last scan
static wifi_ap_record_t ap_list[MAX_AP_RECORDS];// APs found in last scan

char *get_base_mac()
{
	if (!device_mac[0]) {
		uint8_t baseMac[6];
		ESP_ERROR_CHECK(esp_efuse_mac_get_default(baseMac));
		snprintf(device_mac, sizeof(device_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
				baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
	}
	return device_mac;
}

char *get_portal_ap_name()
{
	if (!ap_name[0]) {
		uint8_t baseMac[6];
		ESP_ERROR_CHECK(esp_efuse_mac_get_default(baseMac));
		snprintf(ap_name, sizeof(ap_name), "AirCyclerConnect%X%02X", baseMac[4] & 0xf, baseMac[5]);
	}
	return ap_name;
}

char *get_connect_ap_name()
{
	if (!device_name[0]) {
		uint8_t baseMac[6];
		ESP_ERROR_CHECK(esp_efuse_mac_get_default(baseMac));
		snprintf(device_name, sizeof(device_name), "AirCyclerG3%02X%02X%02X",
				baseMac[3], baseMac[4], baseMac[5]);
	}
	return device_name;
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

	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
		sta_status = STA_STATUS_UP;

	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		sta_status = STA_STATUS_DOWN;
		wifi_event_sta_disconnected_t *info = (wifi_event_sta_disconnected_t *)event_data;
		ESP_LOGW(LOGTAG, "WiFi disconnected reason %d", (int)info->reason);

	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
		uint16_t n;
		esp_wifi_scan_get_ap_num(&n);
		ESP_LOGI(LOGTAG, "Found %d Wifi AP", (int)n);
		if (n > MAX_AP_RECORDS)
			n = MAX_AP_RECORDS;

		//esp_wifi_clear_ap_list();
		esp_wifi_scan_get_ap_records(&n, ap_list);
		ap_count = n;
		//ap_scan_running = 0;
		for (int i = 0; i < n; i++) {
			ESP_LOGI(LOGTAG, "%d. %s rssi %d", i, ap_list[i].ssid, (int)ap_list[i].rssi);
		}
	}
}

static void nm_task( void* arg )
{
	ESP_LOGI(LOGTAG, "Network Manager begins");

	while (nm_task_run) {
		if (sta_mode_req != sta_mode) {
			ESP_LOGI(LOGTAG, "STA mode changes %d>%d", sta_mode, sta_mode_req);
			sta_mode = sta_mode_req;
			esp_err_t rc;
			if ((rc = esp_wifi_disconnect()) != ESP_OK)
				ESP_LOGE(LOGTAG, "Disconnect STA: %s", esp_err_to_name(rc));
		}

		if (ap_mode_req != ap_mode) {
			ESP_LOGI(LOGTAG, "AP mode changes %d>%d", ap_mode, ap_mode_req);

			ap_mode = ap_mode_req;

			switch (ap_mode) {
			case AP_MODE_NONE:
				{
					esp_err_t rc;
					wifi_config_t ap_config;
					if ((rc = esp_wifi_get_config(WIFI_IF_AP, &ap_config)) != ESP_OK) {
						ESP_LOGE(LOGTAG, "Get AP config: %s", esp_err_to_name(rc));

					} else {
						ap_config.ap.max_connection = 0;
						ap_config.ap.ssid_hidden = 1;
						if ((rc = esp_wifi_set_config(WIFI_IF_AP, &ap_config)) != ESP_OK)
							ESP_LOGE(LOGTAG, "Set AP config: %s", esp_err_to_name(rc));
						if ((rc = esp_wifi_deauth_sta(0)) != ESP_OK)
							ESP_LOGE(LOGTAG, "esp_wifi_deauth_sta: %s", esp_err_to_name(rc));
					}
				}
				break;

			case AP_MODE_PORTAL:
				{
					esp_err_t rc;
					wifi_config_t ap_config;
					//memset(&ap_config, 0, sizeof(ap_config));
					if ((rc = esp_wifi_get_config(WIFI_IF_AP, &ap_config)) != ESP_OK) {
						ESP_LOGE(LOGTAG, "Get AP config: %s", esp_err_to_name(rc));

					} else {
						memset(ap_config.ap.ssid, 0, sizeof(ap_config.ap.ssid));
						memset(ap_config.ap.password, 0, sizeof(ap_config.ap.password));
						strcpy((char*)ap_config.ap.ssid, get_portal_ap_name());
						ap_config.ap.ssid_len = 0;
						ap_config.ap.authmode = WIFI_AUTH_OPEN;
						ap_config.ap.ssid_hidden = 0;
						ap_config.ap.max_connection = 8;  // max number of connected devices
						if ((rc = esp_wifi_set_config(WIFI_IF_AP, &ap_config)) != ESP_OK)
							ESP_LOGE(LOGTAG, "Set AP config: %s", esp_err_to_name(rc));
					}
				}
				break;

			case AP_MODE_CONNECTED:
			case AP_MODE_PAIRING:
				{
					esp_err_t rc;
					wifi_config_t ap_config;
					//memset(&ap_config, 0, sizeof(ap_config));
					if ((rc = esp_wifi_get_config(WIFI_IF_AP, &ap_config)) != ESP_OK) {
						ESP_LOGE(LOGTAG, "Get AP config: %s", esp_err_to_name(rc));

					} else {
						memset(ap_config.ap.ssid, 0, sizeof(ap_config.ap.ssid));
						memset(ap_config.ap.password, 0, sizeof(ap_config.ap.password));
						strcpy((char*)ap_config.ap.ssid, get_connect_ap_name());
						strcpy((char*)ap_config.ap.password, "SecretPassword");
						ap_config.ap.ssid_len = 0;
						ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
						ap_config.ap.ssid_hidden = (ap_mode == AP_MODE_CONNECTED) ? 1 : 0;
						ap_config.ap.max_connection = 8;  // max number of connected devices
						if ((rc = esp_wifi_set_config(WIFI_IF_AP, &ap_config)) != ESP_OK)
							ESP_LOGE(LOGTAG, "Set AP config: %s", esp_err_to_name(rc));
					}
				}
				break;
			}
		}

		switch (sta_mode) {
		case STA_MODE_PUBLIC:
			if (sta_status == STA_STATUS_DOWN) {
				if (config_get_wifi_ssid()[0] && config_get_wifi_pw()[0]) {
					esp_err_t rc;
					ESP_LOGI(LOGTAG, "Connect to %s", config_get_wifi_ssid());
					sta_status = STA_STATUS_CONN;
					wifi_config_t wifi_sta_config = { 0 };
					strcpy((char*)wifi_sta_config.sta.ssid, config_get_wifi_ssid());
					strcpy((char*)wifi_sta_config.sta.password, config_get_wifi_pw());
					if ((rc = esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config)) != ESP_OK)
						ESP_LOGE(LOGTAG, "Set STA config: %s", esp_err_to_name(rc));
					if ((rc = esp_wifi_connect()) != ESP_OK)
						ESP_LOGE(LOGTAG, "Connect STA: %s", esp_err_to_name(rc));
				}
			}
			break;
		}

		vTaskDelay(pdMS_TO_TICKS(1000));
	}

    ESP_LOGI(LOGTAG, "Network Manager ends");
    nm_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t
network_init()
{
	if (nm_task_handle != NULL)
		return ESP_ERR_INVALID_STATE;

	ESP_LOGI(LOGTAG, "Starting Network Manager");

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
	 *  TODO: is this enough to have "no" AP?
	 */
	wifi_config_t ap_config;
	memset(&ap_config, 0, sizeof(ap_config));
	ap_config.ap.ssid_hidden = 1;
	ap_config.ap.max_connection = 8;  // max number of connected devices
	//ap_config.ap.beacon_interval = 100;
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

	ESP_ERROR_CHECK(esp_wifi_start());

	/*
	 *  configure the Station
	 */
	wifi_config_t sta_config;
	memset(&sta_config, 0, sizeof(sta_config));
	sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
	sta_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

	nm_task_run = true;
	xTaskCreate(nm_task, "nm_task", 7168, NULL, 15, &nm_task_handle);
	return ESP_OK;
}

esp_err_t
network_set_sta_mode( int mode )
{
	sta_mode_req = mode;
	return ESP_OK;
}

esp_err_t
network_set_ap_mode( int mode )
{
	ap_mode_req = mode;
	return ESP_OK;
}

void
network_advertise_ap( bool onoff )
{
/*
	wifi_config_t ap_config;
	ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_AP, &ap_config));
	ap_config.ap.ssid_hidden = !onoff;
	ESP_LOGI(LOGTAG, "Set AP hidden %d", ap_config.ap.ssid_hidden);
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
*/
}

esp_err_t
network_scan_access_points( uint8_t *count, wifi_ap_record_t **list )
{
	esp_wifi_scan_start(NULL, true);
	*count = ap_count;
	*list = ap_list;
	return ESP_OK;
}

#define DOTTED_IP_LENGTH 16  // 255.255.255.255\0

static char sta_ip[DOTTED_IP_LENGTH];

char *
network_get_sta_ip()
{
	if (netif_sta_handle && sta_status == STA_STATUS_UP) {
		esp_netif_ip_info_t ip_info;
		esp_netif_get_ip_info(netif_sta_handle, &ip_info);
		uint32_t a = ntohl(ip_info.ip.addr);
		snprintf(sta_ip, sizeof(sta_ip), "%d.%d.%d.%d", (int)(a >> 24) & 0xff, (int)(a >> 16) & 0xff, (int)(a >> 8) & 0xff, (int)a & 0xff);
	} else {
		sta_ip[0] = 0;
	}
	return sta_ip;
}

