/*
 * email-sender.c
 * 
 * Copyright 2021 chehw <hongwei.che@gmail.com>
 * 
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of 
 * this software and associated documentation files (the "Software"), to deal in 
 * the Software without restriction, including without limitation the rights to 
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
 * of the Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include <uuid/uuid.h>
#include <curl/curl.h>
#include "email-sender.h"

#ifndef debug_printf
#include <stdarg.h>

#ifdef _DEBUG
#define debug_printf fprintf
#else
#define debug_printf(...) do {} while(0)
#endif
#endif

/***************************************
 * struct string_buf
***************************************/
struct string_buf
{
	size_t max_size;
	size_t length;
	char data[0];
};
struct string_buf * string_buf_new(size_t max_size, const void * data, size_t length)
{
	if(0 == max_size) max_size = length;
	assert(max_size >= length);
	
	struct string_buf * sbuf = calloc(sizeof(*sbuf) + max_size, 1);
	assert(sbuf);
	sbuf->max_size = max_size;
	sbuf->length = length;
	
	if(length && data) {
		memcpy(sbuf->data, data, length);
	}
	return sbuf;
}
#define string_buf_free(sbuf) free(sbuf)

struct string_buf * string_buf_resize(struct string_buf * sbuf, size_t new_size)
{
	sbuf = realloc(sbuf, sizeof(*sbuf) + new_size);
	if(NULL == sbuf) return NULL;
	assert(sbuf);
	
	sbuf->max_size = new_size;
	if(sbuf->length > new_size) sbuf->length = new_size;
	return sbuf;
}


static inline int string_buf_append_crlf(struct string_buf * sbuf)
{
	char * p_end = sbuf->data + sbuf->length;
	assert(sbuf->max_size >= 3);
	
	if(0 == sbuf->length) {
		sbuf->data[0] = '\r';
		sbuf->data[1] = '\n';
		sbuf->data[2] = '\0';
		sbuf->length += 2;
		return 0;
	}
	
	if(sbuf->length >= 2) {
		if(p_end[-1] == '\n' && p_end[-2] == '\r') return 0; // no need to append
		if(p_end[-1] == '\n') --p_end;
	}
	if((p_end - sbuf->data + 3) > sbuf->max_size) return -1;
	
	*p_end++ = '\r'; 
	*p_end++ = '\n';
	*p_end = '\0';
	sbuf->length = p_end - sbuf->data;
	return 0;
}

/***************************************
 * struct email_private
***************************************/
/*
 * rfc4616 The PLAIN SASL Mechanism
 * https://www.ietf.org/rfc/rfc4616.txt
*/
#define SASL_PLAIN_MAX_KEY_SIZE (256) 
typedef struct email_private 
{
	struct email_sender_context * email;
	CURL * curl;
	enum smtp_security_mode mode;
	char url[PATH_MAX];
	char username[SASL_PLAIN_MAX_KEY_SIZE];
	char password[SASL_PLAIN_MAX_KEY_SIZE];
	
	int err_code;
	char err_msg[PATH_MAX];
	
	// payload
	size_t max_lines;
	size_t num_lines;
	struct string_buf ** lines;
	size_t cur_index;
	
#define MAX_EMAIL_ADDR_LENGTH (1024)
	char from_addr[MAX_EMAIL_ADDR_LENGTH];
	char to_addr[MAX_EMAIL_ADDR_LENGTH];
	char cc_addr[MAX_EMAIL_ADDR_LENGTH];
#undef MAX_EMAIL_ADDR_LENGTH
} email_private_t;
static email_private_t * email_private_new(struct email_sender_context * email)
{
	assert(email);
	
	email_private_t * priv = calloc(1, sizeof(*priv));
	assert(priv);
	priv->email = email;
	email->priv = priv;
	
	CURL * curl = curl_easy_init();
	assert(curl);
	
	priv->curl = curl;
	return priv;
}

static void email_private_clear(email_private_t * priv)
{
	if(NULL == priv) return;
	if(priv->lines) {
		for(int i = 0; i < priv->num_lines; ++i) {
			string_buf_free(priv->lines[i]);
			priv->lines[i] = NULL;
		}
	}
	priv->num_lines = 0;
	priv->cur_index = 0;
	return;
}

static void email_private_free(email_private_t * priv)
{
	if(NULL == priv) return;
	if(priv->lines) {
		email_private_clear(priv);
		free(priv->lines);
		priv->lines = NULL;
	}
	priv->max_lines = 0;
	priv->num_lines = 0;
	
	if(priv->curl) {
		curl_easy_cleanup(priv->curl);
		priv->curl = NULL;
	}
	
	// clear secrets
	memset(priv->username, 0, sizeof(priv->username));
	memset(priv->password, 0, sizeof(priv->password));
	
	free(priv);
	return;
}


#define LINES_ARRAY_ALLOC_SIZE (1024)
static inline int email_private_lines_array_resize(email_private_t * priv, int max_lines)
{
	assert(priv);
	if(max_lines == 0) max_lines = LINES_ARRAY_ALLOC_SIZE;
	if(max_lines <= priv->max_lines) return 0;
	
	max_lines = (max_lines + LINES_ARRAY_ALLOC_SIZE - 1) / LINES_ARRAY_ALLOC_SIZE * LINES_ARRAY_ALLOC_SIZE;
	
	struct string_buf ** lines = realloc(priv->lines, sizeof(*lines) * max_lines);
	assert(lines);
	
	memset(lines + priv->max_lines, 0, sizeof(*lines) * (max_lines - priv->max_lines));
	priv->lines = lines;
	priv->max_lines = max_lines;
	return 0;
}
#undef LINES_ARRAY_ALLOC_SIZE


/***************************************
 * struct email_sender_context
***************************************/
static int email_set_from_addr(struct email_sender_context * email, const char * from_addr)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	priv->from_addr[0] = '\0';
	
	if(NULL == from_addr) return 0;
	strncpy(priv->from_addr, from_addr, sizeof(priv->from_addr));
	return email->add_header(email, "From", priv->from_addr);
}
static int email_set_to_addr(struct email_sender_context * email, const char * to_addr)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	priv->to_addr[0] = '\0';
	
	if(NULL == to_addr) return 0;
	strncpy(priv->to_addr, to_addr, sizeof(priv->to_addr));
	return email->add_header(email, "To", priv->to_addr);
}
static int email_set_cc_addr(struct email_sender_context * email, const char * cc_addr)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	priv->cc_addr[0] = '\0';
	
	if(NULL == cc_addr) return 0;
	strncpy(priv->cc_addr, cc_addr, sizeof(priv->cc_addr));
	return email->add_header(email, "Cc", priv->cc_addr);
}

static int email_add_line(struct email_sender_context * email, const char * fmt, ...)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	
	char buf[4096] = "";
	va_list args;
	va_start(args, fmt);
	size_t cb = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	
	if(cb <= 0 || cb == sizeof(buf)) return -1;
	
	int rc = email_private_lines_array_resize(priv, priv->num_lines + 1);
	assert(0 == rc);
	
	struct string_buf * line = string_buf_new(cb + 3, buf, cb);	// append "\r\n\0" if needed
	assert(line);
	
	rc = string_buf_append_crlf(line);
	assert(0 == rc);
	
	priv->lines[priv->num_lines++] = line;
	return 0;
}

static int email_add_body(struct email_sender_context * email, const char * body, size_t cb_body)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	
	if(NULL == body || 0 == cb_body) return -1;
	int rc = email_private_lines_array_resize(priv, priv->num_lines + 1);
	assert(0 == rc);
	
	struct string_buf * line = string_buf_new(cb_body + 3, body, cb_body);	// append "\r\n\0" if needed
	assert(line);
	
	rc = string_buf_append_crlf(line);
	assert(0 == rc);
	
	priv->lines[priv->num_lines++] = line;
	return 0;
}
static int email_add_header(struct email_sender_context * email, const char * key, const char * value)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	
	if(NULL == key) return -1;
	int rc = email_private_lines_array_resize(priv, priv->num_lines + 1);
	assert(0 == rc);
	
	int cb_key = strlen(key);
	int cb_value = 0;
	if(value) cb_value = strlen(value);
	
	// key: value\r\n
	struct string_buf * line = string_buf_new(cb_key + 2 + cb_value + 3, NULL, 0);
	assert(line);
	
	char * p = line->data;
	memcpy(p, key, cb_key); p += cb_key;
	*p++ = ':'; *p++ = ' ';
	if(cb_value > 0) {
		memcpy(p, value, cb_value);
		p += cb_value;
	}
	line->length = p - line->data;
	rc = string_buf_append_crlf(line);
	assert(0 == rc);
	
	priv->lines[priv->num_lines++] = line;
	return 0;
}

static void email_clear(struct email_sender_context * email)
{
	assert(email && email->priv);
	email_private_clear(email->priv);
	return;
}


static int email_set_smtp_server(struct email_sender_context * email, enum smtp_security_mode mode, const char * server_name, unsigned int port)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	static const char * s_protocols[] = {
		"smtp",
		"smtps",
	};
	
	switch(mode) 
	{
	case smtp_security_mode_default:
	case smtp_security_mode_ssl:
	case smtp_security_mode_force_tls:
		priv->mode = CURLUSESSL_ALL;
		break;
	case smtp_security_mode_try_tls:
		priv->mode = CURLUSESSL_TRY;
		break;
	default:
		priv->mode = CURLUSESSL_NONE;
	}
	
	const char * protocol = s_protocols[(mode == smtp_security_mode_ssl)];
	if(0 == port) {
		switch(mode)
		{
		case smtp_security_mode_ssl: 
			port = 465; break;
		default:
			port = 587; 	// default mail submission port
		}
	}
	int cb = snprintf(priv->url, sizeof(priv->url), "%s://%s:%u", protocol, server_name, port);
	assert(cb > 0);
	
	return 0;
}
static int email_set_auth(struct email_sender_context * email, const char * username, const char * password)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	if(username) strncpy(priv->username, username, sizeof(priv->username));
	if(password) strncpy(priv->password, password, sizeof(priv->password));
	return 0;
}

static size_t on_read_data(char * ptr, size_t size, size_t n, void * user_data)
{
	size_t cb = size * n;
	if(cb == 0) return 0;
	
	email_private_t * priv = user_data;
	assert(priv);
	
	if(priv->cur_index >= priv->num_lines) return 0;
	struct string_buf * sbuf = priv->lines[priv->cur_index++];
	if(sbuf->length > cb) return 0;	// not enough memory
	
	debug_printf(stderr, "send line: %s", sbuf->data);
	memcpy(ptr, sbuf->data, sbuf->length);
	return sbuf->length;
}
static int email_send(struct email_sender_context * email)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	CURL * curl = priv->curl;
	if(NULL == curl) {
		curl = curl_easy_init();
		assert(curl);
		priv->curl = curl;
	}else {
		curl_easy_reset(curl);
	}
	
	// clear last error
	priv->err_code = 0;
	priv->err_msg[0] = '\0';
	
	CURLcode ret = CURLE_UNKNOWN_OPTION;
	ret = curl_easy_setopt(curl, CURLOPT_USERNAME, priv->username);
	ret = curl_easy_setopt(curl, CURLOPT_PASSWORD, priv->password);
	
	ret = curl_easy_setopt(curl, CURLOPT_URL, priv->url);
	ret = curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)priv->mode);
	ret = curl_easy_setopt(curl, CURLOPT_MAIL_FROM, priv->from_addr);
	
	struct curl_slist * recipients = NULL;
	recipients = curl_slist_append(recipients, priv->to_addr);
	if(priv->cc_addr[0]) recipients = curl_slist_append(recipients, priv->cc_addr);
	ret = curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
	
	ret = curl_easy_setopt(curl, CURLOPT_READFUNCTION, on_read_data);
	ret = curl_easy_setopt(curl, CURLOPT_READDATA, priv);
	ret = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	ret = curl_easy_perform(curl);
	priv->err_code = ret;
	curl_slist_free_all(recipients);

	return priv->err_code;
}


void email_cleanup(struct email_sender_context * email)
{
	if(NULL == email) return;
	email_private_free(email->priv);
	email->priv = NULL;
	return;
}

struct email_sender_context * email_sender_libcurl_init(struct email_sender_context * email, void * user_data)
{
	if(NULL == email) email = calloc(1, sizeof(*email));
	assert(email);
	email->user_data = user_data;
	
	email->set_from_addr = email_set_from_addr;
	email->set_to_addr = email_set_to_addr;
	email->set_cc_addr = email_set_cc_addr;
	
	email->add_line = email_add_line;
	email->add_body = email_add_body;
	email->add_header = email_add_header;
	email->clear = email_clear;
	email->set_smtp_server = email_set_smtp_server;
	email->set_auth = email_set_auth;
	email->send = email_send;
	email->cleanup = email_cleanup;
	
	email_private_t * priv = email_private_new(email);
	assert(priv && email->priv == priv);
	
	return email;
}

void email_sender_context_dump(const struct email_sender_context * email)
{
	assert(email && email->priv);
	email_private_t * priv = email->priv;
	
	fprintf(stderr, "url: %s\n", priv->url);
	fprintf(stderr, "mode: %d\n", priv->mode);
	fprintf(stderr, "username: %s\n", priv->username);
	fprintf(stderr, "password: %s\n", priv->password);
	
	fprintf(stderr, "== %s(): num_lines: %d\n", __FUNCTION__, (int)priv->num_lines);
	if(priv->lines) {
		for(int i = 0; i < priv->num_lines; ++i) {
			struct string_buf * sbuf = priv->lines[i];
			fprintf(stderr, "%.*s", (int)sbuf->length, sbuf->data);
		}
	}
	return;
}

