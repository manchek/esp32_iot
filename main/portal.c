
#include <string.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include "portal.h"
#include "network.h"
#include "FormUploader.h"
#include "MediaFiles.h"

static const char *LOGTAG = "PORTAL";

static httpd_handle_t httpd_handle = NULL;

//static esp_err_t root_handler( httpd_req_t *req );
static esp_err_t media_handler( httpd_req_t *req );
//static esp_err_t debug_handler( httpd_req_t *req );
static esp_err_t form_handler( httpd_req_t *req );
static esp_err_t login_handler( httpd_req_t *req );
static esp_err_t logout_handler( httpd_req_t *req );

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

static httpd_uri_t login_uri = {
	.uri      = "/login",
	.method   = HTTP_ANY,
	.handler  = login_handler,
	.user_ctx = NULL
};

static httpd_uri_t logout_uri = {
	.uri      = "/logout",
	.method   = HTTP_ANY,
	.handler  = logout_handler,
	.user_ctx = NULL
};

static httpd_uri_t form_uri = {
	.uri      = "/config.html",
	.method   = HTTP_ANY,
	.handler  = form_handler,
	.user_ctx = NULL
};

static const char *page_header =
	"<html>"
	"<head>"
	"<link rel=\"stylesheet\" type=\"text/css\" href=\"pages.css\">"
	"</head>"
	"<body>"
	;

static const char *page_footer =
	"</body>"
	"</html>"
	;

#if 0
static const char *root_html =
"<img src=\"/media/logo.png\">"
"<hr>"
"Portal test page"
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

static char login_token[20];

static bool
check_login( httpd_req_t *req )
{
	char token[64];
	token[0] = '!';
	token[1] = 0;
	size_t tok_len = sizeof(token);
	esp_err_t err = httpd_req_get_cookie_val(req, "VCDR_TOKEN", token, &tok_len);
	ESP_LOGI(LOGTAG, "VCDR_TOKEN len %d token \"%s\" err %d : %s", tok_len, token, err, esp_err_to_name(err));
	if (strcmp(token, login_token)) {
		const char *mimetype;
		int size;
		const uint8_t *data;
		if (find_media("login.html", 10, &mimetype, &size, &data)) {
			httpd_resp_set_type(req, mimetype);
			httpd_resp_send(req, (char *)data, size);
		} else {
			httpd_resp_set_status(req, "500 Internal Server Error");
			httpd_resp_send(req, "", 0);
		}
		return false;
	}
	return true;
}

#if 0
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
#endif

static esp_err_t
login_handler( httpd_req_t *req )
{
	if (req->method == HTTP_POST && req->content_len > 0) {
		char ct[64];
		ct[0] = 0;
		httpd_req_get_hdr_value_str(req, "Content-type", ct, sizeof(ct));
		ESP_LOGI(LOGTAG, "Content-type: %s", ct);
		FormUploader_t upl;
		formUploaderInit(&upl, req, ct);
		char *name;
		char *value;
		while (formUploaderNext(&upl, &name, &value)) {
			ESP_LOGI(LOGTAG, "Post: %s = %s", name, value);
		}

		snprintf(login_token, sizeof(login_token), "%016llx", esp_timer_get_time());
		ESP_LOGI(LOGTAG, "Generate login token %s", login_token);
		char cookie[64];
		snprintf(cookie, sizeof(cookie), "VCDR_TOKEN=%s; Path=/", login_token);
		esp_err_t err = httpd_resp_set_hdr(req, "Set-Cookie", cookie);
		ESP_LOGI(LOGTAG, "httpd_resp_set_hdr : %d : %s", err, esp_err_to_name(err));
		const char *mimetype;
		int size;
		const uint8_t *data;
		if (find_media("login_success.html", 18, &mimetype, &size, &data)) {
			httpd_resp_set_type(req, mimetype);
			httpd_resp_send(req, (char *)data, size);
			return ESP_OK;
		}
	}
	httpd_resp_set_status(req, "500 Internal Server Error");
	httpd_resp_send(req, "", 0);
	return ESP_OK;
}

static esp_err_t
logout_handler( httpd_req_t *req )
{
	char cookie[64];
	snprintf(cookie, sizeof(cookie), "VCDR_TOKEN=x; Max-Age=0");
	esp_err_t err = httpd_resp_set_hdr(req, "Set-Cookie", cookie);
	ESP_LOGI(LOGTAG, "httpd_resp_set_hdr : %d : %s", err, esp_err_to_name(err));
	const char *mimetype;
	int size;
	const uint8_t *data;
	if (find_media("logout.html", 11, &mimetype, &size, &data)) {
		httpd_resp_set_type(req, mimetype);
		httpd_resp_send(req, (char *)data, size);
		return ESP_OK;
	}
	httpd_resp_set_status(req, "500 Internal Server Error");
	httpd_resp_send(req, "", 0);
	return ESP_OK;
}

static esp_err_t
form_handler( httpd_req_t *req )
{
	if (!check_login(req))
		return ESP_OK;

	int pathlen;
	const char *query;
	if ((query = strchr(req->uri, '?')) != NULL)
		pathlen = query - req->uri;
	else
		pathlen = strlen(req->uri);
	const char *path = req->uri;
	ESP_LOGI(LOGTAG, "form_handler method=%d uri=%s path=%.*s query=%s content=%d", req->method, req->uri, pathlen, path, (query ? query : "(none)"), req->content_len);

	bool askscan = false;
	if (req->method == HTTP_POST && req->content_len > 0) {
#if 0
		char buf[256];
		int done = 0;
		int n;

		buf[0] = 0;
		httpd_req_get_hdr_value_str(req, "Content-type", buf, sizeof(buf));
		ESP_LOGI(LOGTAG, "Content-type: %s", buf);
		while (done < req->content_len && (n = httpd_req_recv(req, buf, sizeof(buf)-1)) > 0) {
			done += n;
			ESP_LOGI(LOGTAG, "Request: %.*s", n, buf);
		}
#else
		char ct[64];
		ct[0] = 0;
		httpd_req_get_hdr_value_str(req, "Content-type", ct, sizeof(ct));
		ESP_LOGI(LOGTAG, "Content-type: %s", ct);
		FormUploader_t upl;
		formUploaderInit(&upl, req, ct);
		char *name;
		char *value;
		while (formUploaderNext(&upl, &name, &value)) {
			ESP_LOGI(LOGTAG, "Post: %s = %s", name, value);
			if (!strcmp("scan", name))
				askscan = true;
			else if (!strcmp("ssid", name))
				network_set_ssid(value);
			else if (!strcmp("pass", name))
				network_set_pass(value);
		}
#endif
	}

	httpd_resp_sendstr_chunk(req, page_header);
	httpd_resp_sendstr_chunk(req,
		//"<form method=\"post\" enctype=\"text/plain\">"
		"<form method=\"post\">"
		"<table>"
	);
	if (askscan) {
		//network_scan(true);
		uint8_t num_ap = 0;
		wifi_ap_record_t *aps = NULL;
		network_scan_access_points(&num_ap, &aps);
		httpd_resp_sendstr_chunk(req,
			"<tr><td>SSID</td><td><input list=\"wifis\" name=\"ssid\"> -choose</td></tr>"
			"<datalist id=\"wifis\">"
		);
		for (int i = 0; i < num_ap; i++) {
			char buf[128];
			snprintf(buf, sizeof(buf), "<option value=\"%s\">", aps[i].ssid);
			httpd_resp_sendstr_chunk(req, buf);
		}
		httpd_resp_sendstr_chunk(req,
			"</datalist>"
		);
	} else {
		httpd_resp_sendstr_chunk(req,
			"<tr><td>SSID</td><td><input type=\"text\" name=\"ssid\"></td></tr>"
		);
	}
	httpd_resp_sendstr_chunk(req,
		"<tr><td>Password</td><td><input type=\"text\" name=\"pass\"></td></tr>"
		"</table>"
		"<input type=\"submit\">"
		"<button name=\"scan\" value=\"1\">Scan WiFi</button>"
		"</form>"
	);
	httpd_resp_sendstr_chunk(req, page_footer);
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

static esp_err_t
media_handler( httpd_req_t *req )
{
	if (!check_login(req))
		return ESP_OK;

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
		rc = httpd_register_uri_handler(httpd_handle, &login_uri);
		ESP_LOGI(LOGTAG, "Register %s handler : %s", login_uri.uri, esp_err_to_name(rc));
		rc = httpd_register_uri_handler(httpd_handle, &logout_uri);
		ESP_LOGI(LOGTAG, "Register %s handler : %s", logout_uri.uri, esp_err_to_name(rc));
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

