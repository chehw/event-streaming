/*
 * test-email-sender.c
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

#include <json-c/json.h>
#include "email-sender.h"

#include "utils.h"
#include <curl/curl.h>

int main(int argc, char **argv)
{
	curl_global_init(CURL_GLOBAL_ALL);
	int rc = 0;
	struct email_sender_context * email = email_sender_context_init(NULL, email_sender_user_agent_libcurl, NULL);
	assert(email);
	
	const char * smtp_server = getenv("EMAIL_SENDER_SMTP_SERVER");
	// load credentials
	const char * username = getenv("EMAIL_SENDER_USERNAME");
	const char * password = getenv("EMAIL_SENDER_PASSWORD");
	const char * from_addr = getenv("EMAIL_SENDER_FROM_ADDR");
	
	if(NULL == smtp_server) smtp_server = "email-smtp.ap-northeast-1.amazonaws.com";
	assert(username && username[0]);
	assert(password && password[0]);
	
	email->set_smtp_server(email, smtp_security_mode_force_tls, smtp_server, 587);
	email->set_auth_plain(email, username, password);
	rc = email->set_from_addr(email, from_addr);
	
	rc = email->add_recipents(email, email_address_type_to, 
		//~ "<hongwei.che@gmail.com>", 
		//~ "<htc.chehw@gmail.com>", 
		"<hongwei.che@tlzs.co.jp>", 
		"<tlzs-dev@tlzs.co.jp>", 
		NULL);
	assert(0 == rc);
	
	//~ rc = email->add_recipents(email, email_address_type_cc, 
		//~ "<test1@nour.global>", 
		//~ "<test2@nour.global>", 
		//~ "<test3@nour.global>", 
		//~ NULL);
	//~ assert(0 == rc);
	
	//~ rc = email->add_recipents(email, email_address_type_bcc, 
		//~ "<tlzs-dev@nour.global>",
		//~ "<admin@nour.global>",
		//~ NULL);
		
	email->add_header(email, "Reply-to", "admin@nour.global");
	email->add_header(email, "subject", "test 1");
	
	email->add_body(email, "hello world", -1);
	
	email->prepare_payload(email, email->payload, NULL);
	email_sender_context_dump(email);
	
	rc = email->send(email);
	fprintf(stderr, "email->send(): rc = %d\n", rc);
	
	email->clear(email);
	email_sender_context_cleanup(email);
	free(email);
	
	curl_global_cleanup();
	return 0;
}

