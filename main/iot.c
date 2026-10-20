#include <stdio.h>

#include <esp_system.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_flash_partitions.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include "network.h"
#include "portal.h"
#include "nowfun.h"
#include "Config.h"

static const char *LOGTAG = "MAIN";

void monitor_heap() {
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    ESP_LOGI(LOGTAG,
			"HeapInfo aby %d fby %d mfby %d abk %d fbk %d tbk %d lfbk %d",
			heap_info.total_allocated_bytes,
			heap_info.total_free_bytes,
			heap_info.minimum_free_bytes,
			heap_info.allocated_blocks,
			heap_info.free_blocks,
			heap_info.total_blocks,
			heap_info.largest_free_block
			);
}

void
dump_partitions()
{
	ESP_LOGI(LOGTAG, "Flash partitions:");
	esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
	while (it != NULL) {
		const esp_partition_t *par = esp_partition_get(it);
		esp_ota_img_states_t st = 0xdeadbeef;
		if (esp_ota_get_state_partition(par, &st) == ESP_OK)
			ESP_LOGI(LOGTAG, "label %s type %d.%d addr %08lx size %08lx ota_state %08x",
					par->label, par->type, par->subtype, par->address, par->size, st);
		else
			ESP_LOGI(LOGTAG, "label %s type %d.%d addr %08lx size %08lx",
					par->label, par->type, par->subtype, par->address, par->size);
		it = esp_partition_next(it);
	}
	esp_partition_iterator_release(it);
}

void
check_running_partition()
{
	const esp_partition_t *par = esp_ota_get_running_partition();
	esp_ota_img_states_t st = 0xdeadbeef;
	if (esp_ota_get_state_partition(par, &st) == ESP_OK) {
		ESP_LOGI(LOGTAG, "Running partition %s ota_state %08x", par->label, st);
		if (st == ESP_OTA_IMG_PENDING_VERIFY) {
			ESP_LOGI(LOGTAG, "Mark running image valid");
			esp_ota_mark_app_valid_cancel_rollback();
		}
	} else {
		ESP_LOGE(LOGTAG, "Can't get state of running partition");
	}
}

void
mark_image_valid()
{
	static bool already = false;
	if (already) {
		ESP_LOGE(LOGTAG, "mark_image_valid called again");
		return;
	}
	already = true;
	const esp_partition_t *par = esp_ota_get_running_partition();
	esp_ota_img_states_t st = 0xdeadbeef;
	if (esp_ota_get_state_partition(par, &st) == ESP_OK) {
		ESP_LOGI(LOGTAG, "Running partition %s ota_state %08x", par->label, st);
		if (st == ESP_OTA_IMG_PENDING_VERIFY) {
			ESP_LOGI(LOGTAG, "Mark running image valid");
			esp_ota_mark_app_valid_cancel_rollback();
		}
	} else {
		ESP_LOGE(LOGTAG, "Can't get state of running partition");
	}
}

/*
 *  if "fdb" isn't present in flash, or its value is nonzero,
 *  we're doing a factory reset boot. wipe flash and let
 *  config_init set up new defaults.
 */
void
check_factory_boot()
{
	ESP_LOGI(LOGTAG, "check_factory_boot");
	nvs_handle_t handle;
	esp_err_t err = nvs_open(CONFIG_NAMESPACE_SYSTEM_CONFIG, NVS_READONLY, &handle);
	uint8_t fdb = 0;
	if (err != ESP_OK) {
		ESP_LOGI(LOGTAG, "nvs_open %d", err);
	} else {
		err = nvs_get_u8(handle, CONFIG_FACTORY_DEFAULT_BOOT_KEY, &fdb);
		if (err != ESP_OK) {
			ESP_LOGI(LOGTAG, "nvs_get_u8 %d", err);
		}
		nvs_close(handle);
	}
	if (err != ESP_OK || fdb) {
		ESP_LOGW(LOGTAG, "Reset NVS to factory defaults");
		ESP_ERROR_CHECK(nvs_flash_erase());
		ESP_ERROR_CHECK(nvs_flash_init());
		//checkAdd(CCODE_FACTORY_RESET);
	}
}

void app_main(void)
{
	vTaskDelay(pdMS_TO_TICKS(2000));  // XXX so we can get console started

	ESP_LOGI(LOGTAG, "Reset reason %d", esp_reset_reason());
	dump_partitions();
	check_running_partition();

	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	int button_count = 0;
	for (int i = 0; i < 100; i++) {
		if (!gpio_get_level(GPIO_NUM_0))
			++button_count;
		vTaskDelay(pdMS_TO_TICKS(20));
	}
	ESP_LOGI(LOGTAG, "Button count %d", button_count);
	if (button_count > 50) {
		ESP_LOGW(LOGTAG, "Factory reset triggered");
		config_set_factory_default_boot(1);
		vTaskDelay(pdMS_TO_TICKS(2000));
		esp_restart();
	}

	check_factory_boot();  // this must come before config_init
	config_init();

/*
	ESP_ERROR_CHECK(esp_flash_init(NULL));
	uint32_t size = 0;
	esp_flash_get_size(NULL, &size);
	ESP_LOGI(LOGTAG, "Flash size: %d", size);
	size = 0;
	esp_flash_get_physical_size(NULL, &size);
	ESP_LOGI(LOGTAG, "Flash pyhsical size: %d", size);
*/

	network_init();
	advertise_ap(true);

	portal_init();
	portal_enable(true);

	while (true) {
		static int last_heap_check = 0;
		int now = esp_timer_get_time() / 1000000;
		if (now - last_heap_check >= 60) {
			last_heap_check = now;
			monitor_heap();

			UBaseType_t hwm;
			hwm = uxTaskGetStackHighWaterMark(NULL);
			ESP_LOGI(LOGTAG, "Stack HWM %d", hwm);
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

