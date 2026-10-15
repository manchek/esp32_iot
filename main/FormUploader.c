
/*
 *  mocks for testing
 */

#ifdef UNIT_TEST
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

typedef int esp_err_t;

struct httpd_req_s;
typedef int (*recv_function)( struct httpd_req_s *r, char *buf, size_t buf_len );

typedef struct httpd_req_s {
	// public mock fields
	size_t content_len;

	// for our mocking use
	const char *document;
	int posn;
	recv_function recv;
} httpd_req_t;

int httpd_req_recv_asmuch( httpd_req_t *req, char *buf, size_t buflen )
{
	if (req->posn < req->content_len) {
		int n = req->content_len - req->posn;
		if (buflen < n)
			n = buflen;
		memcpy(buf, req->document + req->posn, n);
		req->posn += n;
		return n;
	}
	return 0;
}

int httpd_req_recv_one( httpd_req_t *req, char *buf, size_t buflen )
{
	if (req->posn < req->content_len) {
		int n = 1;
		if (buflen < n)
			n = buflen;
		memcpy(buf, req->document + req->posn, n);
		req->posn += n;
		return n;
	}
	return 0;
}

int httpd_req_recv( httpd_req_t *req, char *buf, size_t buflen )
{
	return req->recv(req, buf, buflen);
}

#endif /*UNIT_TEST*/

#include "FormUploader.h"

#include <string.h>

void formUploaderInit( FormUploader_t *ulr, httpd_req_t *req )
{
	memset(ulr, 0, sizeof(*ulr));
	ulr->req = req;
}

bool formUploaderNext( FormUploader_t *ulr, char **name, char **value )
{
	if (ulr->used > 0) {
		ulr->buf_len -= ulr->used;
		if (ulr->buf_len > 0)
			memmove(ulr->buf, ulr->buf + ulr->used, ulr->buf_len);
		ulr->buf[ulr->buf_len] = 0;
		ulr->used = 0;
//printf("remove used, buf:\"%s\"\n", ulr->buf);
	}

	for (; ; ) {
		char *eol;

		/* process any complete lines in buf */
		while (ulr->buf_len > 0 && (eol = strchr(ulr->buf, '\n')) != NULL) {
			*eol++ = 0;
			char *eq = strchr(ulr->buf, '=');
			if (eq != NULL) {
//printf("eol %d eq %d\n", eol - ulr->buf, eq - ulr->buf);
				*eq = 0;
				*name = ulr->buf;
				*value = eq + 1;
				ulr->used = eol - ulr->buf;
				return true;
			}
			/* if this line was broken (no =) skip past it */
			ulr->buf_len -= eol - ulr->buf;
			if (ulr->buf_len > 0)
				memmove(ulr->buf, eol, ulr->buf_len);
			ulr->buf[ulr->buf_len] = 0;
//printf("buf \"%s\"\n", ulr->buf);
		}

		if (ulr->eof)
			return false;

		/* refill buf */
		int n = httpd_req_recv(ulr->req, ulr->buf + ulr->buf_len, sizeof(ulr->buf) - ulr->buf_len - 1);
//printf("recv %d : %d\n", sizeof(ulr->buf) - ulr->buf_len - 1, n);
		if (n > 0) {
			ulr->buf_len += n;
			ulr->buf[ulr->buf_len] = 0;
		} else {
			ulr->eof = true;
		}
//printf("refill buf:\"%s\"\n", ulr->buf);
	}
	return false;
}

#ifdef UNIT_TEST

struct testcase {
	const char *name;
	const char *document;
	recv_function recv;
} tests[] = {
	{ "Empty", "" },
	{ "Nothing useful", "Garbage" },
	{ "Incomplete line", "=" },
	{ "Incomplete line", "k=" },
	{ "Incomplete line", "=v" },
	{ "Blank lines", "\n\n\n\n" },
	{ "Line of garbage", "Garbage\n" },
	{ "Missing key and value", "=\n" },
	{ "Missing value", "k=\n" },
	{ "Missing key", "=v\n" },
	{ "Value with =", "key=value=with=equals\n" },
	{ "One value", "KEY=VALUE\n" },
	{ "Two values", "a=1\nb=2\n" },
	{ "More values", "one=1\nhundred=100\nwords=apple pie\n" },
	{ "Reading single chars", "a=1\nb=2\n", httpd_req_recv_one },
};

int
main( int argc, char **argv )
{
	int ntests = sizeof(tests)/sizeof(tests[0]);
	for (int i = 0; i < ntests; i++) {
		char banner[81];
		memset(banner, '-', sizeof(banner));
		banner[sizeof(banner)-1] = 0;
		snprintf(banner, sizeof(banner)-3, "Test %d : %s", i, tests[i].name);
		banner[strlen(banner)] = ' ';
		printf("%s\n", banner);
		printf(" Document: <");
		int n = strlen(tests[i].document);
		for (int j = 0; j < n; j++) {
			char c = tests[i].document[j];
			if (c == '\n')
				printf("\\n");
			else
				printf("%c", c);
		}
		printf(">\n");

		httpd_req_t req;
		req.document = tests[i].document;
		req.content_len = strlen(req.document);
		req.posn = 0;
		req.recv = tests[i].recv ? tests[i].recv : httpd_req_recv_asmuch;

		FormUploader_t ulr;
		formUploaderInit(&ulr, &req);
		char *name;
		char *value;
		while (formUploaderNext(&ulr, &name, &value)) {
			printf(" Form field: key:\"%s\" value:\"%s\"\n", name, value);
		}
	}
}
#endif /*UNIT_TEST*/
