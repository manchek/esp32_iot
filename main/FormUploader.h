#ifndef FORM_UPLOADER_H
#define FORM_UPLOADER_H

/*
 *  FormUploader - helper for digesting esp-idf httpd requests.
 *
 *  we read the upload document body and parse it according to the given encoding.
 *  key=value pairs can be extracted by repeatedly calling formUploaderNext().
 */

#ifndef UNIT_TEST
#include <esp_http_server.h>
#endif

#include <stdbool.h>

/*
 *  context for uploading content from POSTing a form and processing.
 */

typedef struct FormUploader {
	httpd_req_t *req;
	bool eof;               // have we hit EOF reading document
	uint8_t enc;            // encoding
	int used;               // already-used length of buf
	int buf_len;            // data length in buf
	char buf[256];          // partial upload data. always \0 terminated.
} FormUploader_t;

/**
 *  initialize a FormUploader_t with a request so we can process it.
 *  encoding can be hardcoded or extracted from the Content-type header.
 *  supported encodings are:
 *    application/x-www-form-urlencoded
 *    text/plain
 */
void formUploaderInit( FormUploader_t *ulr, httpd_req_t *req, const char *enc );

/**
 *  get the next key=value pair from the form upload.
 *  returns true when there is more data, else false.
 *  when true, name and value return pointing to \0 terminated strings (possibly empty).
 */
bool formUploaderNext( FormUploader_t *ulr, char **name, char **value );

#endif /*FORM_UPLOADER_H*/
