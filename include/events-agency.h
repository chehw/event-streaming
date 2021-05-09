#ifndef _EVENTS_AGENCY_H_
#define _EVENTS_AGENCY_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <json-c/json.h>

/**
 * @defgroup events_agency
 * Producer/Comsumer client Wrapper for kafka-broker or custom-backend
 * 
 * @{ 
 * 
 * @defgroup topic 
 * publish/consume messages to/from the topic
 * @{
 * @}
 * 
 * @defgroup priv_data
 * @{
 * @}
 * 
 * @}
*/

struct events_topic_context;
typedef int (* events_topic_on_notify_fn)(struct events_topic_context * eva_topic, /* const */ json_object * jevents, void * notify_data);

/**
 * @ingroup topic
 * struct events_topic_context
 * @brief
 * @var priv
 * @var eva
 * @var broker
 * @var topic
 * @
 * 
**/
typedef struct events_topic_context
{
	void * priv;
	struct events_agency * eva;
	
	char * broker;
	char * topic;

	// public method
	int (* publish)(struct events_topic_context * eva_topic, /* const */ json_object * jevent);							// push a single message
	int (* consume)(struct events_topic_context * eva_topic, events_topic_on_notify_fn on_notify, void * notify_data);	// poll and consume a single message

	// callback
	events_topic_on_notify_fn on_notify; // consume messages
	void * notify_data;
	void (* on_free_data)(void *notify_data);
}events_topic_context;
/**
 * @}
*/

/**
 * @ingroup topic
*/
events_topic_context * events_topic_context_new(struct events_agency * eva, const char * broker, const char * topic);
void events_topic_context_free(events_topic_context * eva_topic);
/**
 * @}
*/


/**
 * struct events_agency
 * @ingroup events_agency
 * @{
**/
typedef struct events_agency
{
	void * priv;
	void * user_data;
	
	char * bootstap_broker_uri;	// default broker
	char * topic;				// default topic
	json_object * jconfig;
	int (* load_config)(struct events_agency * eva, /* const */ json_object * jconfig);
	
	struct events_topic_context * (* find_topic)(struct events_agency * eva, const char * broker, const char * topic);
	struct events_topic_context * (* subscribe)(struct events_agency * eva, const char * broker, const char * topic, events_topic_on_notify_fn on_notify, void * notify_data, void (* on_free_data)(void *));
	int (* unsubscribe)(struct events_agency * eva, const char * broker, const char * topic);
}events_agency;
/**
 * @}
*/

/**
 * @ingroup events_agency
**/
events_agency * events_agency_init(events_agency * eva, void * user_data);
void events_agency_cleanup(events_agency * eva);
/**
 * @}
*/

#ifdef __cplusplus
}
#endif
#endif
