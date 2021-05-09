/*
 * events-agency.c
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

#include <search.h>
#include <pthread.h>
#include "events-agency.h"

/********************************************************
* struct events_topic_context
********************************************************/
struct events_topic_private
{
	struct events_topic_context * eva_topic;
	pthread_rwlock_t rw_lock;
};
static struct events_topic_private * events_topic_private_new(struct events_topic_context * eva_topic)
{
	assert(eva_topic);
	struct events_topic_private * priv = calloc(1, sizeof(* priv));
	assert(priv);
	
	eva_topic->priv = priv;
	priv->eva_topic = eva_topic;
	
	int rc = pthread_rwlock_init(&priv->rw_lock, NULL);
	assert(0 == rc);
	
	return priv;
}
static void events_topic_private_free(struct events_topic_private * priv)
{
	if(NULL == priv) return;
	
	pthread_rwlock_destroy(&priv->rw_lock);
	free(priv);
	return;
}

static int events_topic_publish(struct events_topic_context * eva_topic, /* const */ json_object * jevent) 
{
	return 0;
}
static int events_topic_consume(struct events_topic_context * eva_topic, 
	events_topic_on_notify_fn on_notify, 
	void * notify_data)
{
	return 0;
}

struct events_topic_context * events_topic_context_new(struct events_agency * eva, const char * broker, const char * topic)
{
	struct events_topic_context * eva_topic = calloc(1, sizeof(*eva_topic));
	assert(eva_topic);

	struct events_topic_private * priv = events_topic_private_new(eva_topic);
	assert(priv && eva_topic->priv == priv);

	if(broker) eva_topic->broker = strdup(broker);
	if(topic) eva_topic->topic = strdup(topic);

	eva_topic->publish = events_topic_publish;
	eva_topic->consume = events_topic_consume;

	return eva_topic;
}

void events_topic_context_free(struct events_topic_context * eva_topic)
{
	if(NULL == eva_topic) return;

	if(eva_topic->notify_data && eva_topic->on_free_data) 
	{
		eva_topic->on_free_data(eva_topic->notify_data);
		eva_topic->notify_data = NULL;
	}
	
	if(eva_topic->broker) free(eva_topic->broker);
	if(eva_topic->topic) free(eva_topic->topic);
	events_topic_private_free(eva_topic->priv);
	
	free(eva_topic);
	return;
}

static int events_topic_compare(const void *_a, const void *_b)
{
	const struct events_topic_context *a = _a;
	const struct events_topic_context *b = _b;
	assert(a && b);
	
	int cmp = 0;
	if(a->broker || b->broker) 
	{
		if(NULL == a->broker) return -1;
		if(NULL == b->broker) return 1;
		cmp = strcmp(a->broker, b->broker);
	}
	if(cmp) return cmp;
	
	if(a->topic || b->topic) 
	{
		if(NULL == a->topic) return -1;
		if(NULL == b->topic) return -1;
		cmp = strcmp(a->topic, b->topic);
	}
	return cmp;
}

/********************************************************
* struct events_agency_private
********************************************************/
struct events_agency_private
{
	struct events_agency * eva;
	pthread_rwlock_t rw_lock;
	void * events_root;
	
};

static struct events_agency_private * events_agency_private_new(struct events_agency * eva)
{
	assert(eva);
	struct events_agency_private * priv = calloc(1, sizeof(*priv));
	assert(priv);
	
	int rc = pthread_rwlock_init(&priv->rw_lock, NULL);
	assert(0 == rc);
	
	eva->priv = priv;
	priv->eva = eva;
	return priv;
}
static void events_agency_private_free(struct events_agency_private * priv)
{
	if(NULL == priv) return;
	if(priv->eva) priv->eva->priv = NULL;
	
	tdestroy(priv->events_root, (void (*)(void *))events_topic_context_free);
	
	pthread_rwlock_destroy(&priv->rw_lock);
	free(priv);
	return;
}

/********************************************************
* struct events_agency
********************************************************/
static int events_agency_load_config(struct events_agency * eva, /* const */ json_object * jconfig)
{
	return -1;
}

static events_topic_context * events_agency_find_topic(struct events_agency * eva, const char * broker, const char * topic)
{
	assert(eva && eva->priv);
	struct events_agency_private * priv = eva->priv;
	
	struct events_topic_context pattern[1] = {{
		.broker = (char *)broker,
		.topic = (char *)topic,
	}};
	
	void * p_node = tfind(pattern, &priv->events_root, events_topic_compare);
	if(p_node) return *(void **)p_node;
	return NULL;
}

static struct events_topic_context * events_agency_subscribe(struct events_agency * eva, 
	const char * broker, const char * topic, 
	events_topic_on_notify_fn on_notify, 
	void * notify_data, 
	void (* on_free_data)(void *))
{
	assert(eva && eva->priv);
	struct events_agency_private * priv = eva->priv;
	struct events_topic_context * eva_topic = events_agency_find_topic(eva, broker, topic);
	if(eva_topic) {
		if(eva_topic->notify_data 
			&& eva_topic->notify_data != notify_data
			&& eva_topic->on_free_data) 
		{
			eva_topic->on_free_data(eva_topic->notify_data);
			eva_topic->notify_data = NULL;
		}
		eva_topic->on_notify = on_notify;
		eva_topic->notify_data = notify_data;
		eva_topic->on_free_data = on_free_data;
		return eva_topic;
	}

	eva_topic = events_topic_context_new(eva, broker, topic);
	eva_topic->on_notify = on_notify;
	eva_topic->notify_data = notify_data;
	eva_topic->on_free_data = on_free_data;
	
	void * p_node = tsearch(eva_topic, &priv->events_root, events_topic_compare);
	assert(p_node && *(void **)p_node == (void *)eva_topic);
	
	return eva_topic;
}

static int events_agency_unscribe(struct events_agency * eva, const char * broker, const char * topic)
{
	assert(eva && eva->priv);
	struct events_agency_private * priv = eva->priv;
	
	struct events_topic_context * eva_topic = events_agency_find_topic(eva, broker, topic);
	if(NULL == eva_topic) return -1;
	
	tdelete(eva_topic, &priv->events_root, events_topic_compare);
	events_topic_context_free(eva_topic);
	return 0;
}

struct events_agency * events_agency_init(struct events_agency * eva, void * user_data)
{
	if(NULL == eva) {
		eva = calloc(1, sizeof(*eva));
		assert(eva);
	}
	
	eva->user_data = user_data;
	eva->load_config = events_agency_load_config;
	
	eva->find_topic = events_agency_find_topic;
	eva->subscribe = events_agency_subscribe;
	eva->unsubscribe = events_agency_unscribe;

	struct events_agency_private * priv = events_agency_private_new(eva);
	assert(priv && eva->priv == priv);
	
	return eva;
}

void events_agency_cleanup(struct events_agency * eva)
{
	if(NULL == eva) return;
	events_agency_private_free(eva->priv);
	eva->priv = NULL;
	return;
}
