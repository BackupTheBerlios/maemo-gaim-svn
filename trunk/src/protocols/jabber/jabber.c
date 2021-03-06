/*
 * gaim - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "cmds.h"
#include "debug.h"
#include "message.h"
#include "notify.h"
#include "pluginpref.h"
#include "prpl.h"
#include "request.h"
#include "server.h"
#include "util.h"
#include "version.h"

#include "auth.h"
#include "buddy.h"
#include "chat.h"
#include "disco.h"
#include "iq.h"
#include "jutil.h"
#include "message.h"
#include "parser.h"
#include "presence.h"
#include "jabber.h"
#include "roster.h"
#include "si.h"
#include "xdata.h"

#define JABBER_CONNECT_STEPS (js->gsc ? 8 : 5)

static GaimPlugin *my_protocol = NULL;

static void jabber_stream_init(JabberStream *js)
{
	char *open_stream;

	open_stream = g_strdup_printf("<stream:stream to='%s' "
				          "xmlns='jabber:client' "
						  "xmlns:stream='http://etherx.jabber.org/streams' "
						  "version='1.0'>",
						  js->user->domain);

	jabber_send_raw(js, open_stream, -1);

	g_free(open_stream);
}

static void
jabber_session_initialized_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	const char *type = xmlnode_get_attrib(packet, "type");
	if(type && !strcmp(type, "result")) {
		jabber_stream_set_state(js, JABBER_STREAM_CONNECTED);
	} else {
		gaim_connection_error(js->gc, _("Error initializing session"));
	}
}

static void jabber_session_init(JabberStream *js)
{
	JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
	xmlnode *session;

	jabber_iq_set_callback(iq, jabber_session_initialized_cb, NULL);

	session = xmlnode_new_child(iq->node, "session");
	xmlnode_set_attrib(session, "xmlns", "urn:ietf:params:xml:ns:xmpp-session");

	jabber_iq_send(iq);
}

static void jabber_bind_result_cb(JabberStream *js, xmlnode *packet,
		gpointer data)
{
	const char *type = xmlnode_get_attrib(packet, "type");
	xmlnode *bind;

	if(type && !strcmp(type, "result") &&
			(bind = xmlnode_get_child_with_namespace(packet, "bind", "urn:ietf:params:xml:ns:xmpp-bind"))) {
		xmlnode *jid;
		char *full_jid;
		if((jid = xmlnode_get_child(bind, "jid")) && (full_jid = xmlnode_get_data(jid))) {
			JabberBuddy *my_jb = NULL;
			jabber_id_free(js->user);
			if(!(js->user = jabber_id_new(full_jid))) {
				gaim_connection_error(js->gc, _("Invalid response from server."));
			}
			if((my_jb = jabber_buddy_find(js, full_jid, TRUE)))
				my_jb->subscription |= JABBER_SUB_BOTH;
			g_free(full_jid);
		}
	} else {
		char *msg = jabber_parse_error(js, packet);
		gaim_connection_error(js->gc, msg);
		g_free(msg);
	}

	jabber_session_init(js);
}

static void jabber_stream_features_parse(JabberStream *js, xmlnode *packet)
{
	if(xmlnode_get_child(packet, "starttls")) {
		if(jabber_process_starttls(js, packet))
			return;
	}

	if(js->registration) {
		jabber_register_start(js);
	} else if(xmlnode_get_child(packet, "mechanisms")) {
		jabber_auth_start(js, packet);
	} else if(xmlnode_get_child(packet, "bind")) {
		xmlnode *bind, *resource;
		JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
		bind = xmlnode_new_child(iq->node, "bind");
		xmlnode_set_attrib(bind, "xmlns", "urn:ietf:params:xml:ns:xmpp-bind");
		resource = xmlnode_new_child(bind, "resource");
		xmlnode_insert_data(resource, js->user->resource, -1);

		jabber_iq_set_callback(iq, jabber_bind_result_cb, NULL);

		jabber_iq_send(iq);
	} else /* if(xmlnode_get_child_with_namespace(packet, "auth")) */ {
		/* If we get an empty stream:features packet, or we explicitly get
		 * an auth feature with namespace http://jabber.org/features/iq-auth
		 * we should revert back to iq:auth authentication, even though we're
		 * connecting to an XMPP server.  */
		js->auth_type = JABBER_AUTH_IQ_AUTH;
		jabber_stream_set_state(js, JABBER_STREAM_AUTHENTICATING);
	}
}

static void jabber_stream_handle_error(JabberStream *js, xmlnode *packet)
{
	char *msg = jabber_parse_error(js, packet);

	gaim_connection_error(js->gc, msg);
	g_free(msg);
}

static void tls_init(JabberStream *js);

void jabber_process_packet(JabberStream *js, xmlnode *packet)
{
	if(!strcmp(packet->name, "iq")) {
		jabber_iq_parse(js, packet);
	} else if(!strcmp(packet->name, "presence")) {
		jabber_presence_parse(js, packet);
	} else if(!strcmp(packet->name, "message")) {
		jabber_message_parse(js, packet);
	} else if(!strcmp(packet->name, "stream:features")) {
		jabber_stream_features_parse(js, packet);
	} else if(!strcmp(packet->name, "stream:error")) {
		jabber_stream_handle_error(js, packet);
	} else if(!strcmp(packet->name, "challenge")) {
		if(js->state == JABBER_STREAM_AUTHENTICATING)
			jabber_auth_handle_challenge(js, packet);
	} else if(!strcmp(packet->name, "success")) {
		if(js->state == JABBER_STREAM_AUTHENTICATING)
			jabber_auth_handle_success(js, packet);
	} else if(!strcmp(packet->name, "failure")) {
		if(js->state == JABBER_STREAM_AUTHENTICATING)
			jabber_auth_handle_failure(js, packet);
	} else if(!strcmp(packet->name, "proceed")) {
		if(js->state == JABBER_STREAM_AUTHENTICATING && !js->gsc)
			tls_init(js);
	} else {
		gaim_debug(GAIM_DEBUG_WARNING, "jabber", "Unknown packet: %s\n",
				packet->name);
	}
}

void jabber_send_raw(JabberStream *js, const char *data, int len)
{
	int ret;

	/* because printing a tab to debug every minute gets old */
	if(strcmp(data, "\t"))
		gaim_debug(GAIM_DEBUG_MISC, "jabber", "Sending%s: %s\n",
				js->gsc ? " (ssl)" : "", data);

	if(js->gsc) {
		ret = gaim_ssl_write(js->gsc, data, len == -1 ? strlen(data) : len);
	} else {
		if(js->fd < 0)
			return;
		ret = write(js->fd, data, len == -1 ? strlen(data) : len);
	}

	if(ret < 0)
		gaim_connection_error(js->gc, _("Write error"));

}

void jabber_send(JabberStream *js, xmlnode *packet)
{
	char *txt;
	int len;

	txt = xmlnode_to_str(packet, &len);
	jabber_send_raw(js, txt, len);
	g_free(txt);
}

static void jabber_keepalive(GaimConnection *gc)
{
	jabber_send_raw(gc->proto_data, "\t", -1);
}

static void
jabber_recv_cb_ssl(gpointer data, GaimSslConnection *gsc,
		GaimInputCondition cond)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;
	int len;
	static char buf[4096];

	if(!g_list_find(gaim_connections_get_all(), gc)) {
		gaim_ssl_close(gsc);
		return;
	}

	if((len = gaim_ssl_read(gsc, buf, sizeof(buf) - 1)) > 0) {
		buf[len] = '\0';
		gaim_debug(GAIM_DEBUG_INFO, "jabber", "Recv (ssl)(%d): %s\n", len, buf);
		jabber_parser_process(js, buf, len);
	} else {
		gaim_connection_error(gc, _("Read Error"));
	}
}

static void
jabber_recv_cb(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;
	int len;
	static char buf[4096];

	if(!g_list_find(gaim_connections_get_all(), gc))
		return;

	if((len = read(js->fd, buf, sizeof(buf) - 1)) > 0) {
		buf[len] = '\0';
		gaim_debug(GAIM_DEBUG_INFO, "jabber", "Recv (%d): %s\n", len, buf);
		jabber_parser_process(js, buf, len);
	} else {
		gaim_connection_error(gc, _("Read Error"));
	}
}

static void
jabber_login_callback_ssl(gpointer data, GaimSslConnection *gsc,
		GaimInputCondition cond)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;

	if(!g_list_find(gaim_connections_get_all(), gc)) {
		gaim_ssl_close(gsc);
		return;
	}

	js->gsc = gsc;

	if(js->state == JABBER_STREAM_CONNECTING)
		jabber_send_raw(js, "<?xml version='1.0' ?>", -1);

	jabber_stream_set_state(js, JABBER_STREAM_INITIALIZING);
	gaim_ssl_input_add(gsc, jabber_recv_cb_ssl, gc);
}


static void
jabber_login_callback(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;

	if (source < 0) {
		gaim_connection_error(gc, _("Couldn't connect to host"));
		return;
	}

	if(!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	js->fd = source;

	if(js->state == JABBER_STREAM_CONNECTING)
		jabber_send_raw(js, "<?xml version='1.0' ?>", -1);

	jabber_stream_set_state(js, JABBER_STREAM_INITIALIZING);
	gc->inpa = gaim_input_add(js->fd, GAIM_INPUT_READ, jabber_recv_cb, gc);
}

static void
jabber_ssl_connect_failure(GaimSslConnection *gsc, GaimSslErrorType error,
		gpointer data)
{
	GaimConnection *gc = data;
	JabberStream *js = gc->proto_data;

	switch(error) {
		case GAIM_SSL_CONNECT_FAILED:
			gaim_connection_error(gc, _("Connection Failed"));
			break;
		case GAIM_SSL_HANDSHAKE_FAILED:
			gaim_connection_error(gc, _("SSL Handshake Failed"));
			break;
	}

	js->gsc = NULL;
}

static void tls_init(JabberStream *js)
{
	gaim_input_remove(js->gc->inpa);
	js->gc->inpa = 0;
	js->gsc = gaim_ssl_connect_fd(js->gc->account, js->fd,
			jabber_login_callback_ssl, jabber_ssl_connect_failure, js->gc);
}


static void
jabber_login(GaimAccount *account)
{
	int rc;
	GaimConnection *gc = gaim_account_get_connection(account);
	const char *connect_server = gaim_account_get_string(account,
			"connect_server", "");
	const char *server;
	JabberStream *js;
	JabberBuddy *my_jb = NULL;

	gc->flags |= GAIM_CONNECTION_HTML;
	js = gc->proto_data = g_new0(JabberStream, 1);
	js->gc = gc;
	js->fd = -1;
	js->iq_callbacks = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	js->disco_callbacks = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	js->buddies = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)jabber_buddy_free);
	js->chats = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)jabber_chat_free);
	js->chat_servers = g_list_append(NULL, g_strdup("conference.jabber.org"));
	js->user = jabber_id_new(gaim_account_get_username(account));
	js->next_id = g_random_int();

	if(!js->user) {
		gaim_connection_error(gc, _("Invalid Jabber ID"));
		return;
	}

	if(!js->user->resource) {
		char *me;
		js->user->resource = g_strdup("Gaim");
		if(!js->user->node) {
			js->user->node = js->user->domain;
			js->user->domain = g_strdup("jabber.org");
		}
		me = g_strdup_printf("%s@%s/%s", js->user->node, js->user->domain,
				js->user->resource);
		gaim_account_set_username(account, me);
		g_free(me);
	}

	if((my_jb = jabber_buddy_find(js, gaim_account_get_username(account), TRUE)))
		my_jb->subscription |= JABBER_SUB_BOTH;

	server = connect_server[0] ? connect_server : js->user->domain;

	jabber_stream_set_state(js, JABBER_STREAM_CONNECTING);


	if(gaim_account_get_bool(account, "old_ssl", FALSE)) {
		if(gaim_ssl_is_supported()) {
			js->gsc = gaim_ssl_connect(account, server,
					gaim_account_get_int(account, "port", 5222),
					jabber_login_callback_ssl, jabber_ssl_connect_failure, gc);
		} else {
			gaim_connection_error(gc, _("SSL support unavailable"));
		}
	}

	if(!js->gsc) {
		rc = gaim_proxy_connect(account, server,
				gaim_account_get_int(account, "port", 5222),
				jabber_login_callback, gc);

		if (rc != 0)
			gaim_connection_error(gc, _("Unable to create socket"));
	}
}

static gboolean
conn_close_cb(gpointer data)
{
	JabberStream *js = data;
	gaim_connection_destroy(js->gc);
	return FALSE;
}

static void
jabber_connection_schedule_close(JabberStream *js)
{
	gaim_timeout_add(0, conn_close_cb, js);
}

static void
jabber_registration_result_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	const char *type = xmlnode_get_attrib(packet, "type");
	char *buf;

	if(!strcmp(type, "result")) {
		buf = g_strdup_printf(_("Registration of %s@%s successful"),
				js->user->node, js->user->domain);
		gaim_notify_info(NULL, _("Registration Successful"),
				_("Registration Successful"), buf);
		g_free(buf);
	} else {
		char *msg = jabber_parse_error(js, packet);

		if(!msg)
			msg = g_strdup(_("Unknown Error"));

		gaim_notify_error(NULL, _("Registration Failed"),
				_("Registration Failed"), msg);
		g_free(msg);
	}
	jabber_connection_schedule_close(js);
}

static void
jabber_register_cb(JabberStream *js, GaimRequestFields *fields)
{
	GList *groups, *flds;
	xmlnode *query, *y;
	JabberIq *iq;
	char *username;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");
	query = xmlnode_get_child(iq->node, "query");

	for(groups = gaim_request_fields_get_groups(fields); groups;
			groups = groups->next) {
		for(flds = gaim_request_field_group_get_fields(groups->data);
				flds; flds = flds->next) {
			GaimRequestField *field = flds->data;
			const char *id = gaim_request_field_get_id(field);
			const char *value = gaim_request_field_string_get_value(field);

			if(!strcmp(id, "username")) {
				y = xmlnode_new_child(query, "username");
			} else if(!strcmp(id, "password")) {
				y = xmlnode_new_child(query, "password");
			} else if(!strcmp(id, "name")) {
				y = xmlnode_new_child(query, "name");
			} else if(!strcmp(id, "email")) {
				y = xmlnode_new_child(query, "email");
			} else if(!strcmp(id, "nick")) {
				y = xmlnode_new_child(query, "nick");
			} else if(!strcmp(id, "first")) {
				y = xmlnode_new_child(query, "first");
			} else if(!strcmp(id, "last")) {
				y = xmlnode_new_child(query, "last");
			} else if(!strcmp(id, "address")) {
				y = xmlnode_new_child(query, "address");
			} else if(!strcmp(id, "city")) {
				y = xmlnode_new_child(query, "city");
			} else if(!strcmp(id, "state")) {
				y = xmlnode_new_child(query, "state");
			} else if(!strcmp(id, "zip")) {
				y = xmlnode_new_child(query, "zip");
			} else if(!strcmp(id, "phone")) {
				y = xmlnode_new_child(query, "phone");
			} else if(!strcmp(id, "url")) {
				y = xmlnode_new_child(query, "url");
			} else if(!strcmp(id, "date")) {
				y = xmlnode_new_child(query, "date");
			} else {
				continue;
			}
			xmlnode_insert_data(y, value, -1);
			if(!strcmp(id, "username")) {
				if(js->user->node)
					g_free(js->user->node);
				js->user->node = g_strdup(value);
			}
		}
	}

	username = g_strdup_printf("%s@%s/%s", js->user->node, js->user->domain,
			js->user->resource);
	gaim_account_set_username(js->gc->account, username);
	g_free(username);

	jabber_iq_set_callback(iq, jabber_registration_result_cb, NULL);

	jabber_iq_send(iq);

}

static void
jabber_register_cancel_cb(JabberStream *js, GaimRequestFields *fields)
{
	jabber_connection_schedule_close(js);
}

static void jabber_register_x_data_cb(JabberStream *js, xmlnode *result, gpointer data)
{
	xmlnode *query;
	JabberIq *iq;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");
	query = xmlnode_get_child(iq->node, "query");

	xmlnode_insert_child(query, result);

	jabber_iq_set_callback(iq, jabber_registration_result_cb, NULL);
	jabber_iq_send(iq);
}

void jabber_register_parse(JabberStream *js, xmlnode *packet)
{
	if(js->registration) {
		GaimRequestFields *fields;
		GaimRequestFieldGroup *group;
		GaimRequestField *field;
		xmlnode *query, *x, *y;
		char *instructions;

		/* get rid of the login thingy */
		gaim_connection_set_state(js->gc, GAIM_CONNECTED);

		query = xmlnode_get_child(packet, "query");

		if(xmlnode_get_child(query, "registered")) {
			gaim_notify_error(NULL, _("Already Registered"),
					_("Already Registered"), NULL);
			jabber_connection_schedule_close(js);
			return;
		}

		if((x = xmlnode_get_child_with_namespace(packet, "x",
						"jabber:x:data"))) {
			jabber_x_data_request(js, x, jabber_register_x_data_cb, NULL);
			return;
		} else if((x = xmlnode_get_child_with_namespace(packet, "x",
						"jabber:x:oob"))) {
			xmlnode *url;

			if((url = xmlnode_get_child(x, "url"))) {
				char *href;
				if((href = xmlnode_get_data(url))) {
					gaim_notify_uri(NULL, href);
					g_free(href);
					js->gc->wants_to_die = TRUE;
					jabber_connection_schedule_close(js);
					return;
				}
			}
		}

		/* as a last resort, use the old jabber:iq:register syntax */

		fields = gaim_request_fields_new();
		group = gaim_request_field_group_new(NULL);
		gaim_request_fields_add_group(fields, group);

		field = gaim_request_field_string_new("username", _("Username"),
				js->user->node, FALSE);
		gaim_request_field_group_add_field(group, field);

		field = gaim_request_field_string_new("password", _("Password"),
				gaim_account_get_password(js->gc->account), FALSE);
		gaim_request_field_string_set_masked(field, TRUE);
		gaim_request_field_group_add_field(group, field);

		if(xmlnode_get_child(query, "name")) {
			field = gaim_request_field_string_new("name", _("Name"),
					gaim_account_get_alias(js->gc->account), FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "email")) {
			field = gaim_request_field_string_new("email", _("E-Mail"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "nick")) {
			field = gaim_request_field_string_new("nick", _("Nickname"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "first")) {
			field = gaim_request_field_string_new("first", _("First Name"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "last")) {
			field = gaim_request_field_string_new("last", _("Last Name"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "address")) {
			field = gaim_request_field_string_new("address", _("Address"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "city")) {
			field = gaim_request_field_string_new("city", _("City"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "state")) {
			field = gaim_request_field_string_new("state", _("State"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "zip")) {
			field = gaim_request_field_string_new("zip", _("Postal Code"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "phone")) {
			field = gaim_request_field_string_new("phone", _("Phone"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "url")) {
			field = gaim_request_field_string_new("url", _("URL"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "date")) {
			field = gaim_request_field_string_new("date", _("Date"),
					NULL, FALSE);
			gaim_request_field_group_add_field(group, field);
		}

		if((y = xmlnode_get_child(query, "instructions")))
			instructions = xmlnode_get_data(y);
		else
			instructions = g_strdup(_("Please fill out the information below "
						"to register your new account."));

		gaim_request_fields(js->gc, _("Register New Jabber Account"),
				_("Register New Jabber Account"), instructions, fields,
				_("Register"), G_CALLBACK(jabber_register_cb),
				_("Cancel"), G_CALLBACK(jabber_register_cancel_cb), js);
	}
}

void jabber_register_start(JabberStream *js)
{
	JabberIq *iq;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:register");
	jabber_iq_send(iq);
}

static void jabber_register_account(GaimAccount *account)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	JabberStream *js;
	JabberBuddy *my_jb = NULL;
	const char *connect_server = gaim_account_get_string(account,
			"connect_server", "");
	const char *server;
	int rc;

	js = gc->proto_data = g_new0(JabberStream, 1);
	js->gc = gc;
	js->registration = TRUE;
	js->iq_callbacks = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	js->disco_callbacks = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	js->user = jabber_id_new(gaim_account_get_username(account));
	js->next_id = g_random_int();

	if(!js->user) {
		gaim_connection_error(gc, _("Invalid Jabber ID"));
		return;
	}

	if(!js->user->resource) {
		char *me;
		js->user->resource = g_strdup("Gaim");
		if(!js->user->node) {
			js->user->node = js->user->domain;
			js->user->domain = g_strdup("jabber.org");
		}
		me = g_strdup_printf("%s@%s/%s", js->user->node, js->user->domain,
				js->user->resource);
		gaim_account_set_username(account, me);
		g_free(me);
	}

	if((my_jb = jabber_buddy_find(js, gaim_account_get_username(account), TRUE)))
		my_jb->subscription |= JABBER_SUB_BOTH;

	server = connect_server[0] ? connect_server : js->user->domain;

	jabber_stream_set_state(js, JABBER_STREAM_CONNECTING);

	if(gaim_account_get_bool(account, "old_ssl", FALSE)) {
		if(gaim_ssl_is_supported()) {
			js->gsc = gaim_ssl_connect(account, server,
					gaim_account_get_int(account, "port", 5222),
					jabber_login_callback_ssl, jabber_ssl_connect_failure, gc);
		} else {
			gaim_connection_error(gc, _("SSL support unavailable"));
		}
	}

	if(!js->gsc) {
		rc = gaim_proxy_connect(account, server,
				gaim_account_get_int(account, "port", 5222),
				jabber_login_callback, gc);

		if (rc != 0)
			gaim_connection_error(gc, _("Unable to create socket"));
	}
}

static void jabber_close(GaimConnection *gc)
{
	JabberStream *js = gc->proto_data;

	jabber_presence_send(gc, "unavailable", _("Logged out"));
	jabber_send_raw(js, "</stream:stream>", -1);

	if(js->gsc) {
		gaim_ssl_close(js->gsc);
	} else if (js->fd > 0) {
		if(js->gc->inpa)
			gaim_input_remove(js->gc->inpa);
		close(js->fd);
	}

	if(js->context)
		g_markup_parse_context_free(js->context);
	if(js->iq_callbacks)
		g_hash_table_destroy(js->iq_callbacks);
	if(js->disco_callbacks)
		g_hash_table_destroy(js->disco_callbacks);
	if(js->buddies)
		g_hash_table_destroy(js->buddies);
	if(js->chats)
		g_hash_table_destroy(js->chats);
	while(js->chat_servers) {
		g_free(js->chat_servers->data);
		js->chat_servers = g_list_delete_link(js->chat_servers, js->chat_servers);
	}
	if(js->stream_id)
		g_free(js->stream_id);
	if(js->user)
		jabber_id_free(js->user);
	g_free(js);
}

void jabber_stream_set_state(JabberStream *js, JabberStreamState state)
{
	js->state = state;
	switch(state) {
		case JABBER_STREAM_OFFLINE:
			break;
		case JABBER_STREAM_CONNECTING:
			gaim_connection_update_progress(js->gc, _("Connecting"), 1,
					JABBER_CONNECT_STEPS);
			break;
		case JABBER_STREAM_INITIALIZING:
			gaim_connection_update_progress(js->gc, _("Initializing Stream"),
					js->gsc ? 5 : 2, JABBER_CONNECT_STEPS);
			jabber_stream_init(js);
			jabber_parser_setup(js);
			break;
		case JABBER_STREAM_AUTHENTICATING:
			gaim_connection_update_progress(js->gc, _("Authenticating"),
					js->gsc ? 6 : 3, JABBER_CONNECT_STEPS);
			if(js->protocol_version == JABBER_PROTO_0_9 && js->registration) {
				jabber_register_start(js);
			} else if(js->auth_type == JABBER_AUTH_IQ_AUTH) {
				jabber_auth_start_old(js);
			}
			break;
		case JABBER_STREAM_REINITIALIZING:
			gaim_connection_update_progress(js->gc, _("Re-initializing Stream"),
					6, JABBER_CONNECT_STEPS);
			jabber_stream_init(js);
			break;
		case JABBER_STREAM_CONNECTED:
			jabber_roster_request(js);
			jabber_presence_send(js->gc, js->gc->away_state, js->gc->away);
			jabber_disco_items_server(js);
			gaim_connection_set_state(js->gc, GAIM_CONNECTED);
			serv_finish_login(js->gc);
			break;
	}
}

char *jabber_get_next_id(JabberStream *js)
{
	return g_strdup_printf("gaim%x", js->next_id++);
}


static void jabber_idle_set(GaimConnection *gc, int idle)
{
	JabberStream *js = gc->proto_data;

	js->idle = idle ? time(NULL) - idle : idle;
}

static const char *jabber_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "jabber";
}

static void jabber_list_emblems(GaimBuddy *b, char **se, char **sw,
		char **nw, char **ne)
{
	JabberStream *js;
	JabberBuddy *jb;

	if(!b->account->gc)
		return;
	js = b->account->gc->proto_data;
	jb = jabber_buddy_find(js, b->name, FALSE);

	if(!GAIM_BUDDY_IS_ONLINE(b)) {
		if(jb && jb->error_msg)
			*nw = "error";

		if(jb && (jb->subscription & JABBER_SUB_PENDING ||
					!(jb->subscription & JABBER_SUB_TO)))
			*se = "notauthorized";
		else
			*se = "offline";
	} else {
		switch (b->uc) {
			case JABBER_STATE_AWAY:
				*se = "away";
				break;
			case JABBER_STATE_CHAT:
				*se = "chat";
				break;
			case JABBER_STATE_XA:
				*se = "extendedaway";
				break;
			case JABBER_STATE_DND:
				*se = "dnd";
				break;
			case JABBER_STATE_ERROR:
				*se = "error";
				break;
		}
	}
}

static char *jabber_status_text(GaimBuddy *b)
{
	JabberBuddy *jb = jabber_buddy_find(b->account->gc->proto_data, b->name,
			FALSE);
	char *ret = NULL;

	if(jb && !GAIM_BUDDY_IS_ONLINE(b) && (jb->subscription & JABBER_SUB_PENDING || !(jb->subscription & JABBER_SUB_TO))) {
		ret = g_strdup(_("Not Authorized"));
	} else if(jb && !GAIM_BUDDY_IS_ONLINE(b) && jb->error_msg) {
		ret = g_strdup(jb->error_msg);
	} else {
		char *stripped;

		stripped = gaim_markup_strip_html(jabber_buddy_get_status_msg(jb));

		if(!stripped && b->uc & UC_UNAVAILABLE)
			stripped = g_strdup(jabber_get_state_string(b->uc));

		if(stripped) {
			ret = g_markup_escape_text(stripped, -1);
			g_free(stripped);
		}
	}

	return ret;
}

static char *jabber_tooltip_text(GaimBuddy *b)
{
	JabberBuddy *jb;
	GString *ret;

	g_return_val_if_fail(b != NULL, NULL);
	g_return_val_if_fail(b->account != NULL, NULL);
	g_return_val_if_fail(b->account->gc != NULL, NULL);
	g_return_val_if_fail(b->account->gc->proto_data != NULL, NULL);

	jb = jabber_buddy_find(b->account->gc->proto_data, b->name,
			FALSE);
	ret = g_string_new("");

	if(jb) {
		JabberBuddyResource *jbr = jabber_buddy_find_resource(jb, NULL);
		const char *sub;
		GList *l = NULL;

		if(jb->subscription & JABBER_SUB_FROM) {
			if(jb->subscription & JABBER_SUB_TO)
				sub = _("Both");
			else if(jb->subscription & JABBER_SUB_PENDING)
				sub = _("From (To pending)");
			else
				sub = _("From");
		} else {
			if(jb->subscription & JABBER_SUB_TO)
				sub = _("To");
			else if(jb->subscription & JABBER_SUB_PENDING)
				sub = _("None (To pending)");
			else
				sub = _("None");
		}
		g_string_append_printf(ret, "\n<b>%s:</b> %s", _("Subscription"), sub);

		for(l=jb->resources; l; l = l->next) {
			char *text = NULL;
			char *res = NULL;

			jbr = l->data;

			if(jbr->status) {
				char *stripped;
				stripped = gaim_markup_strip_html(jbr->status);
				text = g_markup_escape_text(stripped, -1);
				g_free(stripped);
			}

			if(jb->resources->next && jbr->name) {
				res = g_strdup_printf(" (%s)", jbr->name);
			}

			g_string_append_printf(ret, "\n<b>%s%s:</b> %s%s%s",
					_("Status"),
					res ? res : "",
					jabber_get_state_string(jbr->state),
					text ? ": " : "",
					text ? text : "");
			if(text)
				g_free(text);
			if(res)
				g_free(res);
		}

		if(!jb->resources && jb->error_msg) {
			g_string_append_printf(ret, "\n<b>%s:</b> %s",
					_("Error"), jb->error_msg);
		}
	}

	return g_string_free(ret, FALSE);
}

static GList *jabber_away_states(GaimConnection *gc)
{
	JabberStream *js = gc->proto_data;
	GList *m = NULL;

	m = g_list_append(m, _("Online"));
	m = g_list_append(m, _("Chatty"));
	m = g_list_append(m, _("Away"));
	m = g_list_append(m, _("Extended Away"));
	m = g_list_append(m, _("Do Not Disturb"));
	if(js->protocol_version == JABBER_PROTO_0_9)
		m = g_list_append(m, _("Invisible"));
	m = g_list_append(m, GAIM_AWAY_CUSTOM);

	return m;
}

static void
jabber_password_change_result_cb(JabberStream *js, xmlnode *packet,
		gpointer data)
{
	const char *type;

	type = xmlnode_get_attrib(packet, "type");

	if(type && !strcmp(type, "result")) {
		gaim_notify_info(js->gc, _("Password Changed"), _("Password Changed"),
				_("Your password has been changed."));
	} else {
		char *msg = jabber_parse_error(js, packet);

		gaim_notify_error(js->gc, _("Error changing password"),
				_("Error changing password"), msg);
		g_free(msg);
	}
}

static void jabber_password_change_cb(JabberStream *js,
		GaimRequestFields *fields)
{
	const char *p1, *p2;
	JabberIq *iq;
	xmlnode *query, *y;

	p1 = gaim_request_fields_get_string(fields, "password1");
	p2 = gaim_request_fields_get_string(fields, "password2");

	if(strcmp(p1, p2)) {
		gaim_notify_error(js->gc, NULL, _("New passwords do not match."), NULL);
		return;
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");

	xmlnode_set_attrib(iq->node, "to", js->user->domain);

	query = xmlnode_get_child(iq->node, "query");

	y = xmlnode_new_child(query, "username");
	xmlnode_insert_data(y, js->user->node, -1);
	y = xmlnode_new_child(query, "password");
	xmlnode_insert_data(y, p1, -1);

	jabber_iq_set_callback(iq, jabber_password_change_result_cb, NULL);

	jabber_iq_send(iq);

	gaim_account_set_password(js->gc->account, p1);
}

static void jabber_password_change(GaimPluginAction *action)
{

	GaimConnection *gc = (GaimConnection *) action->context;
	JabberStream *js = gc->proto_data;
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();
	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("password1", _("Password"),
			"", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("password2", _("Password (again)"),
			"", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(js->gc, _("Change Jabber Password"),
			_("Change Jabber Password"), _("Please enter your new password"),
			fields, _("OK"), G_CALLBACK(jabber_password_change_cb),
			_("Cancel"), NULL, js);
}

static GList *jabber_actions(GaimPlugin *plugin, gpointer context)
{
	GList *m = NULL;
	GaimPluginAction *act;

	act = gaim_plugin_action_new(_("Set User Info"),
			jabber_setup_set_info);
	m = g_list_append(m, act);

	/* if (js->protocol_options & CHANGE_PASSWORD) { */
		act = gaim_plugin_action_new(_("Change Password"),
				jabber_password_change);
		m = g_list_append(m, act);
	/* } */

	return m;
}

static GaimChat *jabber_find_blist_chat(GaimAccount *account, const char *name)
{
	GaimBlistNode *gnode, *cnode;
	JabberID *jid;

	if(!(jid = jabber_id_new(name)))
		return NULL;

	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			GaimChat *chat = (GaimChat*)cnode;
			const char *room, *server;
			if(!GAIM_BLIST_NODE_IS_CHAT(cnode))
				continue;

			if(chat->account != account)
				continue;

			if(!(room = g_hash_table_lookup(chat->components, "room")))
				continue;
			if(!(server = g_hash_table_lookup(chat->components, "server")))
				continue;

			if(jid->node && jid->domain &&
					!g_utf8_collate(room, jid->node) && !g_utf8_collate(server, jid->domain)) {
				jabber_id_free(jid);
				return chat;
			}
		}
	}
	jabber_id_free(jid);
	return NULL;
}

static void jabber_convo_closed(GaimConnection *gc, const char *who)
{
	JabberStream *js = gc->proto_data;
	JabberID *jid;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;

	if(!(jid = jabber_id_new(who)))
		return;

	if((jb = jabber_buddy_find(js, who, TRUE)) &&
			(jbr = jabber_buddy_find_resource(jb, jid->resource))) {
		if(jbr->thread_id) {
			g_free(jbr->thread_id);
			jbr->thread_id = NULL;
		}
	}

	jabber_id_free(jid);
}


char *jabber_parse_error(JabberStream *js, xmlnode *packet)
{
	xmlnode *error;
	const char *code = NULL, *text = NULL;
	const char *xmlns = xmlnode_get_attrib(packet, "xmlns");
	char *cdata = NULL;

	if((error = xmlnode_get_child(packet, "error"))) {
		cdata = xmlnode_get_data(error);
		code = xmlnode_get_attrib(error, "code");

		/* Stanza errors */
		if(xmlnode_get_child(error, "bad-request")) {
			text = _("Bad Request");
		} else if(xmlnode_get_child(error, "conflict")) {
			text = _("Conflict");
		} else if(xmlnode_get_child(error, "feature-not-implemented")) {
			text = _("Feature Not Implemented");
		} else if(xmlnode_get_child(error, "forbidden")) {
			text = _("Forbidden");
		} else if(xmlnode_get_child(error, "gone")) {
			text = _("Gone");
		} else if(xmlnode_get_child(error, "internal-server-error")) {
			text = _("Internal Server Error");
		} else if(xmlnode_get_child(error, "item-not-found")) {
			text = _("Item Not Found");
		} else if(xmlnode_get_child(error, "jid-malformed")) {
			text = _("Malformed Jabber ID");
		} else if(xmlnode_get_child(error, "not-acceptable")) {
			text = _("Not Acceptable");
		} else if(xmlnode_get_child(error, "not-allowed")) {
			text = _("Not Allowed");
		} else if(xmlnode_get_child(error, "not-authorized")) {
			text = _("Not Authorized");
		} else if(xmlnode_get_child(error, "payment-required")) {
			text = _("Payment Required");
		} else if(xmlnode_get_child(error, "recipient-unavailable")) {
			text = _("Recipient Unavailable");
		} else if(xmlnode_get_child(error, "redirect")) {
			/* XXX */
		} else if(xmlnode_get_child(error, "registration-required")) {
			text = _("Registration Required");
		} else if(xmlnode_get_child(error, "remote-server-not-found")) {
			text = _("Remote Server Not Found");
		} else if(xmlnode_get_child(error, "remote-server-timeout")) {
			text = _("Remote Server Timeout");
		} else if(xmlnode_get_child(error, "resource-constraint")) {
			text = _("Server Overloaded");
		} else if(xmlnode_get_child(error, "service-unavailable")) {
			text = _("Service Unavailable");
		} else if(xmlnode_get_child(error, "subscription-required")) {
			text = _("Subscription Required");
		} else if(xmlnode_get_child(error, "unexpected-request")) {
			text = _("Unexpected Request");
		} else if(xmlnode_get_child(error, "undefined-condition")) {
			text = _("Unknown Error");
		}
	} else if(xmlns && !strcmp(xmlns, "urn:ietf:params:xml:ns:xmpp-sasl")) {
		if(xmlnode_get_child(packet, "aborted")) {
			js->gc->wants_to_die = TRUE;
			text = _("Authorization Aborted");
		} else if(xmlnode_get_child(error, "incorrect-encoding")) {
			text = _("Incorrect encoding in authorization");
		} else if(xmlnode_get_child(error, "invalid-authzid")) {
			js->gc->wants_to_die = TRUE;
			text = _("Invalid authzid");
		} else if(xmlnode_get_child(error, "invalid-mechanism")) {
			js->gc->wants_to_die = TRUE;
			text = _("Invalid Authorization Mechanism");
		} else if(xmlnode_get_child(error, "mechanism-too-weak")) {
			js->gc->wants_to_die = TRUE;
			text = _("Authorization mechanism too weak");
		} else if(xmlnode_get_child(error, "not-authorized")) {
			js->gc->wants_to_die = TRUE;
			text = _("Not Authorized");
		} else if(xmlnode_get_child(error, "temporary-auth-failure")) {
			text = _("Temporary Authentication Failure");
		} else {
			text = _("Authentication Failure");
		}
	} else if(!strcmp(packet->name, "stream:error")) {
		if(xmlnode_get_child(packet, "bad-format")) {
			text = _("Bad Format");
		} else if(xmlnode_get_child(packet, "bad-namespace-prefix")) {
			text = _("Bad Namespace Prefix");
		} else if(xmlnode_get_child(packet, "conflict")) {
			js->gc->wants_to_die = TRUE;
			text = _("Resource Conflict");
		} else if(xmlnode_get_child(packet, "connection-timeout")) {
			text = _("Connection Timeout");
		} else if(xmlnode_get_child(packet, "host-gone")) {
			text = _("Host Gone");
		} else if(xmlnode_get_child(packet, "host-unknown")) {
			text = _("Host Unknown");
		} else if(xmlnode_get_child(packet, "improper-addressing")) {
			text = _("Improper Addressing");
		} else if(xmlnode_get_child(packet, "internal-server-error")) {
			text = _("Internal Server Error");
		} else if(xmlnode_get_child(packet, "invalid-id")) {
			text = _("Invalid ID");
		} else if(xmlnode_get_child(packet, "invalid-namespace")) {
			text = _("Invalid Namespace");
		} else if(xmlnode_get_child(packet, "invalid-xml")) {
			text = _("Invalid XML");
		} else if(xmlnode_get_child(packet, "nonmatching-hosts")) {
			text = _("Non-matching Hosts");
		} else if(xmlnode_get_child(packet, "not-authorized")) {
			text = _("Not Authorized");
		} else if(xmlnode_get_child(packet, "policy-violation")) {
			text = _("Policy Violation");
		} else if(xmlnode_get_child(packet, "remote-connection-failed")) {
			text = _("Remote Connection Failed");
		} else if(xmlnode_get_child(packet, "resource-constraint")) {
			text = _("Resource Constraint");
		} else if(xmlnode_get_child(packet, "restricted-xml")) {
			text = _("Restricted XML");
		} else if(xmlnode_get_child(packet, "see-other-host")) {
			text = _("See Other Host");
		} else if(xmlnode_get_child(packet, "system-shutdown")) {
			text = _("System Shutdown");
		} else if(xmlnode_get_child(packet, "undefined-condition")) {
			text = _("Undefined Condition");
		} else if(xmlnode_get_child(packet, "unsupported-encoding")) {
			text = _("Unsupported Encoding");
		} else if(xmlnode_get_child(packet, "unsupported-stanza-type")) {
			text = _("Unsupported Stanza Type");
		} else if(xmlnode_get_child(packet, "unsupported-version")) {
			text = _("Unsupported Version");
		} else if(xmlnode_get_child(packet, "xml-not-well-formed")) {
			text = _("XML Not Well Formed");
		} else {
			text = _("Stream Error");
		}
	}

	if(text || cdata) {
		char *ret = g_strdup_printf("%s%s%s", code ? code : "",
				code ? ": " : "", text ? text : cdata);
		g_free(cdata);
		return ret;
	} else {
		return NULL;
	}
}

static GaimCmdRet jabber_cmd_chat_config(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);
	jabber_chat_request_room_configure(chat);
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_register(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);
	jabber_chat_register(chat);
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_topic(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);
	jabber_chat_change_topic(chat, args ? args[0] : NULL);
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_nick(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);

	if(!args || !args[0])
		return GAIM_CMD_RET_FAILED;

	jabber_chat_change_nick(chat, args[0]);
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_part(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);
	jabber_chat_part(chat, args ? args[0] : NULL);
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_ban(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);

	if(!args || !args[0])
		return GAIM_CMD_RET_FAILED;

	if(!jabber_chat_ban_user(chat, args[0], args[1])) {
		*error = g_strdup_printf(_("Unable to ban user %s"), args[0]);
		return GAIM_CMD_RET_FAILED;
	}

	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_invite(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	if(!args || !args[0])
		return GAIM_CMD_RET_FAILED;

	jabber_chat_invite(gaim_conversation_get_gc(conv),
			gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)), args[1] ? args[1] : "",
			args[0]);

	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_join(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);
	GHashTable *components;

	if(!args || !args[0])
		return GAIM_CMD_RET_FAILED;

	components = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	g_hash_table_replace(components, "room", args[0]);
	g_hash_table_replace(components, "server", chat->server);
	g_hash_table_replace(components, "handle", chat->handle);
	if(args[1])
		g_hash_table_replace(components, "password", args[1]);

	jabber_chat_join(gaim_conversation_get_gc(conv), components);

	g_hash_table_destroy(components);
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_kick(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);

	if(!args || !args[0])
		return GAIM_CMD_RET_FAILED;

	if(!jabber_chat_kick_user(chat, args[0], args[1])) {
		*error = g_strdup_printf(_("Unable to kick user %s"), args[0]);
		return GAIM_CMD_RET_FAILED;
	}

	return GAIM_CMD_RET_OK;
}

static GaimCmdRet jabber_cmd_chat_msg(GaimConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(conv);
	char *who;

	who = g_strdup_printf("%s@%s/%s", chat->room, chat->server, args[0]);

	jabber_message_send_im(gaim_conversation_get_gc(conv), who, args[1], 0);

	g_free(who);
	return GAIM_CMD_RET_OK;
}

static void jabber_register_commands(void)
{
	gaim_cmd_register("config", "", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
	                  "prpl-jabber", jabber_cmd_chat_config,
	                  _("config:  Configure a chat room."), NULL);
	gaim_cmd_register("configure", "", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
	                  "prpl-jabber", jabber_cmd_chat_config,
	                  _("configure:  Configure a chat room."), NULL);
	gaim_cmd_register("nick", "s", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
	                  "prpl-jabber", jabber_cmd_chat_nick,
	                  _("nick &lt;new nickname&gt;:  Change your nickname."),
	                  NULL);
	gaim_cmd_register("part", "s", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
	                  GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-jabber",
	                  jabber_cmd_chat_part, _("part [room]:  Leave the room."),
	                  NULL);
	gaim_cmd_register("register", "", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
	                  "prpl-jabber", jabber_cmd_chat_register,
	                  _("register:  Register with a chat room."), NULL);
	/* XXX: there needs to be a core /topic cmd, methinks */
	gaim_cmd_register("topic", "s", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
	                  GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-jabber",
	                  jabber_cmd_chat_topic,
	                  _("topic [new topic]:  View or change the topic."),
	                  NULL);
	gaim_cmd_register("ban", "ws", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
	                  GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-jabber",
	                  jabber_cmd_chat_ban,
	                  _("ban &lt;user&gt; [room]:  Ban a user from the room."),
	                  NULL);
	gaim_cmd_register("invite", "ws", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
	                  GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-jabber",
	                  jabber_cmd_chat_invite,
	                  _("invite &lt;user&gt; [message]:  Invite a user to the room."),
	                  NULL);
	gaim_cmd_register("join", "ws", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
	                  GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-jabber",
	                  jabber_cmd_chat_join,
	                  _("join: &lt;room&gt; [server]:  Join a chat on this server."),
	                  NULL);
	gaim_cmd_register("kick", "ws", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
	                  GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-jabber",
	                  jabber_cmd_chat_kick,
	                  _("kick &lt;user&gt; [room]:  Kick a user from the room."),
	                  NULL);
	gaim_cmd_register("msg", "ws", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
	                  "prpl-jabber", jabber_cmd_chat_msg,
	                  _("msg &lt;user&gt; &lt;message&gt;:  Send a private message to another user."),
	                  NULL);
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin) {
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	frame = gaim_plugin_pref_frame_new();

	ppref = gaim_plugin_pref_new_with_label(_("Privacy"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label("/plugins/prpl/jabber/hide_os",
													 _("Hide Operating System"));
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame
};

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME,
	NULL,							/* user_splits */
	NULL,							/* protocol_options */
	NO_BUDDY_ICONS,					/* icon_spec */
	jabber_list_icon,				/* list_icon */
	jabber_list_emblems,			/* list_emblems */
	jabber_status_text,				/* status_text */
	jabber_tooltip_text,			/* tooltip_text */
	jabber_away_states,				/* away_states */
	jabber_blist_node_menu,			/* blist_node_menu */
	jabber_chat_info,				/* chat_info */
	jabber_chat_info_defaults,		/* chat_info_defaults */
	jabber_login,					/* login */
	jabber_close,					/* close */
	jabber_message_send_im,			/* send_im */
	jabber_set_info,				/* set_info */
	jabber_send_typing,				/* send_typing */
	jabber_buddy_get_info,			/* get_info */
	jabber_presence_send,			/* set_away */
	jabber_idle_set,				/* set_idle */
	NULL,							/* change_passwd */
	jabber_roster_add_buddy,		/* add_buddy */
	NULL,							/* add_buddies */
	jabber_roster_remove_buddy,		/* remove_buddy */
	NULL,							/* remove_buddies */
	NULL,							/* add_permit */
	NULL,							/* add_deny */
	NULL,							/* rem_permit */
	NULL,							/* rem_deny */
	NULL,							/* set_permit_deny */
	NULL,							/* warn */
	jabber_chat_join,				/* join_chat */
	NULL,							/* reject_chat */
	jabber_get_chat_name,			/* get_chat_name */
	jabber_chat_invite,				/* chat_invite */
	jabber_chat_leave,				/* chat_leave */
	NULL,							/* chat_whisper */
	jabber_message_send_chat,		/* chat_send */
	jabber_keepalive,				/* keepalive */
	jabber_register_account,		/* register_user */
	jabber_buddy_get_info_chat,		/* get_cb_info */
	NULL,							/* get_cb_away */
	jabber_roster_alias_change,		/* alias_buddy */
	jabber_roster_group_change,		/* group_buddy */
	jabber_roster_group_rename,		/* rename_group */
	NULL,							/* buddy_free */
	jabber_convo_closed,			/* convo_closed */
	jabber_normalize,				/* normalize */
	NULL,							/* set_buddy_icon */
	NULL,							/* remove_group */
	jabber_chat_buddy_real_name,	/* get_cb_real_name */
	jabber_chat_set_topic,			/* set_chat_topic */
	jabber_find_blist_chat,			/* find_blist_chat */
	jabber_roomlist_get_list,		/* roomlist_get_list */
	jabber_roomlist_cancel,			/* roomlist_cancel */
	NULL,							/* roomlist_expand_category */
	NULL,							/* can_receive_file */
	jabber_si_xfer_send				/* send_file */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-jabber",                                    /**< id             */
	"Jabber",                                         /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Jabber Protocol Plugin"),
	                                                  /**  description    */
	N_("Jabber Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	jabber_actions
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;

	split = gaim_account_user_split_new(_("Server"), "jabber.org", '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	split = gaim_account_user_split_new(_("Resource"), "Gaim", '/');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	option = gaim_account_option_bool_new(_("Use TLS if available"), "use_tls",
			TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	option = gaim_account_option_bool_new(_("Force old SSL"), "old_ssl", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	option = gaim_account_option_bool_new(
			_("Allow plaintext auth over unencrypted streams"),
			"auth_plain_in_clear", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	option = gaim_account_option_int_new(_("Port"), "port", 5222);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	option = gaim_account_option_string_new(_("Connect server"),
			"connect_server", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	my_protocol = plugin;

	gaim_prefs_add_none("/plugins/prpl/jabber");
	gaim_prefs_add_bool("/plugins/prpl/jabber/hide_os", FALSE);

	jabber_register_commands();
}

GAIM_INIT_PLUGIN(jabber, init_plugin, info);
