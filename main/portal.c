
#include <string.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include "portal.h"
#include "NetworkManager.h"
#include "FormUploader.h"
#include "MediaFiles.h"
#include "Config.h"

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
 *  page handlers using standard tabbed view
 */
enum {
	TAB_NONE = 0,
	TAB_STATUS,
	TAB_DEVICES,
	TAB_VENTILATION,
	TAB_NETWORK,
	TAB_DIAG,
};

/**
 *  send HTTP response made of formatted string.
 *  escapes `1`, `2` etc. are replaced with contents from the args array (`1` = args[0]).
 *  to get a single backtick, use ``.
 */
static void
send_formatted_string( httpd_req_t *req, const char *fmt, int nargs, char **args )
{
	const char *p = fmt;
	const char *q;
	while (*p) {
		if (*p == '`') {
			char *endp;
			int ix = strtol(p + 1, &endp, 10);
			p = endp;
			if (*p == '`')
				++p;
			if (ix >= 1 && ix <= nargs && args[ix - 1] != NULL)
				httpd_resp_sendstr_chunk(req, args[ix - 1]);
			else
				httpd_resp_sendstr_chunk(req, "`");
		}
		for (q = p; *q && *q != '`'; ++q)
			;
		if (q > p) {
			httpd_resp_send_chunk(req, p, q - p);
			p = q;
		}
	}
}

static void
page_header( httpd_req_t *req, int tab )
{
	char *args[6] = {};

	switch (tab) {
	case TAB_STATUS:
		args[0] = "Status";
		args[1] = "id=\"ntabs\"";
		break;
	case TAB_DEVICES:
		args[0] = "Devices";
		args[2] = "id=\"ntabs\"";
		break;
	case TAB_VENTILATION:
		args[0] = "Ventilation";
		args[3] = "id=\"ntabs\"";
		break;
	case TAB_NETWORK:
		args[0] = "Network";
		args[4] = "id=\"ntabs\"";
		break;
	case TAB_DIAG:
		args[0] = "Troubleshooting";
		args[5] = "id=\"ntabs\"";
		break;
	}

	send_formatted_string(req,
			"<html>"
			"<head>"
			"<link rel=\"stylesheet\" type=\"text/css\" href=\"page.css\">"
			"<title>`1`</title>"
			"</head>"
			"<body>"
			"  <div id=\"outside\">"
			"    <div id=\"header\">"
			"    <img src=\"ac_logo.png\">"
			"    <br>"
			"    VentCommander"
			"    </div>"
			"    <div id=\"tabs\">"
			"      <table class=\"ntab\"><tr>"
			"        <td><button `2` onclick=\"location.href='status.html'\">Status</button></td>"
			"        <td><button `3` onclick=\"location.href='dev.html'\">Devices</button></td>"
			"        <td><button `4` onclick=\"location.href='vent.html'\">Ventilation</button></td>"
			"        <td><button `5` onclick=\"location.href='net.html'\">Network</button></td>"
			"        <td><button `6` onclick=\"location.href='diag.html'\">Troubleshooting</button></td>"
			"      </tr></table>"
			"    </div>"
			"    <div id=\"main\">",
		sizeof(args)/sizeof(args[0]), args);
}

static void
format_duration( char *buf, int buflen, int sec )
{
	if (sec < 60) {
		snprintf(buf, buflen, "%ds", sec);
	} else if (sec < 3600) {
		snprintf(buf, buflen, "%dm%ds", sec / 60, sec % 60);
	} else if (sec < 3600 * 24) {
		snprintf(buf, buflen, "%dh%dm", sec / 3600, sec / 60 % 60);
	} else {
		snprintf(buf, buflen, "%dd%dh", sec / 3600 / 24, sec / 3600 % 24);
	}
}

static void
page_trailer( httpd_req_t *req, int tab )
{
	char uptm[16];
	format_duration(uptm, sizeof(uptm), (int)(esp_timer_get_time() / 1000000));
	char *args[3] = { "1.0.0", uptm, "Offline" };
	send_formatted_string(req,
			"    </div>"
			"    <div id=\"footer\">"
			"      <table class=\"ftab\"><tr><td>`1`</td><td>Up `2`</td><td>`3`</td><td><a href=\"/logout\">Log Out</a></td></tr></table>"
			"    </div>"
			"  </div>"
			"</body>"
			"</html>",
		sizeof(args)/sizeof(args[0]), args);
	httpd_resp_send_chunk(req, NULL, 0);
}

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

static esp_err_t
status_handler( httpd_req_t *req )
{
	httpd_resp_sendstr_chunk(req, "STATUS STUFF");
	return ESP_OK;
}

static esp_err_t
devices_handler( httpd_req_t *req )
{
	httpd_resp_sendstr_chunk(req,
		"<button onclick=\"fetch('/action?adddevice');\">Add Device</button>"
	);
	return ESP_OK;
}

static esp_err_t
ventilation_handler( httpd_req_t *req )
{
	httpd_resp_sendstr_chunk(req, "STUFF");
	return ESP_OK;
}

static esp_err_t
chain_to_network( httpd_req_t *req )
{
	httpd_resp_sendstr(req,
		"<html>"
		"<script>"
		"window.onload = function () {"
		"  window.location = window.location.protocol + '//' + window.location.host + '/net.html';"
		"}"
		"</script>"
		"</html>");
	return ESP_OK;
}

static bool askscan = false;    // signal from form handler to network page, this is a bit of a hack

static esp_err_t
wifi_handler( httpd_req_t *req )
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
			ESP_LOGI(LOGTAG, "Post: %s=%s", name, value);
			if (!strcmp("scan", name))
				askscan = true;
			else if (!strcmp("ssid", name))
				config_set_wifi_ssid(value);
			else if (!strcmp("pass", name))
				config_set_wifi_pw(value);
		}
	}
	return chain_to_network(req);
}

static esp_err_t
wifi_erase_handler( httpd_req_t *req )
{
	config_set_wifi_ssid("");
	return chain_to_network(req);
}

static esp_err_t
network_handler( httpd_req_t *req )
{
	if (config_get_wifi_ssid()[0]) {
		char *args[3];
		args[0] = (char *)config_get_wifi_ssid();
		args[1] = (char *)network_get_sta_ip();
		args[2] = (char *)get_base_mac();
		send_formatted_string(req,
			"<table>"
			"<tr><td>SSID</td><td>`1`</td></tr>"
			"<tr><td>IP Address</td><td>`2`</td></tr>"
			"<tr><td>MAC Address</td><td>`3`</td></tr>"
			"</table>"
			"<button onclick=\"location.href='wifi-erase'\">Clear Network</button>",
			sizeof(args)/sizeof(args[0]), args);

	} else {
		httpd_resp_sendstr_chunk(req,
			"<form method=\"post\" action=\"/wifi\">"
			"<table>"
		);
		if (askscan) {
			askscan = false;
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
	}
	return ESP_OK;
}

static esp_err_t
diag_handler( httpd_req_t *req )
{
	httpd_resp_sendstr_chunk(req,
		"AP Diagnostics:"
		"<button onclick=\"fetch('/action?apdown');\">Shut Down</button>"
		"<button onclick=\"fetch('/action?portal');\">Web Portal</button>"
	);
	return ESP_OK;
}

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
		path = "/status.html";
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
action_handler( httpd_req_t *req )
{
	const char *query = strchr(req->uri, '?');
	query = (query != NULL) ? query + 1 : "";
	ESP_LOGI(LOGTAG, "Action: %s", query);

	if (!strcmp(query, "apdown")) {
		network_set_ap_mode(AP_MODE_NONE);
	} else if (!strcmp(query, "portal")) {
		network_set_ap_mode(AP_MODE_PORTAL);
	} else if (!strcmp(query, "adddevice")) {
		network_set_ap_mode(AP_MODE_PAIRING);
	}
	httpd_resp_send(req, "", 0);
	return ESP_OK;
}

struct pagehandler {
	int tab;                                    // tabbed page format, or TAB_NONE
	int method;                                 // HTTP_method allowed (not HTTP_ANY)
	const char *uri;                            // uri to match
	esp_err_t (*handler)( httpd_req_t *req );   // request handler function
} dynamic_pages[] = {
	{ TAB_STATUS,      HTTP_GET, "/status.html", status_handler },
	{ TAB_DEVICES,     HTTP_GET, "/dev.html", devices_handler },
	{ TAB_VENTILATION, HTTP_GET, "/vent.html", ventilation_handler },
	{ TAB_NETWORK,     HTTP_GET, "/net.html", network_handler },
	{ TAB_DIAG,        HTTP_GET, "/diag.html", diag_handler },

	{ TAB_NONE,        HTTP_POST, "/wifi", wifi_handler },
	{ TAB_NONE,        HTTP_GET,  "/wifi-erase", wifi_erase_handler },
	{ TAB_NONE,        HTTP_GET,  "/logout", logout_handler },
	{ TAB_NONE,        HTTP_GET,  "/action", action_handler },
};

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
	 *  handle dynamically generated tabbed pages
	 */
	for (int i = 0; i < sizeof(dynamic_pages)/sizeof(dynamic_pages[0]); i++) {
		if (req->method == dynamic_pages[i].method) {
			int len = strlen(dynamic_pages[i].uri);
			if (pathlen == len && !strncmp(path, dynamic_pages[i].uri, len)) {
				if (dynamic_pages[i].tab != TAB_NONE)
					page_header(req, dynamic_pages[i].tab);
				dynamic_pages[i].handler(req);
				if (dynamic_pages[i].tab != TAB_NONE)
					page_trailer(req, dynamic_pages[i].tab);
				return ESP_OK;
			}
		}
	}

	/*
	 *  handle stuff like forms
	 */
/*
	if (pathlen == 5 && !strncmp(path, "/wifi", 5) && req->method == HTTP_POST)
	   return wifi_handler(req);
	if (pathlen == 11 && !strncmp(path, "/wifi-erase", 11))
	   return wifi_erase_handler(req);
	if (pathlen == 7 && !strncmp(path, "/logout", 7))
	   return logout_handler(req);
*/

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

