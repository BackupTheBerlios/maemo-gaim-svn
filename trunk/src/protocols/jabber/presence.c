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

#include "debug.h"
#include "notify.h"
#include "request.h"
#include "server.h"
#include "util.h"

#include "buddy.h"
#include "chat.h"
#include "presence.h"
#include "iq.h"
#include "jutil.h"
#include "xmlnode.h"


static void chats_send_presence_foreach(gpointer key, gpointer val,
		gpointer user_data)
{
	JabberChat *chat = val;
	xmlnode *presence = user_data;
	char *chat_full_jid;

	if(!chat->conv)
		return;

	chat_full_jid = g_strdup_printf("%s@%s/%s", chat->room, chat->server,
			chat->handle);

	xmlnode_set_attrib(presence, "to", chat_full_jid);
	jabber_send(chat->js, presence);
	g_free(chat_full_jid);
}

void jabber_presence_fake_to_self(JabberStream *js, const char *away_state, const char *msg) {
	char *my_base_jid;

	if(!js->user)
		return;

	my_base_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);

	if(gaim_find_buddy(js->gc->account, my_base_jid)) {
		JabberBuddy *jb;
		JabberBuddyResource *jbr;
		if((jb = jabber_buddy_find(js, my_base_jid, TRUE))) {
			int state = 0;
			if(away_state) {
				if(!strcmp(away_state, _("Away")) ||
						(msg && *msg && !strcmp(away_state, GAIM_AWAY_CUSTOM)))
					state = JABBER_STATE_AWAY;
				else if(!strcmp(away_state, _("Chatty")))
					state = JABBER_STATE_CHAT;
				else if(!strcmp(away_state, _("Extended Away")))
					state = JABBER_STATE_XA;
				else if(!strcmp(away_state, _("Do Not Disturb")))
					state = JABBER_STATE_DND;
			}
		       
			if (away_state && !strcmp(away_state, "unavailable")) {
				jabber_buddy_remove_resource(jb, js->user->resource);
			} else {
				jabber_buddy_track_resource(jb, js->user->resource, 0, state, (msg && *msg) ? msg : NULL);
			}
			if((jbr = jabber_buddy_find_resource(jb, NULL)))
				serv_got_update(js->gc, my_base_jid, TRUE, 0, 0, 0, jbr->state);
			else
				serv_got_update(js->gc, my_base_jid, FALSE, 0, 0, 0, 0);
		}
	}
	g_free(my_base_jid);
}


void jabber_presence_send(GaimConnection *gc, const char *state,
		const char *msg)
{
	JabberStream *js = gc->proto_data;
	xmlnode *presence;
	char *stripped = NULL;

	if(msg) {
		gaim_markup_html_to_xhtml(msg, NULL, &stripped);
	} else if(!state || strcmp(state, GAIM_AWAY_CUSTOM)) {
		/* i can't wait until someone re-writes the status/away stuff */
		stripped = g_strdup("");
	}

	if(gc->away)
		g_free(gc->away);
	gc->away = stripped;

	presence = jabber_presence_create(state, stripped);
	jabber_send(js, presence);
	g_hash_table_foreach(js->chats, chats_send_presence_foreach, presence);
	xmlnode_free(presence);

	jabber_presence_fake_to_self(js, state, stripped);
}

xmlnode *jabber_presence_create(const char *state, const char *msg)
{
	xmlnode *show, *status, *presence;


	presence = xmlnode_new("presence");

	if(state) {
		const char *show_string = NULL;
		if(!strcmp(state, _("Chatty")))
			show_string = "chat";
		else if(!strcmp(state, _("Away")) ||
				(msg && !strcmp(state, GAIM_AWAY_CUSTOM)))
			show_string = "away";
		else if(!strcmp(state, _("Extended Away")))
			show_string = "xa";
		else if(!strcmp(state, _("Do Not Disturb")))
			show_string = "dnd";
		else if(!strcmp(state, _("Invisible")))
			xmlnode_set_attrib(presence, "type", "invisible");
		else if(!strcmp(state, "unavailable"))
			xmlnode_set_attrib(presence, "type", "unavailable");

		if(show_string) {
			show = xmlnode_new_child(presence, "show");
			xmlnode_insert_data(show, show_string, -1);
		}
	}

	if(msg && *msg) {
		status = xmlnode_new_child(presence, "status");
		xmlnode_insert_data(status, msg, -1);
	}

	return presence;
}

struct _jabber_add_permit {
	JabberStream *js;
	GaimConnection *gc;
	char *who;
};

static void free_jabber_add_permit(struct _jabber_add_permit *jap)
{
	g_free(jap->who);
	g_free(jap);
}

static void add_user_cb(struct _jabber_add_permit *jap)
{
	if (g_list_find(gaim_connections_get_all(), jap->gc))
	{
		gaim_blist_request_add_buddy(jap->gc->account, jap->who,
		                             NULL, NULL);
	}

	free_jabber_add_permit(jap);
}

static void authorize_add_cb(struct _jabber_add_permit *jap)
{
	if(g_list_find(gaim_connections_get_all(), jap->gc)) {
		GaimBuddy *buddy = NULL;
		jabber_presence_subscription_set(jap->gc->proto_data, jap->who,
				"subscribed");

		buddy = gaim_find_buddy(jap->gc->account, jap->who);

		if (buddy) {
			JabberBuddy *jb = NULL;

			jb = jabber_buddy_find(jap->js, jap->who, TRUE);

			if ((jb->subscription & JABBER_SUB_TO) == 0) {
				char *buffer = NULL;
				struct _jabber_add_permit *jap2 = g_new0(struct _jabber_add_permit, 1);
				jap2->gc = jap->gc;
				jap2->who = g_strdup(jap->who);

				/* XXX: Tidy this up when not in string freeze */
				buffer = g_strdup_printf(_("%s%s%s%s has made %s his or her buddy%s%s%s"),
				                         jap->who, "", "", "",
				                         (gaim_connection_get_display_name(jap->gc) != NULL
				                            ? gaim_connection_get_display_name(jap->gc)
				                            : gaim_account_get_username(jap->gc->account)),
				                         ".", "",
				                         _("\n\nDo you wish to add him or her to your buddy list?"));

				gaim_request_action(NULL, NULL, _("Add buddy to your list?"),
				                    buffer, GAIM_DEFAULT_ACTION_NONE, jap2, 2,
				                    _("Add"),    G_CALLBACK(add_user_cb),
				                    _("Cancel"), G_CALLBACK(free_jabber_add_permit));

				g_free(buffer);

			} else
				gaim_account_notify_added(jap->gc->account, NULL, jap->who, NULL, NULL);
		} else {
			gaim_account_notify_added(jap->gc->account, NULL, jap->who, NULL, NULL);
		}
	}

	g_free(jap->who);
	g_free(jap);
}

static void deny_add_cb(struct _jabber_add_permit *jap)
{
	if(g_list_find(gaim_connections_get_all(), jap->gc)) {
		jabber_presence_subscription_set(jap->gc->proto_data, jap->who,
				"unsubscribed");
	}

	g_free(jap->who);
	g_free(jap);
}

static int show_to_state(const char *show) {
	if(!show)
		return 0;
	else if(!strcmp(show, "away"))
		return JABBER_STATE_AWAY;
	else if(!strcmp(show, "chat"))
		return JABBER_STATE_CHAT;
	else if(!strcmp(show, "xa"))
		return JABBER_STATE_XA;
	else if(!strcmp(show, "dnd"))
		return JABBER_STATE_DND;
	return 0;
}

void jabber_presence_parse(JabberStream *js, xmlnode *packet)
{
	const char *from = xmlnode_get_attrib(packet, "from");
	const char *type = xmlnode_get_attrib(packet, "type");
	const char *real_jid = NULL;
	const char *affiliation = NULL;
	const char *role = NULL;
	char *status = NULL;
	int priority = 0;
	JabberID *jid;
	JabberChat *chat;
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL;
	GaimConvChatBuddyFlags flags = GAIM_CBFLAGS_NONE;
	gboolean delayed = FALSE;
	GaimBuddy *b;
	char *buddy_name;
	int state = 0;
	xmlnode *y;
	gboolean muc = FALSE;


	if(!(jb = jabber_buddy_find(js, from, TRUE)))
		return;

	if(!(jid = jabber_id_new(from)))
		return;

	if(jb->error_msg) {
		g_free(jb->error_msg);
		jb->error_msg = NULL;
	}

	if(type && !strcmp(type, "error")) {
		char *msg = jabber_parse_error(js, packet);

		state = JABBER_STATE_ERROR;
		jb->error_msg = msg ? msg : g_strdup(_("Unknown Error in presence"));
	} else if(type && !strcmp(type, "subscribe")) {
		struct _jabber_add_permit *jap = g_new0(struct _jabber_add_permit, 1);
		char *msg = g_strdup_printf(_("The user %s wants to add you to their buddy list."), from);
		jap->gc = js->gc;
		jap->who = g_strdup(from);
		jap->js = js;

		gaim_request_action(js->gc, NULL, msg, NULL, GAIM_DEFAULT_ACTION_NONE, 
				jap, 2,
				_("Authorize"), G_CALLBACK(authorize_add_cb),
				_("Deny"), G_CALLBACK(deny_add_cb));
		g_free(msg);
		jabber_id_free(jid);
		return;
	} else if(type && !strcmp(type, "subscribed")) {
		/* we've been allowed to see their presence, but we don't care */
		jabber_id_free(jid);
		return;
	} else {
		if((y = xmlnode_get_child(packet, "show"))) {
			char *show = xmlnode_get_data(y);
			state = show_to_state(show);
			g_free(show);
		} else {
			state = 0;
		}
	}


	for(y = packet->child; y; y = y->next) {
		if(y->type != XMLNODE_TYPE_TAG)
			continue;

		if(!strcmp(y->name, "status")) {
			g_free(status);
			status = xmlnode_get_data(y);
		} else if(!strcmp(y->name, "priority")) {
			char *p = xmlnode_get_data(y);
			if(p) {
				priority = atoi(p);
				g_free(p);
			}
		} else if(!strcmp(y->name, "x")) {
			const char *xmlns = xmlnode_get_attrib(y, "xmlns");
			if(xmlns && !strcmp(xmlns, "jabber:x:delay")) {
				/* XXX: compare the time.  jabber:x:delay can happen on presence packets that aren't really and truly delayed */
				delayed = TRUE;
			} else if(xmlns && !strcmp(xmlns, "http://jabber.org/protocol/muc#user")) {
				xmlnode *z;

				muc = TRUE;
				if((z = xmlnode_get_child(y, "status"))) {
					const char *code = xmlnode_get_attrib(z, "code");
					if(code && !strcmp(code, "201")) {
						chat = jabber_chat_find(js, jid->node, jid->domain);
						chat->config_dialog_type = GAIM_REQUEST_ACTION;
						chat->config_dialog_handle =
							gaim_request_action(js->gc, _("Create New Room"),
								_("Create New Room"),
								_("You are creating a new room.  Would you like to "
									"configure it, or accept the default settings?"),
								1, chat, 2, _("Configure Room"),
								G_CALLBACK(jabber_chat_request_room_configure),
								_("Accept Defaults"),
								G_CALLBACK(jabber_chat_create_instant_room));
					}
				}
				if((z = xmlnode_get_child(y, "item"))) {
					real_jid = xmlnode_get_attrib(z, "jid");
					affiliation = xmlnode_get_attrib(z, "affiliation");
					role = xmlnode_get_attrib(z, "role");
					if(affiliation != NULL && !strcmp(affiliation, "owner"))
						flags |= GAIM_CBFLAGS_FOUNDER;
					if (role != NULL) {
						if (!strcmp(role, "moderator"))
							flags |= GAIM_CBFLAGS_OP;
						else if (!strcmp(role, "participant"))
							flags |= GAIM_CBFLAGS_VOICE;
					}
				}
			}
		}
	}


	if(jid->node && (chat = jabber_chat_find(js, jid->node, jid->domain))) {
		static int i = 1;
		char *room_jid = g_strdup_printf("%s@%s", jid->node, jid->domain);

		if(state == JABBER_STATE_ERROR) {
			char *title, *msg = jabber_parse_error(js, packet);

			if(chat->conv) {
				title = g_strdup_printf(_("Error in chat %s"), from);
				serv_got_chat_left(js->gc, chat->id);
			} else {
				title = g_strdup_printf(_("Error joining chat %s"), from);
			}
			gaim_notify_error(js->gc, title, title, msg);
			g_free(title);
			g_free(msg);

			jabber_chat_destroy(chat);
			jabber_id_free(jid);
			g_free(status);
			g_free(room_jid);
			return;
		}


		if(type && !strcmp(type, "unavailable")) {
			gboolean nick_change = FALSE;

			/* If we haven't joined the chat yet, we don't care that someone
			 * left, or it was us leaving after we closed the chat */
			if(!chat->conv) {
				if(jid->resource && chat->handle && !strcmp(jid->resource, chat->handle))
					jabber_chat_destroy(chat);
				jabber_id_free(jid);
				g_free(status);
				g_free(room_jid);
				return;
			}

			jabber_buddy_remove_resource(jb, jid->resource);
			if(chat->muc) {
				xmlnode *x;
				for(x = xmlnode_get_child(packet, "x"); x; x = xmlnode_get_next_twin(x)) {
					const char *xmlns, *nick, *code;
					xmlnode *stat, *item;
					if(!(xmlns = xmlnode_get_attrib(x, "xmlns")) ||
							strcmp(xmlns, "http://jabber.org/protocol/muc#user"))
						continue;
					if(!(stat = xmlnode_get_child(x, "status")))
						continue;
					if(!(code = xmlnode_get_attrib(stat, "code")))
						continue;
					if(!strcmp(code, "301")) {
						/* XXX: we got banned */
					} else if(!strcmp(code, "303")) {
						if(!(item = xmlnode_get_child(x, "item")))
							continue;
						if(!(nick = xmlnode_get_attrib(item, "nick")))
							continue;
						nick_change = TRUE;
						if(!strcmp(jid->resource, chat->handle)) {
							g_free(chat->handle);
							chat->handle = g_strdup(nick);
						}
						gaim_conv_chat_rename_user(GAIM_CONV_CHAT(chat->conv), jid->resource, nick);
						jabber_chat_remove_handle(chat, jid->resource);
						break;
					} else if(!strcmp(code, "307")) {
						/* XXX: we got kicked */
					} else if(!strcmp(code, "321")) {
						/* XXX: removed due to an affiliation change */
					} else if(!strcmp(code, "322")) {
						/* XXX: removed because room is now members-only */
					} else if(!strcmp(code, "332")) {
						/* XXX: removed due to system shutdown */
					}
				}
			}
			if(!nick_change) {
				if(!g_utf8_collate(jid->resource, chat->handle)) {
					serv_got_chat_left(js->gc, chat->id);
					jabber_chat_destroy(chat);
				} else {
					gaim_conv_chat_remove_user(GAIM_CONV_CHAT(chat->conv), jid->resource,
							status);
					jabber_chat_remove_handle(chat, jid->resource);
				}
			}
		} else {
			if(!chat->conv) {
				chat->id = i++;
				chat->muc = muc;
				chat->conv = serv_got_joined_chat(js->gc, chat->id, room_jid);
				gaim_conv_chat_set_nick(GAIM_CONV_CHAT(chat->conv), chat->handle);

				jabber_chat_disco_traffic(chat);
			}

			jabber_buddy_track_resource(jb, jid->resource, priority, state,
					status);

			jabber_chat_track_handle(chat, jid->resource, real_jid, affiliation, role);

			if(!jabber_chat_find_buddy(chat->conv, jid->resource))
				gaim_conv_chat_add_user(GAIM_CONV_CHAT(chat->conv), jid->resource,
						real_jid, flags, !delayed);
			else
				gaim_conv_chat_user_set_flags(GAIM_CONV_CHAT(chat->conv), jid->resource,
						flags);
		}
		g_free(room_jid);
	} else {
		if(state != JABBER_STATE_ERROR && !(jb->subscription & JABBER_SUB_TO || jb->subscription & JABBER_SUB_PENDING)) {
			gaim_debug(GAIM_DEBUG_INFO, "jabber",
					"got unexpected presence from %s, ignoring\n", from);
			jabber_id_free(jid);
			g_free(status);
			return;
		}

		buddy_name = g_strdup_printf("%s%s%s", jid->node ? jid->node : "",
				jid->node ? "@" : "", jid->domain);
		if((b = gaim_find_buddy(js->gc->account, buddy_name)) == NULL) {
			jabber_id_free(jid);
			g_free(buddy_name);
			g_free(status);
			return;
		}

		if(state == JABBER_STATE_ERROR ||
				(type && (!strcmp(type, "unavailable") ||
						  !strcmp(type, "unsubscribed")))) {
			GaimConversation *conv;

			jabber_buddy_remove_resource(jb, jid->resource);
			if((conv = jabber_find_unnormalized_conv(from, js->gc->account)))
				gaim_conversation_set_name(conv, buddy_name);

		} else {
			jabber_buddy_track_resource(jb, jid->resource, priority, state,
					status);
		}

		if((jbr = jabber_buddy_find_resource(jb, NULL)))
			serv_got_update(js->gc, buddy_name, TRUE, 0, b->signon, b->idle,
					jbr->state);
		else
			serv_got_update(js->gc, buddy_name, FALSE, 0, 0, 0, 0);

		g_free(buddy_name);
	}
	g_free(status);
	jabber_id_free(jid);
}

void jabber_presence_subscription_set(JabberStream *js, const char *who, const char *type)
{
	xmlnode *presence = xmlnode_new("presence");

	xmlnode_set_attrib(presence, "to", who);
	xmlnode_set_attrib(presence, "type", type);

	jabber_send(js, presence);
	xmlnode_free(presence);
}
