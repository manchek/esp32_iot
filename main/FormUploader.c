
/*
 *  mocks for testing
 */

#ifdef UNIT_TEST
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

typedef int esp_err_t;

/*
 *  just enough of an http request to mock
 */
struct httpd_req_s;

typedef int (*recv_function)( struct httpd_req_s *r, char *buf, size_t buf_len );

typedef struct httpd_req_s {
	// public mock fields
	size_t content_len;         // length of document

	// for our mocking use
	const char *document;       // upload document
	int posn;                   // read position
	recv_function recv;         // for different recv function behavior
} httpd_req_t;

/** read as much as client requests */
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

/** read a single byte at a time */
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

/** mock recv function where we can control the behavior */
int httpd_req_recv( httpd_req_t *req, char *buf, size_t buflen )
{
	return req->recv(req, buf, buflen);
}

int verbose = 1;

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
			if (eol > ulr->buf && *(eol - 1) == '\r')
				*(eol - 1) = 0;
			*eol++ = 0;
			char *eq = strchr(ulr->buf, '=');
			if (eq != NULL && eq > ulr->buf && *(eq + 1)) {
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

#include <stdlib.h>
#include <getopt.h>

struct testcase {
	const char *name;
	const char *document;
	recv_function recv;
	int exp_count;
} tests[] = {
	{ "Empty", "", NULL, 0 },
	{ "Nothing useful", "Garbage", NULL, 0 },
	{ "Nothing useful", "Garbage\r\nand more garbage\nand more", NULL, 0 },
	{ "Incomplete line", "=", NULL, 0 },
	{ "Incomplete line", "k=", NULL, 0 },
	{ "Incomplete line", "=v", NULL, 0 },
	{ "Blank lines", "\r\n\r\n\r\n\r\n", NULL, 0 },
	{ "Line of garbage", "Garbage\r\n", NULL, 0 },
	{ "Missing key and value", "=\r\n", NULL, 0 },
	{ "Missing value", "k=\r\n", NULL, 0 },
	{ "Missing key", "=v\r\n", NULL, 0 },
	{ "Value with =", "key=value=with=equals\r\n", NULL, 1 },
	{ "One value", "KEY=VALUE\r\n", NULL, 1 },
	{ "Two values", "a=1\r\nb=2\r\n", NULL, 2 },
	{ "More values", "one=1\r\nhundred=100\r\nwords=apple pie\r\n", NULL, 3 },
	{ "Reading single chars", "a=1\r\nb=2\r\n", httpd_req_recv_one, 2 },
};

int
main( int argc, char **argv )
{
	int errors = 0;
	int c;

	while ((c = getopt(argc, argv, "qv")) != EOF) {
		switch (c) {
		case 'q':
			--verbose;
			break;
		case 'v':
			++verbose;
			break;
		default:
			++errors;
			break;
		}
	}

	int ntests = sizeof(tests)/sizeof(tests[0]);
	for (int i = 0; i < ntests; i++) {
		if (verbose > 0) {
			char banner[81];
			memset(banner, '-', sizeof(banner));
			banner[sizeof(banner)-1] = 0;
			snprintf(banner, sizeof(banner)-3, "Test %d : %s", i, tests[i].name);
			banner[strlen(banner)] = ' ';
			printf("%s\n", banner);
		}
		if (verbose > 1) {
			printf(" Document: <");
			int n = strlen(tests[i].document);
			for (int j = 0; j < n; j++) {
				char c = tests[i].document[j];
				if (c == '\n')
					printf("\\n");
				else if (c == '\r')
					printf("\\r");
				else
					printf("%c", c);
			}
			printf(">\n");
		}

		httpd_req_t req;
		req.document = tests[i].document;
		req.content_len = strlen(req.document);
		req.posn = 0;
		req.recv = tests[i].recv ? tests[i].recv : httpd_req_recv_asmuch;

		FormUploader_t ulr;
		formUploaderInit(&ulr, &req);
		char *name;
		char *value;
		int nfields = 0;
		while (formUploaderNext(&ulr, &name, &value)) {
			++nfields;
			if (verbose > 0)
				printf(" Form field: key:\"%s\" value:\"%s\"\n", name, value);
		}
		if (tests[i].exp_count >= 0 && nfields != tests[i].exp_count) {
			fprintf(stderr, "Test %d : %s : Expected %d fields, got %d\n", i, tests[i].name, tests[i].exp_count, nfields);
			++errors;
		}
	}
	if (errors > 0) {
		fprintf(stderr, "Tests: %d Errors: %d\n", ntests, errors);
	} else if (verbose > 0) {
		printf("Tests: %d\n", ntests);
	}
	exit(errors > 0 ? 1 : 0);
}
#endif /*UNIT_TEST*/
