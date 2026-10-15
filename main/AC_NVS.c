/*
 *  AC_NVS.c
 */

#include "AC_Debug.h"
#include "AC_NVS.h"
#include "esp_log.h"

static const char *LOGTAG __attribute__((unused)) = "ACNVS";

int8_t
read_nvs_i8( const char *namespace, const char *key, int8_t defval )
{
	int8_t data = defval;
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READONLY, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_get_i8(handle, key, &data);
		nvs_close(handle);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_get_i8 %s::%s failed: %d", namespace, key, err);
		}
	}
	return data;
}

int16_t
read_nvs_i16( const char *namespace, const char *key, int16_t defval )
{
	int16_t data = defval;
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READONLY, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_get_i16(handle, key, &data);
		nvs_close(handle);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_get_i16 %s::%s failed: %d", namespace, key, err);
		}
	}
	return data;
}

int32_t
read_nvs_i32( const char *namespace, const char *key, int32_t defval )
{
	int32_t data = defval;
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READONLY, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_get_i32(handle, key, &data);
		nvs_close(handle);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_get_i32 %s::%s failed: %d", namespace, key, err);
		}
	}
	return data;
}

esp_err_t
read_nvs_string( const char *namespace, const char *key, char *data, size_t maxlen )
{
	size_t length;
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READONLY, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_get_str(handle, key, NULL, &length);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_get_str %s::%s failed: %d", namespace, key, err);
		} else if (maxlen > 0 && length > maxlen) {
			ESP_LOGE(LOGTAG, "read_nvs_string %s::%s too long : %d %d", namespace, key, (int)length, (int)maxlen);
			err = ESP_ERR_INVALID_SIZE;
		} else {
			err = nvs_get_str(handle, key, data, &length);
			if (err != ESP_OK) {
				ESP_LOGE(LOGTAG, "nvs_get_str %s::%s failed: %d", namespace, key, err);
			} else {
				if (length > 0)  // paranoia
					data[length - 1] = 0;
				ESP_LOGD_DEBUG(LOGTAG, "read_nvs_string got \"%s\" from %s::%s", data, namespace, key);
			}
		}
		nvs_close(handle);
	}
	if (err != ESP_OK)
		data[0] = 0;
	return err;
}

esp_err_t read_nvs_blob( const char *namespace, const char *key, void *data, size_t *length )
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READONLY, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_get_blob(handle, key, data, length);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_get_blob %s::%s failed: %d", namespace, key, err);
		}
		nvs_close(handle);
	}
	return err;
}

esp_err_t
save_nvs_i8( const char *namespace, const char *key, int8_t data )
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_set_i8(handle, key, data);
		nvs_close(handle);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_i8 %s::%s failed: %d", namespace, key, err);
		} else {
			ESP_LOGI(LOGTAG, "save_nvs_i8 %s::%s=%d", namespace, key, (int)data);
		}
	}
	return err;
}

esp_err_t
save_nvs_i16( const char *namespace, const char *key, int16_t data )
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_set_i16(handle, key, data);
		nvs_close(handle);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_i16 %s::%s failed: %d", namespace, key, err);
		} else {
			ESP_LOGI(LOGTAG, "save_nvs_i16 %s::%s=%d", namespace, key, (int)data);
		}
	}
	return err;
}

esp_err_t
save_nvs_i32( const char *namespace, const char *key, int32_t data )
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_set_i32(handle, key, data);
		nvs_close(handle);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_i32 %s::%s failed: %d", namespace, key, err);
		} else {
			ESP_LOGI(LOGTAG, "save_nvs_i32 %s::%s=%d", namespace, key, (int)data);
		}
	}
	return err;
}

esp_err_t
save_nvs_string( const char *namespace, const char *key, const char *data )
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_set_str(handle, key, data);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_str %s::%s failed: %d", namespace, key, err);
		} else {
			err = nvs_commit(handle);
			if (err != ESP_OK) {
				ESP_LOGE(LOGTAG, "nvs_commit %s::%s failed: %d", namespace, key, err);
			} else {
				ESP_LOGI(LOGTAG, "save_nvs_string %s::%s=%s", namespace, key, data);
			}
		}
		nvs_close(handle);
	}
	return err;
}

esp_err_t
save_nvs_blob( const char *namespace, const char *key, const void *data, size_t length )
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open(namespace, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(LOGTAG, "Open namespace %s failed: %d", namespace, err);
	} else {
		err = nvs_set_blob(handle, key, data, length);
		if (err != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_blob %s::%s failed: %d", namespace, key, err);
		} else {
			err = nvs_commit(handle);
			if (err != ESP_OK) {
				ESP_LOGE(LOGTAG, "nvs_commit %s::%s failed: %d", namespace, key, err);
			} else {
				ESP_LOGI(LOGTAG, "save_nvs_blob %s::%s [%d]", namespace, key, (int)length);
			}
		}
		nvs_close(handle);
	}
	return err;
}

esp_err_t
test_create_nvs_i8( nvs_handle_t handle, const char *key, int8_t defval )
{
	int8_t data;
	esp_err_t err = nvs_get_i8(handle, key, &data);
	if (err != ESP_OK) {
		ESP_LOGW(LOGTAG, "test nvs_get_i8 %s failed: %d", key, err);
		esp_err_t err2;
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			err2 = nvs_erase_key(handle, key);
			if (err2 != ESP_OK) {
				ESP_LOGE(LOGTAG, "nvs_erase_key %s failed: %d", key, err);
			}
		}
		err2 = nvs_set_i8(handle, key, defval);
		if (err2 != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_i8 %s failed: %d", key, err);
		} else {
			ESP_LOGI(LOGTAG, "test_create_nvs_i8 init %s = %d", key, defval);
		}
	}
	return err;
}

esp_err_t
test_create_nvs_i16( nvs_handle_t handle, const char *key, int16_t defval )
{
	int16_t data;
	esp_err_t err = nvs_get_i16(handle, key, &data);
	if (err != ESP_OK) {
		ESP_LOGW(LOGTAG, "test nvs_get_i16 %s failed: %d", key, err);
		esp_err_t err2;
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			err2 = nvs_erase_key(handle, key);
			if (err2 != ESP_OK) {
				ESP_LOGE(LOGTAG, "nvs_erase_key %s failed: %d", key, err);
			}
		}
		err2 = nvs_set_i16(handle, key, defval);
		if (err2 != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_i16 %s failed: %d", key, err);
		} else {
			ESP_LOGI(LOGTAG, "test_create_nvs_i16 init %s = %d", key, defval);
		}
	}
	return err;
}

esp_err_t
test_create_nvs_i32( nvs_handle_t handle, const char *key, int32_t defval )
{
	int32_t data;
	esp_err_t err = nvs_get_i32(handle, key, &data);
	if (err != ESP_OK) {
		ESP_LOGW(LOGTAG, "test nvs_get_i32 %s failed: %d", key, err);
		esp_err_t err2;
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			err2 = nvs_erase_key(handle, key);
			if (err2 != ESP_OK) {
				ESP_LOGE(LOGTAG, "nvs_erase_key %s failed: %d", key, err);
			}
		}
		err2 = nvs_set_i32(handle, key, defval);
		if (err2 != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_i32 %s failed: %d", key, err);
		} else {
			ESP_LOGI(LOGTAG, "test_create_nvs_i32 init %s = %d", key, defval);
		}
	}
	return err;
}

esp_err_t
test_create_nvs_string( nvs_handle_t handle, const char *key, size_t maxlen, const char *defval )
{
	size_t length;
	esp_err_t err = nvs_get_str(handle, key, NULL, &length);
	if (err != ESP_OK) {
		ESP_LOGW(LOGTAG, "test nvs_get_str %s failed: %d", key, err);
	} else if (maxlen > 0 && length > maxlen) {
		ESP_LOGE(LOGTAG, "test_create_nvs_string %s too long : %d %d", key, (int)length, (int)maxlen);
		err = ESP_ERR_INVALID_SIZE;
	}
	if (err != ESP_OK) {
		esp_err_t err2;
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			err2 = nvs_erase_key(handle, key);
			if (err2 != ESP_OK) {
				ESP_LOGE(LOGTAG, "nvs_erase_key %s failed: %d", key, err);
			}
		}
		err2 = nvs_set_str(handle, key, defval);
		if (err2 != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_str %s failed: %d", key, err);
		} else {
			ESP_LOGI(LOGTAG, "test_create_nvs_string init %s", key);
		}
	}
	return err;
}

esp_err_t
test_create_nvs_blob( nvs_handle_t handle, const char *key, size_t maxlen, const void *defval, size_t deflen )
{
	size_t length;
	esp_err_t err = nvs_get_blob(handle, key, NULL, &length);
	if (err != ESP_OK) {
		ESP_LOGW(LOGTAG, "test nvs_get_blob %s failed: %d", key, err);
	} else if (maxlen > 0 && length > maxlen) {
		ESP_LOGE(LOGTAG, "test_create_nvs_blob %s too long : %d %d", key, (int)length, (int)maxlen);
		err = ESP_ERR_INVALID_SIZE;
	}
	if (err != ESP_OK) {
		esp_err_t err2;
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			err2 = nvs_erase_key(handle, key);
			if (err2 != ESP_OK) {
				ESP_LOGE(LOGTAG, "nvs_erase_key %s failed: %d", key, err);
			}
		}
		err2 = nvs_set_blob(handle, key, defval, deflen);
		if (err2 != ESP_OK) {
			ESP_LOGE(LOGTAG, "nvs_set_blob %s failed: %d", key, err);
		} else {
			ESP_LOGI(LOGTAG, "test_create_nvs_blob init %s", key);
		}
	}
	return err;
}

