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

struct email_sender_context * email_sender_libcurl_init(struct email_sender_context *, void *);

struct email_sender_context * email_sender_context_init(struct email_sender_context * email, enum email_sender_user_agent agent, void * user_data)
{
	switch(agent) 
	{
	case email_sender_user_agent_default:
	case email_sender_user_agent_libcurl:
		return email_sender_libcurl_init(email, user_data);
	case email_sender_interactive:
		// TODO: 
		// return email_sender_interative_init(email, user_data);
	default:
		break;
	}
	return NULL;
}

void email_sender_context_cleanup(struct email_sender_context * email)
{
	if(NULL == email) return;
	if(email->cleanup) email->cleanup(email);
	return;
}


/* *******************************************************
 * TEST module
   ## build
   $ gcc -std=gnu99 -g -Wall -Wno-comment \
     -D_TEST_EMAIL_SENDER -D _STAND_ALONE \
     -DIMPORT_C_IMPL_FILE \
     -o email-sender \
     email-sender.c \
     -lm -lpthread -luuid -lcurl
   
   ## test  
   ## export EMAIL_SENDER_USERNAME="..."
   ## export EMAIL_SENDER_PASSWORD="..."
   ## export EMAIL_SENDER_FROM_ADDR="..."
   ## export EMAIL_SENDER_TO_ADDR="..."
   $ source [your_env.sh] 
   $ valgrind --leak-check=full ./email-sender
** ******************************************************/
#if defined(_TEST_EMAIL_SENDER) && defined(_STAND_ALONE)

#if defined(IMPORT_C_IMPL_FILE) 
#include "email-sender-libcurl.c"
#endif

int main(int argc, char **argv)
{
	curl_global_init(CURL_GLOBAL_ALL);
	int rc = 0;
	
	struct email_sender_context email[1];
	memset(email, 0, sizeof(email));
	email_sender_context_init(email, email_sender_user_agent_libcurl, NULL);
	
	const char * smtp_server = "email-smtp.ap-northeast-1.amazonaws.com";
	// load credentials
	const char * username = getenv("EMAIL_SENDER_USERNAME");
	const char * password = getenv("EMAIL_SENDER_PASSWORD");
	email->set_smtp_server(email, smtp_security_mode_default, smtp_server, 587);
	email->set_auth(email, username, password);
	
	const char * from_addr = getenv("EMAIL_SENDER_FROM_ADDR");
	const char * to_addr   = getenv("EMAIL_SENDER_TO_ADDR");
	const char * cc_addr   = NULL;
	
	if(argc > 1) to_addr = argv[1];
	if(argc > 2) cc_addr = argv[2];
	
	assert(from_addr && to_addr);
	
	const char * subject   = "test smtp send";
	char msg_id[40] = "";
	uuid_t uuid;
	uuid_generate(uuid);
	uuid_unparse(uuid, msg_id);
	
	char sz_date[100] = "";
	struct timespec ts[1];
	memset(ts, 0, sizeof(ts));
	clock_gettime(CLOCK_REALTIME, ts);
	struct tm t[1];
	memset(t, 0, sizeof(t));
	localtime_r(&ts->tv_sec, t);

	// RFC 2822-compliant  date  format
	const char * rfc_2882_date_fmt = "%a, %d %b %Y %T %z";
	ssize_t cb_date = strftime(sz_date, sizeof(sz_date), rfc_2882_date_fmt, t);
	assert(cb_date > 0);

	email->add_header(email, "Date", sz_date);
	
	email->set_to_addr(email, to_addr);
	email->set_from_addr(email, from_addr);
	email->set_cc_addr(email, cc_addr);
	
	if(msg_id[0]) email->add_header(email, "Message-ID", msg_id);
	if(subject && subject[0]) email->add_header(email, "Subject", subject);

	/* add an empty line to devide headers from body (rfc5322) */
	email->add_line(email, "%s", "\r\n");
	
	// email contents
	email->add_body(email, "hello", sizeof("hello") - 1);
	
	email_sender_context_dump(email);
	
	rc = email->send(email);
	if(rc) {
		#define email_sender_strerror(rc) curl_easy_strerror(rc)
		fprintf(stderr, "[ERROR]: email->send(): %s\n", email_sender_strerror(rc));
	}
	email->clear(email);
	
	
	email_sender_context_cleanup(email);
	curl_global_cleanup();
	return 0;
}
#endif


