
/*
 *  FormUploader - helper for digesting esp-idf httpd requests.
 */

#ifdef UNIT_TEST
/*
 *  mocks for testing
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

typedef int esp_err_t;

int verbose = 1;

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
#endif /*UNIT_TEST*/

#include "FormUploader.h"

#include <string.h>

enum {
	ENCODING_NONE,
	ENCODING_TEXT_PLAIN,    // text/plain
	ENCODING_URL            // application/x-www-form-urlencoded
};

void formUploaderInit( FormUploader_t *ulr, httpd_req_t *req, const char *enc )
{
	memset(ulr, 0, sizeof(*ulr));
	ulr->req = req;
	ulr->enc = (enc != NULL && !strcmp("text/plain", enc)) ? ENCODING_TEXT_PLAIN : ENCODING_URL;
}

static bool formUploaderNextPlain( FormUploader_t *ulr, char **name, char **value )
{
	for (; ; ) {
		char *eol;

		/* process any complete lines in buf */
		while (ulr->buf_len > 0 && (eol = strchr(ulr->buf, '\n')) != NULL) {
			if (eol > ulr->buf && *(eol - 1) == '\r')
				*(eol - 1) = 0;
			*eol++ = 0;
			if (ulr->buf[0]) {
				char *eq = strchr(ulr->buf, '=');
				if (eq != NULL) {
//printf("eol %d eq %d\n", eol - ulr->buf, eq - ulr->buf);
					*eq++ = 0;
					*name = ulr->buf;
					*value = eq;
				} else {
					*name = ulr->buf;
					*value = eol - 1;
				}
				ulr->used = eol - ulr->buf;
				return true;
			}
			/* if this line was empty skip past it */
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

static int hexvalue( char c )
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int urldecode( char *in, char *out )
{
	int d1, d2;
	char *wp = out;
	for (; *in; ++in) {
		switch (*in) {
		case '+':
			*wp++ = ' ';
			break;
		case '%':
			if (*++in && (d1 = hexvalue(*in)) >= 0) {
				if (*++in && (d2 = hexvalue(*in)) >= 0) {
					*wp++ = (d1 << 4) | d2;
				} else {
					in -= 2;
					*wp++ = *in;
				}
			} else {
				--in;
				*wp++ = *in;
			}
			break;
		default:
			*wp++ = *in;
			break;
		}
	}
	*wp = 0;
	return wp - out;
}

static bool formUploaderNextUrl( FormUploader_t *ulr, char **name, char **value )
{
	for (; ; ) {
		char *eop;

		/*
		 *  process any complete segment in buf ending with '&'.
		 *  if we hit eof it's whatever is left.
		*/
		while (ulr->buf_len > 0 && ((eop = strchr(ulr->buf, '&')) != NULL || ulr->eof)) {
			if (eop != NULL)
				*eop++ = 0;
			else
				eop = ulr->buf + ulr->buf_len;
			if (ulr->buf[0]) {
				char *eq = strchr(ulr->buf, '=');
				if (eq != NULL) {
					*eq++ = 0;
					urldecode(ulr->buf, ulr->buf);
					urldecode(eq, eq);
					*name = ulr->buf;
					*value = eq;
				} else {
					*name = ulr->buf;
					*value = "";
				}
				ulr->used = eop - ulr->buf;
				return true;
			}
			/* if this pair was broken (no =) skip past it */
			ulr->buf_len -= eop - ulr->buf;
			if (ulr->buf_len > 0)
				memmove(ulr->buf, eop, ulr->buf_len);
			ulr->buf[ulr->buf_len] = 0;
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

	if (ulr->enc == ENCODING_URL)
		return formUploaderNextUrl(ulr, name, value);
	return formUploaderNextPlain(ulr, name, value);
}

#ifdef UNIT_TEST

#include <stdlib.h>
#include <getopt.h>

struct testcase {
	const char *name;
	const char *document;
	const char *encoding;
	recv_function recv;         // optional alternate function to simulate receive behavior
	int exp_count;              // expected number of fields or -1 if unknown
} tests[] = {
	{ "Empty", "", "text/plain", NULL, 0 },
	{ "Blank lines", "\r\n\r\n\r\n\r\n", "text/plain", NULL, 0 },
	{ "Nothing useful", "Garbage", "text/plain", NULL, 0 },
	{ "Incomplete line", "=", "text/plain", NULL, 0 },
	{ "Incomplete line", "k=", "text/plain", NULL, 0 },
	{ "Incomplete line", "=v", "text/plain", NULL, 0 },
	{ "No Value", "Name\r\n", "text/plain", NULL, 1 },
	{ "Either Line Ending", "Name1\r\nName2=2\n", "text/plain", NULL, 2 },
	{ "Trailing Garbage", "Name\r\nName2\nAnd more", "text/plain", NULL, 2 },
	{ "Missing key and value", "=\r\n", "text/plain", NULL, 1 },
	{ "Missing value", "KEY=\r\n", "text/plain", NULL, 1 },
	{ "Missing key", "=VALUE\r\n", "text/plain", NULL, 1 },
	{ "One value", "KEY=VALUE\r\n", "text/plain", NULL, 1 },
	{ "Two values", "a=1\r\nb=2\r\n", "text/plain", NULL, 2 },
	{ "More values", "one=1\r\nhundred=100\r\nwords=apple pie\r\n", "text/plain", NULL, 3 },
	{ "Value with =", "key=value=with=equals\r\n", "text/plain", NULL, 1 },
	{ "Reading single chars", "a=1\r\nb=2\r\n", "text/plain", httpd_req_recv_one, 2 },

	{ "Missing key and value", "=", "application/x-www-form-urlencoded", NULL, 1 },
	{ "Missing value", "k=", "application/x-www-form-urlencoded", NULL, 1 },
	{ "Missing key", "=v", "application/x-www-form-urlencoded", NULL, 1 },
	{ "Empty", "", "application/x-www-form-urlencoded", NULL, 0 },
	{ "Just a Key", "KEY", "application/x-www-form-urlencoded", NULL, 1 },
	{ "Just Keys", "KEY1&KEY2&KEY3", "application/x-www-form-urlencoded", NULL, 3 },
	{ "Segments Missing Parts", "&k1=v1&=&=v2&&k3=&&k4=v4&", "application/x-www-form-urlencoded", NULL, 5 },
	{ "Value with =", "key=value=with=equals", "application/x-www-form-urlencoded", NULL, 1 },
	{ "One value", "KEY=VALUE", "application/x-www-form-urlencoded", NULL, 1 },
	{ "Two values", "a=1&b=2", "application/x-www-form-urlencoded", NULL, 2 },
	{ "More values", "one=1&hundred=100&words=apple pie", "application/x-www-form-urlencoded", NULL, 3 },
	{ "Reading single chars", "a=1&b=2", "application/x-www-form-urlencoded", httpd_req_recv_one, 2 },
	{ "Decoding", "key=x+y%20%29%2a%2A%2f%2F&key2=%22value+2%22&key3=a+%3D+b&key4=", "application/x-www-form-urlencoded", httpd_req_recv_one, 4 },
	{ "Broken Percent then Percent", "k1=%&k2=%2&k3=%2g&k4=%%21&k5=%2%21&k6=%2g%21", "application/x-www-form-urlencoded", httpd_req_recv_one, 6 },
	{ "Broken Percent then Plus", "k1=%+%21&k2=%2+%21&k3=%2g+%21", "application/x-www-form-urlencoded", httpd_req_recv_one, 3 },
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
		formUploaderInit(&ulr, &req, tests[i].encoding);
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
