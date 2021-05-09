/*
 * main.c
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

#include <limits.h>
#include <sys/types.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <search.h>

#include <db.h>
#include <libsoup/soup.h>
#include <glib.h>

#include "events-agency.h"
typedef struct global_params
{
	void * user_data;
	struct events_agency * eva;
	
	GMainLoop * loop;
	SoupServer * server;
	
	DB * dbp;	// main db
	DB * sdbp;	// secondary db (indexed by timestamps)
}global_params_t;


static void on_document_root(SoupServer * server, SoupMessage *msg, const char * path, GHashTable * query, SoupClientContext * client, gpointer user_data);
int main(int argc, char **argv)
{
	int rc = 0;
	struct events_agency eva[1] = {{ NULL }};
	
	const char * conf_file = "conf/config.json";
	if(argc > 1) conf_file = argv[1];
	
	json_object * jconfig = json_object_from_file(conf_file);
	assert(jconfig);
	
	global_params_t params[1];
	memset(params, 0, sizeof(params));
	
	// starts a web server to accept direct API calls
	GMainLoop * loop = NULL;
	GError * gerr = NULL;
	gboolean ok = FALSE;
	SoupServer * server = soup_server_new(SOUP_SERVER_SERVER_HEADER, "api-gateway", NULL);
	
	params->eva = eva;
	params->server = server;
	
	events_agency_init(eva, params);
	rc = eva->load_config(eva, jconfig);
	assert(0 == rc);

	soup_server_add_handler(server, "/", on_document_root, params, NULL);
	ok = soup_server_listen_all(server, 8088, SOUP_SERVER_LISTEN_IPV4_ONLY, &gerr);
	if(gerr) {
		fprintf(stderr, "[ERROR]: soup_server_listen_all: %s\n", gerr->message);
		g_error_free(gerr);
		gerr = NULL;
	}
	assert(ok && NULL == gerr);
	
	loop = g_main_loop_new(NULL, FALSE);
	assert(loop);
	
	params->eva = eva;
	params->server = server;
	params->loop = loop;
	g_main_loop_run(loop);
	
	params->loop = NULL;
	g_main_loop_unref(loop);
	loop = NULL;
	
	events_agency_cleanup(eva);
	return rc;
}

static void on_document_root(SoupServer * server, SoupMessage *msg, const char * path, GHashTable * query, SoupClientContext * client, gpointer user_data)
{
	soup_message_set_status(msg, SOUP_STATUS_ACCEPTED);
	return;
}
