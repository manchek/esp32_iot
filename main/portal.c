
#include <string.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include "portal.h"
#include "MediaFiles.h"

static const char *LOGTAG = "PORTAL";

static httpd_handle_t httpd_handle = NULL;

//static esp_err_t root_handler( httpd_req_t *req );
static esp_err_t media_handler( httpd_req_t *req );
static esp_err_t debug_handler( httpd_req_t *req );

#if 0
static httpd_uri_t root_page = {
	.uri      = "/",
	.method   = HTTP_GET,
	.handler  = root_handler,
	.user_ctx = NULL
};
#endif

static httpd_uri_t media_base_uri = {
	.uri      = "/*",
	.method   = HTTP_GET,
	.handler  = media_handler,
	.user_ctx = NULL
};

static httpd_uri_t form_uri = {
	.uri      = "/config.html",
	.method   = HTTP_GET,
	.handler  = debug_handler,
	.user_ctx = NULL
};

#if 0
static const char *page_header =
"<html>"
"<head>"
"</head>"
"<body>"
;

static const char *root_html =
"<img src=\"/media/logo.png\">"
"<hr>"
"Portal test page"
;

static const char *page_footer =
"</body>"
"</html>"
;

static esp_err_t
root_handler( httpd_req_t *req )
{
	ESP_LOGI(LOGTAG, "root_handler");
/*
 *      httpd_resp_set_status() - for setting the HTTP status string,
 *      httpd_resp_set_type()   - for setting the Content Type,
 *      httpd_resp_set_hdr()    - for appending any additional field
 *                                value entries in the response header
*/
	//httpd_resp_send(req, root_html, strlen(root_html));
	httpd_resp_sendstr_chunk(req, page_header);
	httpd_resp_sendstr_chunk(req, root_html);
	httpd_resp_sendstr_chunk(req, page_footer);
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}
#endif

#define MEDIA_PREFIX_LEN 1  // "/"

static esp_err_t
debug_handler( httpd_req_t *req )
{
	int pathlen;
	const char *query;
	if ((query = strchr(req->uri, '?')) != NULL)
		pathlen = query - req->uri;
	else
		pathlen = strlen(req->uri);
	const char *path = req->uri;
	ESP_LOGI(LOGTAG, "debug_handler uri=%s path=%.*s query=%s", req->uri, pathlen, path, (query ? query : "(none)"));
	httpd_resp_set_status(req, "404 Not Found");
	httpd_resp_send(req, "", 0);
	return ESP_OK;
}

static esp_err_t
media_handler( httpd_req_t *req )
{
	int pathlen;
	const char *query;
	if ((query = strchr(req->uri, '?')) != NULL)
		pathlen = query - req->uri;
	else
		pathlen = strlen(req->uri);
	const char *path = req->uri;
	ESP_LOGI(LOGTAG, "media_handler uri=%s path=%.*s query=%s", req->uri, pathlen, path, (query ? query : "(none)"));

	if (pathlen == 1) {
		path = "/test_page.html";
		pathlen = strlen(path);
	}

	if (pathlen > MEDIA_PREFIX_LEN) {
		const char *mimetype;
		int size;
		const uint8_t *data;
		if (find_media(path + MEDIA_PREFIX_LEN, pathlen - MEDIA_PREFIX_LEN, &mimetype, &size, &data)) {
			httpd_resp_set_type(req, mimetype);
			httpd_resp_send(req, (char *)data, size);
			return ESP_OK;
		}
	}
/*
 *      httpd_resp_set_status() - for setting the HTTP status string,
 *      httpd_resp_set_type()   - for setting the Content Type,
 *      httpd_resp_set_hdr()    - for appending any additional field
 *                                value entries in the response header
*/
	httpd_resp_set_status(req, "404 Not Found");
	httpd_resp_send(req, "", 0);
	return ESP_OK;
}

esp_err_t
portal_init()
{
	return ESP_OK;
}

esp_err_t
portal_enable( bool onoff )
{
	if (onoff) {
		if (httpd_handle != NULL)
			return ESP_ERR_INVALID_STATE;
		ESP_LOGI(LOGTAG, "Start portal");
		httpd_config_t config = HTTPD_DEFAULT_CONFIG();
		config.uri_match_fn = httpd_uri_match_wildcard;
		ESP_ERROR_CHECK(httpd_start(&httpd_handle, &config));
		/*
		 *  register handlers from specific to wildcard
		 */
		esp_err_t rc;
		rc = httpd_register_uri_handler(httpd_handle, &form_uri);
		ESP_LOGI(LOGTAG, "Register %s handler : %s", form_uri.uri, esp_err_to_name(rc));
		rc = httpd_register_uri_handler(httpd_handle, &media_base_uri);
		ESP_LOGI(LOGTAG, "Register %s handler : %s", media_base_uri.uri, esp_err_to_name(rc));

	} else {
		if (httpd_handle == NULL)
			return ESP_ERR_INVALID_STATE;
		ESP_LOGI(LOGTAG, "Stop portal");
		httpd_stop(httpd_handle);
		httpd_handle = NULL;
	}
	return ESP_OK;
}

