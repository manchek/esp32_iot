#include <stdio.h>

#include <esp_system.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_flash_partitions.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>

static const char *LOGTAG = "MAIN";

void app_main(void)
{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	ESP_LOGI(LOGTAG, "Reset reason %d", esp_reset_reason());

	{
		ESP_LOGI(LOGTAG, "Flash partitions:");
		esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
		while (it != NULL) {
			const esp_partition_t *par = esp_partition_get(it);
			esp_ota_img_states_t st = 0xdeadbeef;
			if (esp_ota_get_state_partition(par, &st) == ESP_OK)
				ESP_LOGI("app_main", "label %s type %d.%d addr %08lx size %08lx ota_state %08x",
						par->label, par->type, par->subtype, par->address, par->size, st);
			else
				ESP_LOGI("app_main", "label %s type %d.%d addr %08lx size %08lx",
						par->label, par->type, par->subtype, par->address, par->size);
			it = esp_partition_next(it);
		}
		esp_partition_iterator_release(it);
	}

/*
	ESP_ERROR_CHECK(esp_flash_init(NULL));
	uint32_t size = 0;
	esp_flash_get_size(NULL, &size);
	ESP_LOGI(LOGTAG, "Flash size: %d", size);
	size = 0;
	esp_flash_get_physical_size(NULL, &size);
	ESP_LOGI(LOGTAG, "Flash pyhsical size: %d", size);
*/
}

