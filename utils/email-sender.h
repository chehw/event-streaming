#ifndef EMAIL_SENDER_H_
#define EMAIL_SENDER_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

enum smtp_security_mode
{
	smtp_security_mode_default = 0,		// smtp_security_mode_force_tls
	smtp_security_mode_try_tls = 1,
	smtp_security_mode_ssl = 2, 		// default port 465, use legacy smtps protocol
	smtp_security_mode_force_tls = 3,	// default port 587
};

enum email_sender_user_agent
{
	email_sender_user_agent_default, 
	email_sender_user_agent_libcurl = 1,
	email_sender_interactive, // tcp with io-redirect
};

struct email_sender_context
{
	void * priv;
	void * user_data;
	
	// public methods
	int (* set_smtp_server)(struct email_sender_context * email, enum smtp_security_mode mode, const char * server_name, unsigned int port);
	int (* set_auth)(struct email_sender_context * email, const char * username, const char * password);
	
	int (* set_from_addr)(struct email_sender_context * email, const char * from_addr);
	int (* set_to_addr)(struct email_sender_context * email, const char * to_addr);
	int (* set_cc_addr)(struct email_sender_context * email, const char * cc_addr);
	
	int (* add_line)(struct email_sender_context * email, const char * fmt, ...);	// add formated lines, total length should be less than 4096
	int (* add_header)(struct email_sender_context * email, const char * key, const char * value); // add key-value pairs
	int (* add_body)(struct email_sender_context * email, const char * body, size_t cb_body); // add large blocks of data
	void (* clear)(struct email_sender_context * email);	// remove all lines
	
	int (* send)(struct email_sender_context * email);
	void (* cleanup)(struct email_sender_context * email);
};
void email_sender_context_cleanup(struct email_sender_context * email);
struct email_sender_context * email_sender_context_init(
	struct email_sender_context * email, 
	enum email_sender_user_agent agent, 
	void * user_data);

#ifdef __cplusplus
}
#endif
#endif
