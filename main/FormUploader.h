#ifndef FORM_UPLOADER_H
#define FORM_UPLOADER_H

#ifndef UNIT_TEST
#include <esp_http_server.h>
#endif

#include <stdbool.h>

/*
 *  FormUploader - helper for digesting esp-idf httpd requests.
 */

/*
 *  context for uploading content from POSTing a form and processing.
 *  encoding should be text/plain for now (each value on a line)
 */

typedef struct form_uploader {
	httpd_req_t *req;
	bool eof;               // have we hit EOF reading document
	int used;               // already-used length of buf
	int buf_len;            // data length in buf
	char buf[256];          // partial upload data. always \0 terminated.
} FormUploader_t;

/**
 *  initialize a FormUploader_t with a request so we can process it.
 */
void formUploaderInit( FormUploader_t *ulr, httpd_req_t *req );

/**
 *  get the next key=value pair from the form upload.
 *  returns true when there is more data, else false.
 */
bool formUploaderNext( FormUploader_t *ulr, char **name, char **value );

#endif /*FORM_UPLOADER_H*/
