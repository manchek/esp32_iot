
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

/** single handler for all requests */
static esp_err_t base_handler( httpd_req_t *req );

static httpd_uri_t base_uri = {
	.uri      = "/*",
	.method   = HTTP_ANY,
	.handler  = base_handler,
	.user_ctx = NULL
};

#define MEDIA_PREFIX_LEN 1  // "/"

static char login_token[20];

/**
 *  verify login token in request cookies against our active token.
 *  if not found or mismatch, actually serve the login page.
 *  returns true if logged in, else false (and the caller should simply return).
 */
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
		if (find_media("login.html", &mimetype, &size, &data)) {
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
		bool valid = false;
		while (formUploaderNext(&upl, &name, &value)) {
			ESP_LOGI(LOGTAG, "Post: %s = %s", name, value);
			if (!strcmp(name, "pw") && !strcmp(value, "admin"))
				valid = true;
		}

		const char *mimetype;
		int size;
		const uint8_t *data;

		if (valid) {
			char cookie[64];
			snprintf(cookie, sizeof(cookie), "VCDR_TOKEN=%s; Path=/", login_token);
			esp_err_t err = httpd_resp_set_hdr(req, "Set-Cookie", cookie);
			ESP_LOGI(LOGTAG, "httpd_resp_set_hdr : %d : %s", err, esp_err_to_name(err));
			if (find_media("login_success.html", &mimetype, &size, &data)) {
				httpd_resp_set_type(req, mimetype);
				httpd_resp_send(req, (char *)data, size);
				return ESP_OK;
			}
		} else {
			if (find_media("deny.html", &mimetype, &size, &data)) {
				httpd_resp_set_type(req, mimetype);
				httpd_resp_send(req, (char *)data, size);
				return ESP_OK;
			}
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
	if (find_media("logout.html", &mimetype, &size, &data)) {
		httpd_resp_set_type(req, mimetype);
		httpd_resp_send(req, (char *)data, size);
		return ESP_OK;
	}
	httpd_resp_set_status(req, "500 Internal Server Error");
	httpd_resp_send(req, "", 0);
	return ESP_OK;
}

#if 0
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
#endif

static esp_err_t
media_handler( httpd_req_t *req )
{
	const char *path = req->uri;
	const char *query;
	int pathlen;
	if ((query = strchr(path, '?')) != NULL)
		pathlen = query - path;
	else
		pathlen = strlen(path);

	if (pathlen == 1) {
		path = "/default.html";
		pathlen = strlen(path);
	}

	if (pathlen > MEDIA_PREFIX_LEN) {
		const char *mimetype;
		int size;
		const uint8_t *data;
		if (find_media_len(path + MEDIA_PREFIX_LEN, pathlen - MEDIA_PREFIX_LEN, &mimetype, &size, &data)) {
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

static esp_err_t
base_handler( httpd_req_t *req )
{
	const char *path = req->uri;
	const char *query;
	int pathlen;
	if ((query = strchr(path, '?')) != NULL)
		pathlen = query - path;
	else
		pathlen = strlen(path);
	ESP_LOGI(LOGTAG, "base_handler method=%s uri=%s path=%.*s query=%s", http_method_str(req->method), req->uri, pathlen, path, (query ? query : "(none)"));

	/*
	 *  if not logged in, we serve only the login page
	 */
	if (pathlen == 6 && !strncmp(path, "/login", 6) && req->method == HTTP_POST)
		return login_handler(req);
	if (!check_login(req))
		return ESP_OK;

	/*
	 *  handle dynamically generated pages
	 */
	if (pathlen == 7 && !strncmp(path, "/logout", 7))
		return logout_handler(req);

	/*
	 *  then static content
	 */
	return media_handler(req);
}

esp_err_t
portal_init()
{
	return ESP_OK;
}

void
portal_generate_login_token()
{
	snprintf(login_token, sizeof(login_token), "%016llx", esp_timer_get_time());
	ESP_LOGI(LOGTAG, "Generate login token %s", login_token);
}

esp_err_t
portal_enable( bool onoff )
{
	if (onoff) {
		if (httpd_handle != NULL)
			return ESP_ERR_INVALID_STATE;
		ESP_LOGI(LOGTAG, "Start portal");
		if (!login_token[0])
			portal_generate_login_token();  // for now
		httpd_config_t config = HTTPD_DEFAULT_CONFIG();
		config.max_uri_handlers = 1;
		config.uri_match_fn = httpd_uri_match_wildcard;
		ESP_ERROR_CHECK(httpd_start(&httpd_handle, &config));
		/*
		 *  register handlers from specific to wildcard
		 */
		esp_err_t rc;
		rc = httpd_register_uri_handler(httpd_handle, &base_uri);
		ESP_LOGI(LOGTAG, "Register handler %s : %s", base_uri.uri, esp_err_to_name(rc));

	} else {
		if (httpd_handle == NULL)
			return ESP_ERR_INVALID_STATE;
		ESP_LOGI(LOGTAG, "Stop portal");
		httpd_stop(httpd_handle);
		httpd_handle = NULL;
	}
	return ESP_OK;
}

