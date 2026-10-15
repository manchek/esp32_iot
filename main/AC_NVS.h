#ifndef MAIN_AC_NVS_H
#define MAIN_AC_NVS_H

/*
 *  AC_NVS.h
 *
 *  internal low-level NVS API. this is not intended to be used directly,
 *  only through the Config variable system.
 */

#include "stdint.h"
#include "esp_err.h"
#include "nvs_flash.h"

/*
 *  read and save functions take a namespace and key. NVS is opened and closed
 *  for each transaction.
 *
 *  read functions for simple types just return the value itself.
 *  we specify a default value to use in case "something goes wrong".
 *
 *  functions for variable length types string and blob take a data pointer,
 *  and a max length parameter. for strings this includes the trailing \0.
 *  for blobs it is an in/out param returning the actual length.
 */
int8_t read_nvs_i8( const char *namespace, const char *key, int8_t defval );
int16_t read_nvs_i16( const char *namespace, const char *key, int16_t defval );
int32_t read_nvs_i32( const char *namespace, const char *key, int32_t defval );
esp_err_t read_nvs_string( const char *namespace, const char *key, char *data, size_t maxlen );
esp_err_t read_nvs_blob( const char *namespace, const char *key, void *data, size_t *length );

/*
 *  save functions are pretty simple by comparison.
 */
esp_err_t save_nvs_i8( const char *namespace, const char *key, int8_t data );
esp_err_t save_nvs_i16( const char *namespace, const char *key, int16_t data );
esp_err_t save_nvs_i32( const char *namespace, const char *key, int32_t data );
esp_err_t save_nvs_string( const char *namespace, const char *key, const char *data );
esp_err_t save_nvs_blob( const char *namespace, const char *key, const void *data, size_t length );

/*
 *  test and create methods are used by the Config verifier at startup to check that
 *  NVS entries are present and of the correct type, and if not, to set defaults.
 *  they are used in batch mode (per namespace) so they take a handle (NVS_READWRITE).
 *
 *  these return ESP_OK if the NVS entry exists and, in the case of string or blob,
 *  its length is <= the specified limit (return ESP_ERR_INVALID_SIZE if not).
 *  anything else indicates function attempted to fix things using default.
 */
esp_err_t test_create_nvs_i8( nvs_handle_t h, const char *key, int8_t defval );
esp_err_t test_create_nvs_i16( nvs_handle_t h, const char *key, int16_t defval );
esp_err_t test_create_nvs_i32( nvs_handle_t h, const char *key, int32_t defval );
esp_err_t test_create_nvs_string( nvs_handle_t h, const char *key, size_t maxlen, const char *defval );
esp_err_t test_create_nvs_blob( nvs_handle_t h, const char *key, size_t maxlen, const void *defval, size_t length );

#endif /*MAIN_AC_NVS_H*/
