/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "util.h"
#include "version.h"

#include "sha.h"
#include "yahoo.h"
#include "yahoo_friend.h"
#include "yahoochat.h"
#include "ycht.h"
#include "yahoo_auth.h"
#include "yahoo_filexfer.h"
#include "yahoo_picture.h"
#include "md5.h"

extern char *yahoo_crypt(const char *, const char *);

/* #define YAHOO_DEBUG */

static void yahoo_add_buddy(GaimConnection *gc, GaimBuddy *, GaimGroup *);
static void yahoo_login_page_cb(void *user_data, const char *buf, size_t len);


static void
yahoo_add_permit(GaimConnection *gc, const char *who)
{
	gaim_debug_info("yahoo",
			"Permitting ID %s local contact rights for account %s\n", who, gc->account);	
	gaim_privacy_permit_add(gc->account,who,TRUE);
}

static void
yahoo_rem_permit(GaimConnection *gc, const char *who)
{
	gaim_debug_info("yahoo",
			"Denying ID %s local contact rights for account %s\n", who, gc->account);	
	gaim_privacy_permit_remove(gc->account,who,TRUE);
}

gboolean yahoo_privacy_check(GaimConnection *gc, const char *who)
{
	/* returns TRUE if allowed through, FALSE otherwise */
	GSList *list;
	gboolean permitted=FALSE;

	switch (gc->account->perm_deny) {
		case GAIM_PRIVACY_ALLOW_ALL:
			permitted = TRUE;
			break;

		case GAIM_PRIVACY_DENY_ALL:
			gaim_debug_info("yahoo",
			    "%s blocked data received from %s (GAIM_PRIVACY_DENY_ALL)\n",
			    gc->account->username,who);
			break;

		case GAIM_PRIVACY_ALLOW_USERS:
			for( list=gc->account->permit; list!=NULL; list=list->next ) {
				if ( !gaim_utf8_strcasecmp(who, gaim_normalize(gc->account, (char *)list->data)) ) {
					permitted=TRUE;
					gaim_debug_info("yahoo",
					    "%s allowed data received from %s (GAIM_PRIVACY_ALLOW_USERS)\n",
					    gc->account->username,who);
					break;
				}
			}
			break;

		case GAIM_PRIVACY_DENY_USERS:
			/* seeing we're letting everyone through, except the deny list*/
			permitted=TRUE;
			for( list=gc->account->deny; list!=NULL; list=list->next ) {
				if ( !gaim_utf8_strcasecmp(who, gaim_normalize( gc->account, (char *)list->data )) ) {
					permitted=FALSE;
					gaim_debug_info("yahoo",
					    "%s blocked data received from %s (GAIM_PRIVACY_DENY_USERS)\n",
					    gc->account->username,who);
					break;
				}
			}
			break;

		case GAIM_PRIVACY_ALLOW_BUDDYLIST:
			if ( gaim_find_buddy(gc->account,who) != NULL ) {
				permitted = TRUE;
			} else {
				gaim_debug_info("yahoo",
				    "%s blocked data received from %s (GAIM_PRIVACY_ALLOW_BUDDYLIST)\n",
				    gc->account->username,who);
			}
		break;

		default:
			gaim_debug_warning("yahoo", "Privacy setting was unknown.  If you can "
							   "reproduce this, please file a bug report.\n");
			permitted = FALSE;
			break;
	}

	return permitted;
}

struct yahoo_packet *yahoo_packet_new(enum yahoo_service service, enum yahoo_status status, int id)
{
	struct yahoo_packet *pkt = g_new0(struct yahoo_packet, 1);

	pkt->service = service;
	pkt->status = status;
	pkt->id = id;

	return pkt;
}

void yahoo_packet_hash(struct yahoo_packet *pkt, int key, const char *value)
{
	struct yahoo_pair *pair = g_new0(struct yahoo_pair, 1);
	pair->key = key;
	pair->value = g_strdup(value);
	pkt->hash = g_slist_append(pkt->hash, pair);
}

int yahoo_packet_length(struct yahoo_packet *pkt)
{
	GSList *l;

	int len = 0;

	l = pkt->hash;
	while (l) {
		struct yahoo_pair *pair = l->data;
		int tmp = pair->key;
		do {
			tmp /= 10;
			len++;
		} while (tmp);
		len += 2;
		len += strlen(pair->value);
		len += 2;
		l = l->next;
	}

	return len;
}

static void yahoo_packet_read(struct yahoo_packet *pkt, guchar *data, int len)
{
	int pos = 0;

	while (pos + 1 < len) {
		char key[64], *value = NULL, *esc;
		int accept;
		int x;

		struct yahoo_pair *pair = g_new0(struct yahoo_pair, 1);

		/* this is weird, and in one of the chat packets, and causes us
		 * think all the values are keys and all the keys are values after
		 * this point if we don't handle it */
		if (data[pos] == '\0') {
			while (pos + 1 < len) {
				if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
					break;
				pos++;
			}
			pos += 2;
			g_free(pair);
			continue;
		}

		x = 0;
		while (pos + 1 < len) {
			if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
				break;
			if (x >= sizeof(key)-1) {
				x++;
				pos++;
				continue;
			}
			key[x++] = data[pos++];
		}
		if (x >= sizeof(key)-1) {
			x = 0;
		}
		key[x] = 0;
		pos += 2;
		pair->key = strtol(key, NULL, 10);
		accept = x; /* if x is 0 there was no key, so don't accept it */

		if (len - pos + 1 <= 0) {
			/* Truncated. Garbage or something. */
			accept = 0;
		}

		if (accept) {
			value = g_malloc(len - pos + 1);
			x = 0;
			while (pos + 1 < len) {
				if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
					break;
				value[x++] = data[pos++];
			}
			value[x] = 0;
			pair->value = g_strdup(value);
			g_free(value);
			pkt->hash = g_slist_append(pkt->hash, pair);
			esc = g_strescape(pair->value, NULL);
			gaim_debug(GAIM_DEBUG_MISC, "yahoo",
					   "Key: %d  \tValue: %s\n", pair->key, esc);
			g_free(esc);
		} else {
			g_free(pair);
		}
		pos += 2;

		/* Skip over garbage we've noticed in the mail notifications */
		if (data[0] == '9' && data[pos] == 0x01)
			pos++;
	}
}

void yahoo_packet_write(struct yahoo_packet *pkt, guchar *data)
{
	GSList *l = pkt->hash;
	int pos = 0;

	while (l) {
		struct yahoo_pair *pair = l->data;
		guchar buf[100];

		g_snprintf(buf, sizeof(buf), "%d", pair->key);
		strcpy(data + pos, buf);
		pos += strlen(buf);
		data[pos++] = 0xc0;
		data[pos++] = 0x80;

		strcpy(data + pos, pair->value);
		pos += strlen(pair->value);
		data[pos++] = 0xc0;
		data[pos++] = 0x80;

		l = l->next;
	}
}

static void yahoo_packet_dump(guchar *data, int len)
{
#ifdef YAHOO_DEBUG
	int i;

	gaim_debug(GAIM_DEBUG_MISC, "yahoo", "");

	for (i = 0; i + 1 < len; i += 2) {
		if ((i % 16 == 0) && i) {
			gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
			gaim_debug(GAIM_DEBUG_MISC, "yahoo", "");
		}

		gaim_debug(GAIM_DEBUG_MISC, NULL, "%02x%02x ", data[i], data[i + 1]);
	}
	if (i < len)
		gaim_debug(GAIM_DEBUG_MISC, NULL, "%02x", data[i]);

	gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
	gaim_debug(GAIM_DEBUG_MISC, "yahoo", "");

	for (i = 0; i < len; i++) {
		if ((i % 16 == 0) && i) {
			gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
			gaim_debug(GAIM_DEBUG_MISC, "yahoo", "");
		}

		if (g_ascii_isprint(data[i]))
			gaim_debug(GAIM_DEBUG_MISC, NULL, "%c ", data[i]);
		else
			gaim_debug(GAIM_DEBUG_MISC, NULL, ". ");
	}

	gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
#endif
}

int yahoo_send_packet(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	int pktlen = yahoo_packet_length(pkt);
	int len = YAHOO_PACKET_HDRLEN + pktlen;
	int ret;

	guchar *data;
	int pos = 0;

	if (yd->fd < 0)
		return -1;

	data = g_malloc0(len + 1);

	memcpy(data + pos, "YMSG", 4); pos += 4;

	if (yd->wm)
		pos += yahoo_put16(data + pos, YAHOO_WEBMESSENGER_PROTO_VER);
	else
		pos += yahoo_put16(data + pos, YAHOO_PROTO_VER);

	pos += yahoo_put16(data + pos, 0x0000);
	pos += yahoo_put16(data + pos, pktlen);
	pos += yahoo_put16(data + pos, pkt->service);
	pos += yahoo_put32(data + pos, pkt->status);
	pos += yahoo_put32(data + pos, pkt->id);

	yahoo_packet_write(pkt, data + pos);

	yahoo_packet_dump(data, len);
	ret = write(yd->fd, data, len);
	if (ret != len)
		gaim_debug_warning("yahoo", "Only wrote %d of %d bytes!", ret, len);
	g_free(data);

	return ret;
}

int yahoo_send_packet_special(int fd, struct yahoo_packet *pkt, int pad)
{
	int pktlen = yahoo_packet_length(pkt);
	int len = YAHOO_PACKET_HDRLEN + pktlen;
	int ret;

	guchar *data;
	int pos = 0;

	if (fd < 0)
		return -1;

	data = g_malloc0(len + 1);

	memcpy(data + pos, "YMSG", 4); pos += 4;

	pos += yahoo_put16(data + pos, YAHOO_PROTO_VER);

	pos += yahoo_put16(data + pos, 0x0000);
	pos += yahoo_put16(data + pos, pktlen + pad);
	pos += yahoo_put16(data + pos, pkt->service);
	pos += yahoo_put32(data + pos, pkt->status);
	pos += yahoo_put32(data + pos, pkt->id);

	yahoo_packet_write(pkt, data + pos);

	ret = write(fd, data, len);
	g_free(data);

	return ret;
}

void yahoo_packet_free(struct yahoo_packet *pkt)
{
	while (pkt->hash) {
		struct yahoo_pair *pair = pkt->hash->data;
		g_free(pair->value);
		g_free(pair);
		pkt->hash = g_slist_remove(pkt->hash, pair);
	}
	g_free(pkt);
}

static void yahoo_update_status(GaimConnection *gc, const char *name, YahooFriend *f)
{
	gboolean online = TRUE;

	if (!gc || !name || !f || !gaim_find_buddy(gaim_connection_get_account(gc), name))
		return;

	if (f->status == YAHOO_STATUS_OFFLINE)
		online = FALSE;

	serv_got_update(gc, name, online, 0, 0, f->idle, f->away ? UC_UNAVAILABLE : 0);
}

static void yahoo_process_status(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = gc->proto_data;
	GSList *l = pkt->hash;
	YahooFriend *f = NULL;
	char *name = NULL;

	if (pkt->service == YAHOO_SERVICE_LOGOFF && pkt->status == -1) {
		gc->wants_to_die = TRUE;
		gaim_connection_error(gc, _("You have been logged off as you have logged in on a different machine or device."));
		return;
	}

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 0: /* we won't actually do anything with this */
			break;
		case 1: /* we don't get the full buddy list here. */
			if (!yd->logged_in) {
				gaim_connection_set_display_name(gc, pair->value);
				gaim_connection_set_state(gc, GAIM_CONNECTED);
				serv_finish_login(gc);
				yd->logged_in = TRUE;
				if (yd->picture_upload_todo) {
					yahoo_buddy_icon_upload(gc, yd->picture_upload_todo);
					yd->picture_upload_todo = NULL;
				}

				/* this requests the list. i have a feeling that this is very evil
				 *
				 * scs.yahoo.com sends you the list before this packet without  it being
				 * requested
				 *
				 * do_import(gc, NULL);
				 * newpkt = yahoo_packet_new(YAHOO_SERVICE_LIST, YAHOO_STATUS_OFFLINE, 0);
				 * yahoo_send_packet(yd, newpkt);
				 * yahoo_packet_free(newpkt);
				 */

				}
			break;
		case 8: /* how many online buddies we have */
			break;
		case 7: /* the current buddy */
			if (name && f) /* update the last buddy before setting a new one */
				yahoo_update_status(gc, name, f);

			name = pair->value;
			if (name && g_utf8_validate(name, -1, NULL))
				f = yahoo_friend_find_or_new(gc, name);
			else {
				f = NULL;
				name = NULL;
			}
			break;
		case 10: /* state */
			if (!f)
				break;

			f->status = strtol(pair->value, NULL, 10);
			if ((f->status >= YAHOO_STATUS_BRB) && (f->status <= YAHOO_STATUS_STEPPEDOUT))
				f->away = 1;
			else
				f->away = 0;
			if (f->status == YAHOO_STATUS_IDLE) {
				/* Idle may have already been set in a more precise way in case 137 */
				if (f->idle == 0)
					f->idle = time(NULL);
			} else
				f->idle = 0;
			if (f->status != YAHOO_STATUS_CUSTOM)
				yahoo_friend_set_status_message(f, NULL);

			f->sms = 0;
			break;
		case 19: /* custom message */
			if (f)
				yahoo_friend_set_status_message(f, yahoo_string_decode(gc, pair->value, FALSE));
			break;
		case 11: /* this is the buddy's session id */
			break;
		case 17: /* in chat? */
			break;
		case 47: /* is custom status away or not? 2=idle*/
			if (!f)
				break;

			/* I have no idea what it means when this is
			 * set when someone's available, but it doesn't
			 * mean idle. */
			if (f->status == YAHOO_STATUS_AVAILABLE)
				break;

			f->away = strtol(pair->value, NULL, 10);
			if (f->away == 2) {
				/* Idle may have already been set in a more precise way in case 137 */
				if (f->idle == 0)
 					f->idle = time(NULL);
			}

 			break;
		case 138: /* either we're not idle, or we are but won't say how long */
			if (!f)
				break;

			if (f->idle)
				f->idle = -1;
			break;
		case 137: /* usually idle time in seconds, sometimes login time */
			if (!f)
				break;

			if (f->status != YAHOO_STATUS_AVAILABLE)
				f->idle = time(NULL) - strtol(pair->value, NULL, 10);
			break;
		case 13: /* bitmask, bit 0 = pager, bit 1 = chat, bit 2 = game */
			if (strtol(pair->value, NULL, 10) == 0) {
				if (f)
					f->status = YAHOO_STATUS_OFFLINE;
				serv_got_update(gc, name, FALSE, 0, 0, 0, 0);
				break;
			}
			break;
		case 60: /* SMS */
			if (f) {
				f->sms = strtol(pair->value, NULL, 10);
				yahoo_update_status(gc, name, f);
			}
			break;
		case 197: /* Avatars */
		{
			char *decoded, *tmp;
			guint len;

			if (pair->value) {
				gaim_base64_decode(pair->value, &decoded, &len);
				if (len) {
					tmp = gaim_str_binary_to_ascii(decoded, len);
					gaim_debug_info("yahoo", "Got key 197, value = %s\n", tmp);
					g_free(tmp);
				}
				g_free(decoded);
			}
			break;
		}
		case 192: /* Pictures, aka Buddy Icons, checksum */
		{
			int cksum = strtol(pair->value, NULL, 10);
			GaimBuddy *b;

			if (!name)
				break;

			b = gaim_find_buddy(gc->account, name);

			if (!cksum || (cksum == -1)) {
				if (f)
					yahoo_friend_set_buddy_icon_need_request(f, TRUE);
				gaim_buddy_icons_set_for_user(gc->account, name, NULL, 0);
				if (b)
					gaim_blist_node_remove_setting((GaimBlistNode *)b, YAHOO_ICON_CHECKSUM_KEY);
				break;
			}

			if (!f)
				break;

			yahoo_friend_set_buddy_icon_need_request(f, FALSE);
			if (cksum != gaim_blist_node_get_int((GaimBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY))
				yahoo_send_picture_request(gc, name);

			break;
		}
		case 16: /* Custom error message */
			{
				char *tmp = yahoo_string_decode(gc, pair->value, TRUE);
				gaim_notify_error(gc, NULL, tmp, NULL);
				g_free(tmp);
			}
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "yahoo",
					   "Unknown status key %d\n", pair->key);
			break;
		}

		l = l->next;
	}

	if (name && f) /* there's not a next buddy after the last buddy,
	                  so this does for the last buddy what case 7 does
	                  for all the rest */
		yahoo_update_status(gc, name, f);
}

static void yahoo_do_group_check(GaimAccount *account, GHashTable *ht, const char *name, const char *group)
{
	GaimBuddy *b;
	GaimGroup *g;
	GSList *list, *i;
	gboolean onlist = 0;
	char *oname = NULL;

	char **oname_p = &oname;
	GSList **list_p = &list;

	if (!g_hash_table_lookup_extended(ht, gaim_normalize(account, name), (gpointer *) oname_p, (gpointer *) list_p))
		list = gaim_find_buddies(account, name);
	else
		g_hash_table_steal(ht, name);

	for (i = list; i; i = i->next) {
		b = i->data;
		g = gaim_find_buddys_group(b);
		if (!gaim_utf8_strcasecmp(group, g->name)) {
			gaim_debug(GAIM_DEBUG_MISC, "yahoo",
				"Oh good, %s is in the right group (%s).\n", name, group);
			list = g_slist_delete_link(list, i);
			onlist = 1;
			break;
		}
	}

	if (!onlist) {
		gaim_debug(GAIM_DEBUG_MISC, "yahoo",
			"Uhoh, %s isn't on the list (or not in this group), adding him to group %s.\n", name, group);
		if (!(g = gaim_find_group(group))) {
			g = gaim_group_new(group);
			gaim_blist_add_group(g, NULL);
		}
		b = gaim_buddy_new(account, name, NULL);
		gaim_blist_add_buddy(b, NULL, g, NULL);
	}

	if (list) {
		if (!oname)
			oname = g_strdup(gaim_normalize(account, name));
		g_hash_table_insert(ht, oname, list);
	} else if (oname)
		g_free(oname);
}

static void yahoo_do_group_cleanup(gpointer key, gpointer value, gpointer user_data)
{
	char *name = key;
	GSList *list = value, *i;
	GaimBuddy *b;
	GaimGroup *g;

	for (i = list; i; i = i->next) {
		b = i->data;
		g = gaim_find_buddys_group(b);
		gaim_debug(GAIM_DEBUG_MISC, "yahoo", "Deleting Buddy %s from group %s.\n", name, g->name);
		gaim_blist_remove_buddy(b);
	}
}

static char *_getcookie(char *rawcookie)
{
	char *cookie = NULL;
	char *tmpcookie;
	char *cookieend;

	if (strlen(rawcookie) < 2)
		return NULL;
	tmpcookie = g_strdup(rawcookie+2);
	cookieend = strchr(tmpcookie, ';');

	if (cookieend)
		*cookieend = '\0';

	cookie = g_strdup(tmpcookie);
	g_free(tmpcookie);

	return cookie;
}

static void yahoo_process_cookie(struct yahoo_data *yd, char *c)
{
	if (c[0] == 'Y') {
		if (yd->cookie_y)
			g_free(yd->cookie_y);
   		yd->cookie_y = _getcookie(c);
	} else if (c[0] == 'T') {
		if (yd->cookie_t)
			g_free(yd->cookie_t);
		yd->cookie_t = _getcookie(c);
	}
}

static void yahoo_process_list(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	gboolean export = FALSE;
	gboolean got_serv_list = FALSE;
	GaimBuddy *b;
	GaimGroup *g;
	YahooFriend *f = NULL;
	GaimAccount *account = gaim_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	GHashTable *ht;

	char **lines;
	char **split;
	char **buddies;
	char **tmp, **bud, *norm_bud;
	char *grp = NULL;

	if (pkt->id)
		yd->session_id = pkt->id;

	while (l) {
		struct yahoo_pair *pair = l->data;
		l = l->next;

		switch (pair->key) {
		case 87:
			if (!yd->tmp_serv_blist)
				yd->tmp_serv_blist = g_string_new(pair->value);
			else
				g_string_append(yd->tmp_serv_blist, pair->value);
			break;
		case 88:
			if (!yd->tmp_serv_ilist)
				yd->tmp_serv_ilist = g_string_new(pair->value);
			else
				g_string_append(yd->tmp_serv_ilist, pair->value);
			break;
		case 59: /* cookies, yum */
			yahoo_process_cookie(yd, pair->value);
			break;
		}
	}

	if (pkt->status != 0)
		return;

	if (yd->tmp_serv_blist) {
		ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_slist_free);

		lines = g_strsplit(yd->tmp_serv_blist->str, "\n", -1);
		for (tmp = lines; *tmp; tmp++) {
			split = g_strsplit(*tmp, ":", 2);
			if (!split)
				continue;
			if (!split[0] || !split[1]) {
				g_strfreev(split);
				continue;
			}
			grp = yahoo_string_decode(gc, split[0], FALSE);
			buddies = g_strsplit(split[1], ",", -1);
			for (bud = buddies; bud && *bud; bud++) {
				norm_bud = g_strdup(gaim_normalize(account, *bud));
				f = yahoo_friend_find_or_new(gc, norm_bud);

				if (!(b = gaim_find_buddy(account, norm_bud))) {
					if (!(g = gaim_find_group(grp))) {
						g = gaim_group_new(grp);
						gaim_blist_add_group(g, NULL);
					}
					b = gaim_buddy_new(account, norm_bud, NULL);
					gaim_blist_add_buddy(b, NULL, g, NULL);
					export = TRUE;
				}

				yahoo_do_group_check(account, ht, norm_bud, grp);
				g_free(norm_bud);
			}
			g_strfreev(buddies);
			g_strfreev(split);
			g_free(grp);
		}
		g_strfreev(lines);

		g_string_free(yd->tmp_serv_blist, TRUE);
		yd->tmp_serv_blist = NULL;
		g_hash_table_foreach(ht, yahoo_do_group_cleanup, NULL);
		g_hash_table_destroy(ht);
	}


	if (yd->tmp_serv_ilist) {
		buddies = g_strsplit(yd->tmp_serv_ilist->str, ",", -1);
		for (bud = buddies; bud && *bud; bud++) {
			/* The server is already ignoring the user */
			got_serv_list = TRUE;
			gaim_privacy_deny_add(gc->account, *bud, 1);
		}
		g_strfreev(buddies);

		g_string_free(yd->tmp_serv_ilist, TRUE);
		yd->tmp_serv_ilist = NULL;
	}

	if (got_serv_list &&
		((gc->account->perm_deny != GAIM_PRIVACY_ALLOW_BUDDYLIST) &&
		(gc->account->perm_deny != GAIM_PRIVACY_DENY_ALL) &&
		(gc->account->perm_deny != GAIM_PRIVACY_ALLOW_USERS)))
	{
		gc->account->perm_deny = GAIM_PRIVACY_DENY_USERS;
		gaim_debug_info("yahoo", "Privacy defaulting to GAIM_PRIVACY_DENY_USERS.\n", gc->account->username);
	}

}

static void yahoo_process_notify(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *msg = NULL;
	char *from = NULL;
	char *stat = NULL;
	char *game = NULL;
	YahooFriend *f = NULL;
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 4)
			from = pair->value;
		if (pair->key == 49)
			msg = pair->value;
		if (pair->key == 13)
			stat = pair->value;
		if (pair->key == 14)
			game = pair->value;
		l = l->next;
	}

	if (!from || !msg)
		return;

	if (!g_ascii_strncasecmp(msg, "TYPING", strlen("TYPING"))
		&& (yahoo_privacy_check(gc, from)))  {
		if (*stat == '1')
			serv_got_typing(gc, from, 0, GAIM_TYPING);
		else
			serv_got_typing_stopped(gc, from);
	} else if (!g_ascii_strncasecmp(msg, "GAME", strlen("GAME"))) {
		GaimBuddy *bud = gaim_find_buddy(gc->account, from);

		if (!bud) {
			gaim_debug(GAIM_DEBUG_WARNING, "yahoo",
					   "%s is playing a game, and doesn't want "
					   "you to know.\n", from);
		}

		f = yahoo_friend_find(gc, from);
		if (!f)
			return; /* if they're not on the list, don't bother */

		yahoo_friend_set_game(f, NULL);

		if (*stat == '1') {
			yahoo_friend_set_game(f, game);
			if (bud)
				yahoo_update_status(gc, from, f);
		}
	}
}


struct _yahoo_im {
	char *from;
	int time;
	int utf8;
	int buddy_icon;
	char *msg;
};

static void yahoo_process_message(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	GSList *list = NULL;
	struct _yahoo_im *im = NULL;

	if (pkt->status <= 1 || pkt->status == 5) {
		while (l) {
			struct yahoo_pair *pair = l->data;
			if (pair->key == 4) {
				im = g_new0(struct _yahoo_im, 1);
				list = g_slist_append(list, im);
				im->from = pair->value;
				im->time = time(NULL);
			}
			if (pair->key == 97)
				if (im)
					im->utf8 = strtol(pair->value, NULL, 10);
			if (pair->key == 15)
				if (im)
					im->time = strtol(pair->value, NULL, 10);
			if (pair->key == 206)
				if (im)
					im->buddy_icon = strtol(pair->value, NULL, 10);
			if (pair->key == 14) {
				if (im)
					im->msg = pair->value;
			}
			l = l->next;
		}
	} else if (pkt->status == 2) {
		gaim_notify_error(gc, NULL,
						  _("Your Yahoo! message did not get sent."), NULL);
	}

	for (l = list; l; l = l->next) {
		YahooFriend *f;
		char *m, *m2;
		im = l->data;

		if (!im->from || !im->msg) {
			g_free(im);
			continue;
		}

		if (!yahoo_privacy_check(gc, im->from)) {
			gaim_debug_info("yahoo", "Message from %s dropped.\n", im->from);
			return;
		}

		m = yahoo_string_decode(gc, im->msg, im->utf8);
		gaim_str_strip_cr(m);

		if (!strcmp(m, "<ding>")) {
			GaimConversation *c = gaim_conversation_new(GAIM_CONV_IM,
			                                            gaim_connection_get_account(gc), im->from);
			gaim_conv_im_write(GAIM_CONV_IM(c), "", _("Buzz!!"), GAIM_MESSAGE_NICK|GAIM_MESSAGE_RECV,
			                   im->time);
			g_free(m);
			g_free(im);
			continue;
		}

		m2 = yahoo_codes_to_html(m);
		g_free(m);
		serv_got_im(gc, im->from, m2, 0, im->time);
		g_free(m2);

		if ((f = yahoo_friend_find(gc, im->from)) && im->buddy_icon == 2) {
			if (yahoo_friend_get_buddy_icon_need_request(f)) {
				yahoo_send_picture_request(gc, im->from);
				yahoo_friend_set_buddy_icon_need_request(f, FALSE);
			}
		}

		g_free(im);
	}
	g_slist_free(list);
}

static void yahoo_process_sysmessage(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *prim, *me = NULL, *msg = NULL, *escmsg = NULL;

	while (l) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 5)
			me = pair->value;
		if (pair->key == 14)
			msg = pair->value;

		l = l->next;
	}

	if (!msg || !g_utf8_validate(msg, -1, NULL))
		return;

	escmsg = gaim_escape_html(msg);

	prim = g_strdup_printf(_("Yahoo! system message for %s:"),
	                       me?me:gaim_connection_get_display_name(gc));
	gaim_notify_info(NULL, NULL, prim, escmsg);
	g_free(prim);
	g_free(escmsg);
}

struct yahoo_add_request {
	GaimConnection *gc;
	char *id;
	char *who;
	char *msg;
};

static void
yahoo_buddy_add_authorize_cb(struct yahoo_add_request *add_req, const char *msg) {
	gaim_account_notify_added(add_req->gc->account, add_req->id,
			add_req->who, NULL, add_req->msg);

	g_free(add_req->id);
	g_free(add_req->who);
	g_free(add_req->msg);
	g_free(add_req);
}

static void
yahoo_buddy_add_deny_cb(struct yahoo_add_request *add_req, const char *msg) {
	struct yahoo_packet *pkt;
	char *encoded_msg = NULL;
	struct yahoo_data *yd = add_req->gc->proto_data;

	if (msg)
		encoded_msg = yahoo_string_encode(add_req->gc, msg, NULL);

	pkt = yahoo_packet_new(YAHOO_SERVICE_REJECTCONTACT,
			YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, gaim_normalize(add_req->gc->account,
				gaim_account_get_username(
					gaim_connection_get_account(
						add_req->gc))));
	yahoo_packet_hash(pkt, 7, add_req->who);
	yahoo_packet_hash(pkt, 14, encoded_msg ? encoded_msg : "");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	g_free(encoded_msg);

	g_free(add_req->id);
	g_free(add_req->who);
	g_free(add_req->msg);
	g_free(add_req);
}

static void yahoo_buddy_added_us(GaimConnection *gc, struct yahoo_packet *pkt) {
	struct yahoo_add_request *add_req;
	char *msg = NULL;
	GSList *l = pkt->hash;

	add_req = g_new0(struct yahoo_add_request, 1);
	add_req->gc = gc;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 1:
			add_req->id = g_strdup(pair->value);
			break;
		case 3:
			add_req->who = g_strdup(pair->value);
			break;
		case 15: /* time, for when they add us and we're offline */
			break;
		case 14:
			msg = pair->value;
			break;
		}
		l = l->next;
	}

	if (add_req->id) {
		char *prompt_msg;
		if (msg)
			add_req->msg = yahoo_string_decode(gc, msg, FALSE);

		/* TODO: this is almost exactly the same as what MSN does,
		 * this should probably be moved to the core.
		 * */
		prompt_msg = g_strdup_printf(_("The user %s wants to add %s to "
					"his or her buddy list%s%s."),
				add_req->who, add_req->id,
				add_req->msg ? ": " : "",
				add_req->msg ? add_req->msg : "");
		gaim_request_input(gc, NULL, prompt_msg,
				_("Message (optional) :"),
				NULL, TRUE, FALSE, NULL,
				_("Authorize"), G_CALLBACK(
					yahoo_buddy_add_authorize_cb),
				_("Deny"), G_CALLBACK(
					yahoo_buddy_add_deny_cb),
				add_req);
		g_free(prompt_msg);
	} else {
		g_free(add_req->id);
		g_free(add_req->who);
		/*g_free(add_req->msg);*/
		g_free(add_req);
	}
}

static void yahoo_buddy_denied_our_add(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *who = NULL;
	char *msg = NULL;
	GSList *l = pkt->hash;
	GString *buf = NULL;
	struct yahoo_data *yd = gc->proto_data;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 3:
			who = pair->value;
			break;
		case 14:
			msg = pair->value;
			break;
		}
		l = l->next;
	}

	if (who) {
		char *msg2;
		buf = g_string_sized_new(0);
		if (!msg) {
			g_string_printf(buf, _("%s has (retroactively) denied your request to add them to your list."), who);
		} else {
			msg2 = yahoo_string_decode(gc, msg, FALSE);
			g_string_printf(buf, _("%s has (retroactively) denied your request to add them to your list for the following reason: %s."), who, msg2);
			g_free(msg2);
		}
		gaim_notify_info(gc, NULL, _("Add buddy rejected"), buf->str);
		g_string_free(buf, TRUE);
		g_hash_table_remove(yd->friends, who);
		serv_got_update(gc, who, FALSE, 0, 0, 0, 0);
	}
}

static void yahoo_process_contact(GaimConnection *gc, struct yahoo_packet *pkt)
{


	switch (pkt->status) {
	case 1:
		yahoo_process_status(gc, pkt);
		return;
	case 3:
		yahoo_buddy_added_us(gc, pkt);
		break;
	case 7:
		yahoo_buddy_denied_our_add(gc, pkt);
		break;
	default:
		break;
	}
}

#define OUT_CHARSET "utf-8"

static char *yahoo_decode(const char *text)
{
	char *converted = NULL;
	char *n, *new;
	const char *end, *p;
	int i, k;

	n = new = g_malloc(strlen (text) + 1);
	end = text + strlen(text);

	for (p = text; p < end; p++, n++) {
		if (*p == '\\') {
			if (p[1] >= '0' && p[1] <= '7') {
				p += 1;
				for (i = 0, k = 0; k < 3; k += 1) {
					char c = p[k];
					if (c < '0' || c > '7') break;
					i *= 8;
					i += c - '0';
				}
				*n = i;
				p += k - 1;
			} else { /* bug 959248 */
				/* If we see a \ not followed by an octal number,
				 * it means that it is actually a \\ with one \
				 * already eaten by some unknown function.
				 * This is arguably broken.
				 *
				 * I think wing is wrong here, there is no function
				 * called that I see that could have done it. I guess
				 * it is just really sending single \'s. That's yahoo
				 * for you.
				 */
				*n = *p;
			}
		}
		else
			*n = *p;
	}

	*n = '\0';

	if (strstr(text, "\033$B"))
		converted = g_convert(new, n - new, OUT_CHARSET, "iso-2022-jp", NULL, NULL, NULL);
	if (!converted)
		converted = g_convert(new, n - new, OUT_CHARSET, "iso-8859-1", NULL, NULL, NULL);
	g_free(new);

	return converted;
}

static void yahoo_process_mail(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	char *who = NULL;
	char *email = NULL;
	char *subj = NULL;
	char *yahoo_mail_url = (yd->jp? YAHOOJP_MAIL_URL: YAHOO_MAIL_URL);
	int count = 0;
	GSList *l = pkt->hash;

	if (!gaim_account_get_check_mail(account))
		return;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 9)
			count = strtol(pair->value, NULL, 10);
		else if (pair->key == 43)
			who = pair->value;
		else if (pair->key == 42)
			email = pair->value;
		else if (pair->key == 18)
			subj = pair->value;
		l = l->next;
	}

	if (who && subj && email && *email) {
		char *dec_who = yahoo_decode(who);
		char *dec_subj = yahoo_decode(subj);
		char *from = g_strdup_printf("%s (%s)", dec_who, email);

		gaim_notify_email(gc, dec_subj, from, gaim_account_get_username(account),
						  yahoo_mail_url, NULL, NULL);

		g_free(dec_who);
		g_free(dec_subj);
		g_free(from);
	} else if (count > 0) {
		const char *to = gaim_account_get_username(account);
		const char *url = yahoo_mail_url;

		gaim_notify_emails(gc, count, FALSE, NULL, NULL, &to, &url,
						   NULL, NULL);
	}
}
/* This is the y64 alphabet... it's like base64, but has a . and a _ */
char base64digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";

/* This is taken from Sylpheed by Hiroyuki Yamamoto.  We have our own tobase64 function
 * in util.c, but it has a bug I don't feel like finding right now ;) */
void to_y64(unsigned char *out, const unsigned char *in, int inlen)
     /* raw bytes in quasi-big-endian order to base 64 string (NUL-terminated) */
{
	for (; inlen >= 3; inlen -= 3)
		{
			*out++ = base64digits[in[0] >> 2];
			*out++ = base64digits[((in[0] << 4) & 0x30) | (in[1] >> 4)];
			*out++ = base64digits[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
			*out++ = base64digits[in[2] & 0x3f];
			in += 3;
		}
	if (inlen > 0)
		{
			unsigned char fragment;

			*out++ = base64digits[in[0] >> 2];
			fragment = (in[0] << 4) & 0x30;
			if (inlen > 1)
				fragment |= in[1] >> 4;
			*out++ = base64digits[fragment];
			*out++ = (inlen < 2) ? '-' : base64digits[(in[1] << 2) & 0x3c];
			*out++ = '-';
		}
	*out = '\0';
}

static void yahoo_process_auth_old(GaimConnection *gc, const char *seed)
{
	struct yahoo_packet *pack;
	GaimAccount *account = gaim_connection_get_account(gc);
	const char *name = gaim_normalize(account, gaim_account_get_username(account));
	const char *pass = gaim_account_get_password(account);
	struct yahoo_data *yd = gc->proto_data;

	/* So, Yahoo has stopped supporting its older clients in India, and undoubtedly
	 * will soon do so in the rest of the world.
	 *
	 * The new clients use this authentication method.  I warn you in advance, it's
	 * bizarre, convoluted, inordinately complicated.  It's also no more secure than
	 * crypt() was.  The only purpose this scheme could serve is to prevent third
	 * part clients from connecting to their servers.
	 *
	 * Sorry, Yahoo.
	 */

	md5_byte_t result[16];
	md5_state_t ctx;

	char *crypt_result;
	char password_hash[25];
	char crypt_hash[25];
	char *hash_string_p = g_malloc(50 + strlen(name));
	char *hash_string_c = g_malloc(50 + strlen(name));

	char checksum;

	int sv;

	char result6[25];
	char result96[25];

	sv = seed[15];
	sv = sv % 8;

	md5_init(&ctx);
	md5_append(&ctx, pass, strlen(pass));
	md5_finish(&ctx, result);
	to_y64(password_hash, result, 16);

	md5_init(&ctx);
	crypt_result = yahoo_crypt(pass, "$1$_2S43d5f$");
	md5_append(&ctx, crypt_result, strlen(crypt_result));
	md5_finish(&ctx, result);
	to_y64(crypt_hash, result, 16);

	switch (sv) {
	case 1:
	case 6:
		checksum = seed[seed[9] % 16];
		g_snprintf(hash_string_p, strlen(name) + 50,
			   "%c%s%s%s", checksum, name, seed, password_hash);
		g_snprintf(hash_string_c, strlen(name) + 50,
			   "%c%s%s%s", checksum, name, seed, crypt_hash);
		break;
	case 2:
	case 7:
		checksum = seed[seed[15] % 16];
		g_snprintf(hash_string_p, strlen(name) + 50,
			   "%c%s%s%s", checksum, seed, password_hash, name);
		g_snprintf(hash_string_c, strlen(name) + 50,
			   "%c%s%s%s", checksum, seed, crypt_hash, name);
		break;
	case 3:
		checksum = seed[seed[1] % 16];
		g_snprintf(hash_string_p, strlen(name) + 50,
			   "%c%s%s%s", checksum, name, password_hash, seed);
		g_snprintf(hash_string_c, strlen(name) + 50,
			   "%c%s%s%s", checksum, name, crypt_hash, seed);
		break;
	case 4:
		checksum = seed[seed[3] % 16];
		g_snprintf(hash_string_p, strlen(name) + 50,
			   "%c%s%s%s", checksum, password_hash, seed, name);
		g_snprintf(hash_string_c, strlen(name) + 50,
			   "%c%s%s%s", checksum, crypt_hash, seed, name);
		break;
	case 0:
	case 5:
		checksum = seed[seed[7] % 16];
			g_snprintf(hash_string_p, strlen(name) + 50,
                                   "%c%s%s%s", checksum, password_hash, name, seed);
                        g_snprintf(hash_string_c, strlen(name) + 50,
				   "%c%s%s%s", checksum, crypt_hash, name, seed);
			break;
	}

	md5_init(&ctx);
	md5_append(&ctx, hash_string_p, strlen(hash_string_p));
	md5_finish(&ctx, result);
	to_y64(result6, result, 16);

	md5_init(&ctx);
	md5_append(&ctx, hash_string_c, strlen(hash_string_c));
	md5_finish(&ctx, result);
	to_y64(result96, result, 16);

	pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP,	YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pack, 0, name);
	yahoo_packet_hash(pack, 6, result6);
	yahoo_packet_hash(pack, 96, result96);
	yahoo_packet_hash(pack, 1, name);

	yahoo_send_packet(yd, pack);

	g_free(hash_string_p);
	g_free(hash_string_c);

	yahoo_packet_free(pack);

}

/* I'm dishing out some uber-mad props to Cerulean Studios for cracking this
 * and sending the fix!  Thanks guys. */

static void yahoo_process_auth_new(GaimConnection *gc, const char *seed)
{
	struct yahoo_packet *pack = NULL;
	GaimAccount *account = gaim_connection_get_account(gc);
	const char *name = gaim_normalize(account, gaim_account_get_username(account));
	const char *pass = gaim_account_get_password(account);
	struct yahoo_data *yd = gc->proto_data;

	md5_byte_t			result[16];
	md5_state_t			ctx;

	SHA_CTX				ctx1;
	SHA_CTX				ctx2;

	char				*alphabet1			= "FBZDWAGHrJTLMNOPpRSKUVEXYChImkwQ";
	char				*alphabet2			= "F0E1D2C3B4A59687abcdefghijklmnop";

	char				*challenge_lookup	= "qzec2tb3um1olpar8whx4dfgijknsvy5";
	char				*operand_lookup		= "+|&%/*^-";
	char				*delimit_lookup		= ",;";

	char				*password_hash		= (char *)g_malloc(25);
	char				*crypt_hash		= (char *)g_malloc(25);
	char				*crypt_result		= NULL;

	char				pass_hash_xor1[64];
	char				pass_hash_xor2[64];
	char				crypt_hash_xor1[64];
	char				crypt_hash_xor2[64];
	char				resp_6[100];
	char				resp_96[100];

	unsigned char		digest1[20];
	unsigned char		digest2[20];
	unsigned char		comparison_src[20]; 
	unsigned char		magic_key_char[4];
	const unsigned char		*magic_ptr;

	unsigned int		magic[64];
	unsigned int		magic_work = 0;
	unsigned int		magic_4 = 0;

	int					x;
	int					y;
	int					cnt = 0;
	int					magic_cnt = 0;
	int					magic_len;

	memset(password_hash, 0, 25);
	memset(crypt_hash, 0, 25);
	memset(&pass_hash_xor1, 0, 64);
	memset(&pass_hash_xor2, 0, 64);
	memset(&crypt_hash_xor1, 0, 64);
	memset(&crypt_hash_xor2, 0, 64);
	memset(&digest1, 0, 20);
	memset(&digest2, 0, 20);
	memset(&magic, 0, 64);
	memset(&resp_6, 0, 100);
	memset(&resp_96, 0, 100);
	memset(&magic_key_char, 0, 4);
	memset(&comparison_src, 0, 20);

	/* 
	 * Magic: Phase 1.  Generate what seems to be a 30 byte value (could change if base64
	 * ends up differently?  I don't remember and I'm tired, so use a 64 byte buffer.
	 */

	magic_ptr = seed;

	while (*magic_ptr != (int)NULL) {
		char   *loc;
		
		/* Ignore parentheses.
		 */
		
		if (*magic_ptr == '(' || *magic_ptr == ')') {
			magic_ptr++;
			continue;
		}
		
		/* Characters and digits verify against the challenge lookup.
		 */
		
		if (isalpha(*magic_ptr) || isdigit(*magic_ptr)) {
			loc = strchr(challenge_lookup, *magic_ptr);
			if (!loc) {
			  /* SME XXX Error - disconnect here */
			}
			
			/* Get offset into lookup table and shl 3.
			 */
			
			magic_work = loc - challenge_lookup;
			magic_work <<= 3;
			
			magic_ptr++;
			continue;
		} else {
			unsigned int	local_store;
			
			loc = strchr(operand_lookup, *magic_ptr);
			if (!loc) {
				/* SME XXX Disconnect */
			}
			
			local_store = loc - operand_lookup;
			
			/* Oops; how did this happen?
			 */
			
			if (magic_cnt >= 64) 
				break;
			
			magic[magic_cnt++] = magic_work | local_store;
			magic_ptr++;
			continue;
		}
			}
	
	magic_len = magic_cnt;
	magic_cnt = 0;
	
	/* Magic: Phase 2.  Take generated magic value and sprinkle fairy dust on the values.
	 */

	for (magic_cnt = magic_len-2; magic_cnt >= 0; magic_cnt--) {
		unsigned char	byte1;
		unsigned char	byte2;
		
		/* Bad.  Abort.
		 */
		
		if ((magic_cnt + 1 > magic_len) || (magic_cnt > magic_len))
			break;
		
		byte1 = magic[magic_cnt];
		byte2 = magic[magic_cnt+1];
		
		byte1 *= 0xcd;
		byte1 ^= byte2;
		
		magic[magic_cnt+1] = byte1;
			}
	
	/* 
	 * Magic: Phase 3.  This computes 20 bytes.  The first 4 bytes are used as our magic
	 * key (and may be changed later); the next 16 bytes are an MD5 sum of the magic key
	 * plus 3 bytes.  The 3 bytes are found by looping, and they represent the offsets
	 * into particular functions we'll later call to potentially alter the magic key.
	 *
	 * %-)
	 */
	
	magic_cnt = 1;
	x = 0;
	
	do {
		unsigned int	bl = 0; 
		unsigned int	cl = magic[magic_cnt++];
		
		if (magic_cnt >= magic_len)
			break;
		
		if (cl > 0x7F) {
			if (cl < 0xe0) 
				bl = cl = (cl & 0x1f) << 6; 
			else {
				bl = magic[magic_cnt++]; 
				cl = (cl & 0x0f) << 6; 
				bl = ((bl & 0x3f) + cl) << 6; 
			} 

			cl = magic[magic_cnt++]; 
			bl = (cl & 0x3f) + bl; 
		} else
			bl = cl; 
		
		comparison_src[x++] = (bl & 0xff00) >> 8; 
		comparison_src[x++] = bl & 0xff; 
	} while (x < 20);
	
	/* First four bytes are magic key.
	 */
	
	memcpy(&magic_key_char[0], comparison_src, 4);
	magic_4 = magic_key_char[0] | (magic_key_char[1]<<8) | (magic_key_char[2]<<16) | (magic_key_char[3]<<24);
	
	/* 
	 * Magic: Phase 4.  Determine what function to use later by getting outside/inside
	 * loop values until we match our previous buffer.
	 */
	
	for (x = 0; x < 65535; x++) {
		int			leave = 0;

		for (y = 0; y < 5; y++) {
			md5_byte_t		result[16];
			md5_state_t		ctx;
			
			unsigned char	test[3];
			
			memset(&result, 0, 16);
			memset(&test, 0, 3);
			
			/* Calculate buffer.
			 */

			test[0] = x;
			test[1] = x >> 8;
			test[2] = y;
			
			md5_init(&ctx);
			md5_append(&ctx, magic_key_char, 4);
			md5_append(&ctx, test, 3);
			md5_finish(&ctx, result);
			
			if (!memcmp(result, comparison_src+4, 16)) {
				leave = 1;
				break;
			}
		}
		
		if (leave == 1)
			break;
	}
	
	/* If y != 0, we need some help.
	 */
	
	if (y != 0) {
		unsigned int	updated_key;
		
		/* Update magic stuff.   Call it twice because Yahoo's encryption is super bad ass.
		 */
		
		updated_key = yahoo_auth_finalCountdown(magic_4, 0x60, y, x);
		updated_key = yahoo_auth_finalCountdown(updated_key, 0x60, y, x);
		
		magic_key_char[0] = updated_key & 0xff;
		magic_key_char[1] = (updated_key >> 8) & 0xff;
		magic_key_char[2] = (updated_key >> 16) & 0xff;
		magic_key_char[3] = (updated_key >> 24) & 0xff;
	} 
	
/* Get password and crypt hashes as per usual.
	 */
	
	md5_init(&ctx);
	md5_append(&ctx, pass, strlen(pass));
	md5_finish(&ctx, result);
	to_y64(password_hash, result, 16);
	
	md5_init(&ctx);
	crypt_result = yahoo_crypt(pass, "$1$_2S43d5f$");  
	md5_append(&ctx, crypt_result, strlen(crypt_result));
	md5_finish(&ctx, result);
	to_y64(crypt_hash, result, 16);

	/* Our first authentication response is based off of the password hash.
	 */
	
	for (x = 0; x < (int)strlen(password_hash); x++) 
		pass_hash_xor1[cnt++] = password_hash[x] ^ 0x36;
	
	if (cnt < 64) 
		memset(&(pass_hash_xor1[cnt]), 0x36, 64-cnt);

	cnt = 0;
	
	for (x = 0; x < (int)strlen(password_hash); x++) 
		pass_hash_xor2[cnt++] = password_hash[x] ^ 0x5c;
	
	if (cnt < 64) 
		memset(&(pass_hash_xor2[cnt]), 0x5c, 64-cnt);
	
	shaInit(&ctx1);
	shaInit(&ctx2);
	
	/* 
	 * The first context gets the password hash XORed with 0x36 plus a magic value
	 * which we previously extrapolated from our challenge.
	 */
	
	shaUpdate(&ctx1, pass_hash_xor1, 64);
	if (y >= 3)
		ctx1.sizeLo = 0x1ff;
	shaUpdate(&ctx1, magic_key_char, 4);
	shaFinal(&ctx1, digest1);
	
	/* 
	 * The second context gets the password hash XORed with 0x5c plus the SHA-1 digest
	 * of the first context.
	 */
	
	shaUpdate(&ctx2, pass_hash_xor2, 64);
	shaUpdate(&ctx2, digest1, 20);
	shaFinal(&ctx2, digest2);
	
	/* 
	 * Now that we have digest2, use it to fetch characters from an alphabet to construct
	 * our first authentication response.
	 */

	for (x = 0; x < 20; x += 2) {
		unsigned int	val = 0;
		unsigned int	lookup = 0;
		
		char			byte[6];
				
		memset(&byte, 0, 6);

		/* First two bytes of digest stuffed together.
		 */

		val = digest2[x];
		val <<= 8;
		val += digest2[x+1];
		
		lookup = (val >> 0x0b);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet1))
			break;
		sprintf(byte, "%c", alphabet1[lookup]);
		strcat(resp_6, byte);
		strcat(resp_6, "=");

		lookup = (val >> 0x06);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_6, byte);
		
		lookup = (val >> 0x01);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_6, byte);

		lookup = (val & 0x01);
		if (lookup >= strlen(delimit_lookup))
			break;
		sprintf(byte, "%c", delimit_lookup[lookup]);
		strcat(resp_6, byte);
	}
	
	/* Our second authentication response is based off of the crypto hash.
	 */
	
	cnt = 0;
	memset(&digest1, 0, 20);
	memset(&digest2, 0, 20);
	
	for (x = 0; x < (int)strlen(crypt_hash); x++) 
		crypt_hash_xor1[cnt++] = crypt_hash[x] ^ 0x36;
	
	if (cnt < 64) 
		memset(&(crypt_hash_xor1[cnt]), 0x36, 64-cnt);

	cnt = 0;
	
	for (x = 0; x < (int)strlen(crypt_hash); x++) 
		crypt_hash_xor2[cnt++] = crypt_hash[x] ^ 0x5c;
	
	if (cnt < 64) 
		memset(&(crypt_hash_xor2[cnt]), 0x5c, 64-cnt);
	
	shaInit(&ctx1);
	shaInit(&ctx2);
	
	/* 
	 * The first context gets the password hash XORed with 0x36 plus a magic value
	 * which we previously extrapolated from our challenge.
	 */
	
	shaUpdate(&ctx1, crypt_hash_xor1, 64);
	if (y >= 3)
		ctx1.sizeLo = 0x1ff;
	shaUpdate(&ctx1, magic_key_char, 4);
	shaFinal(&ctx1, digest1);
	
	/* 
	 * The second context gets the password hash XORed with 0x5c plus the SHA-1 digest
	 * of the first context.
	 */
	
	shaUpdate(&ctx2, crypt_hash_xor2, 64);
	shaUpdate(&ctx2, digest1, 20);
	shaFinal(&ctx2, digest2);
	
	/* 
	 * Now that we have digest2, use it to fetch characters from an alphabet to construct
	 * our first authentication response.
	 */
	
	for (x = 0; x < 20; x += 2) {
		unsigned int	val = 0;
		unsigned int	lookup = 0;
		
		char			byte[6];
		
		memset(&byte, 0, 6);
		
		/* First two bytes of digest stuffed together.
		 */
		
		val = digest2[x];
		val <<= 8;
		val += digest2[x+1];

		lookup = (val >> 0x0b);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet1))
			break;
		sprintf(byte, "%c", alphabet1[lookup]);
		strcat(resp_96, byte);
		strcat(resp_96, "=");
		
		lookup = (val >> 0x06);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_96, byte);
		
		lookup = (val >> 0x01);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_96, byte);
		
		lookup = (val & 0x01);
		if (lookup >= strlen(delimit_lookup))
			break;
		sprintf(byte, "%c", delimit_lookup[lookup]);
		strcat(resp_96, byte);
	}
	
	pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP,	YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pack, 0, name);
	yahoo_packet_hash(pack, 6, resp_6);
	yahoo_packet_hash(pack, 96, resp_96);
	yahoo_packet_hash(pack, 1, name);
	yahoo_packet_hash(pack, 135, "6,0,0,1710");
	if (yd->picture_checksum) {
		char *cksum = g_strdup_printf("%d", yd->picture_checksum);
		yahoo_packet_hash(pack, 192, cksum);
		g_free(cksum);
	}
	yahoo_send_packet(yd, pack);
	yahoo_packet_free(pack);

	g_free(password_hash);
	g_free(crypt_hash);
}

static void yahoo_process_auth(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *seed = NULL;
	char *sn   = NULL;
	GSList *l = pkt->hash;
	int m = 0;
	gchar *buf;


	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 94)
			seed = pair->value;
		if (pair->key == 1)
			sn = pair->value;
		if (pair->key == 13)
			m = atoi(pair->value);
		l = l->next;
	}

	if (seed) {
		switch (m) {
		case 0:
			yahoo_process_auth_old(gc, seed);
			break;
		case 1:
			yahoo_process_auth_new(gc, seed);
			break;
		default:
			buf = g_strdup_printf(_("The Yahoo server has requested the use of an unrecognized "
						"authentication method.  This version of Gaim will likely not be able "
						"to successfully sign on to Yahoo.  Check %s for updates."), GAIM_WEBSITE);
			gaim_notify_error(gc, "", _("Failed Yahoo! Authentication"),
					  buf);
			g_free(buf);
			yahoo_process_auth_new(gc, seed); /* Can't hurt to try it anyway. */
		}
	}
}

static void ignore_buddy(GaimBuddy *buddy) {
	GaimGroup *group;
	GaimConversation *conv;
	GaimAccount *account;
	gchar *name;

	if (!buddy)
		return;

	group = gaim_find_buddys_group(buddy);
	name = g_strdup(buddy->name);
	account = buddy->account;

	gaim_debug(GAIM_DEBUG_INFO, "blist",
		"Removing '%s' from buddy list.\n", buddy->name);
	serv_remove_buddy(account->gc, buddy, group);
	gaim_blist_remove_buddy(buddy);

	serv_add_deny(account->gc, name);

	conv = gaim_find_conversation_with_account(name, account);

	if (conv != NULL)
		gaim_conversation_update(conv, GAIM_CONV_UPDATE_REMOVE);

	g_free(name);
}

static void keep_buddy(GaimBuddy *b) {
	gaim_privacy_deny_remove(b->account, b->name, 1);
}

static void yahoo_process_ignore(GaimConnection *gc, struct yahoo_packet *pkt) {
	GaimBuddy *b;
	GSList *l;
	gchar *who = NULL;
	gchar *sn = NULL;
	gchar buf[BUF_LONG];
	gint ignore = 0;
	gint status = 0;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;
		switch (pair->key) {
		case 0:
			who = pair->value;
			break;
		case 1:
			sn = pair->value;
			break;
		case 13:
			ignore = strtol(pair->value, NULL, 10);
			break;
		case 66:
			status = strtol(pair->value, NULL, 10);
			break;
		default:
			break;
		}
	}

	switch (status) {
	case 12:
		b = gaim_find_buddy(gc->account, who);
		g_snprintf(buf, sizeof(buf), _("You have tried to ignore %s, but the "
					"user is on your buddy list.  Clicking \"Yes\" "
					"will remove and ignore the buddy."), who);
		gaim_request_yes_no(gc, NULL, _("Ignore buddy?"), buf, 0, b,
						G_CALLBACK(ignore_buddy),
						G_CALLBACK(keep_buddy));
		break;
	case 2:
	case 3:
	case 0:
	default:
		break;
	}
}

static void yahoo_process_authresp(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = gc->proto_data;
	GSList *l = pkt->hash;
	int err = 0;
	char *msg;
	char *url = NULL;
	char *fullmsg;

	while (l) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 66)
			err = strtol(pair->value, NULL, 10);
		if (pair->key == 20)
			url = pair->value;

		l = l->next;
	}

	switch (err) {
	case 3:
		msg = g_strdup(_("Invalid username."));
		break;
	case 13:
		if (!yd->wm) {
			yd->wm = TRUE;
			if (yd->fd >= 0)
				close(yd->fd);
			if (gc->inpa)
				gaim_input_remove(gc->inpa);
			gaim_url_fetch(WEBMESSENGER_URL, TRUE, "Gaim/" VERSION, FALSE,
			               yahoo_login_page_cb, gc);
			gaim_notify_warning(gc, NULL, _("Normal authentication failed!"),
			                    _("The normal authentication method has failed. "
			                      "This means either your password is incorrect, "
			                      "or Yahoo!'s authentication scheme has changed. "
			                      "Gaim will now attempt to log in using Web "
			                      "Messenger authentication, which will result "
			                      "in reduced functionality and features."));
			return;
		}
		msg = g_strdup(_("Incorrect password."));
		break;
	case 14:
		msg = g_strdup(_("Your account is locked, please log in to the Yahoo! website."));
		break;
	default:
		msg = g_strdup_printf(_("Unknown error number %d. Logging into the Yahoo! website may fix this."), err);
	}

	if (url)
		fullmsg = g_strdup_printf("%s\n%s", msg, url);
	else
		fullmsg = g_strdup(msg);

	gc->wants_to_die = TRUE;
	gaim_connection_error(gc, fullmsg);
	g_free(msg);
	g_free(fullmsg);
}

static void yahoo_process_addbuddy(GaimConnection *gc, struct yahoo_packet *pkt)
{
	int err = 0;
	char *who = NULL;
	char *group = NULL;
	char *decoded_group;
	char *buf;
	YahooFriend *f;
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 66:
			err = strtol(pair->value, NULL, 10);
			break;
		case 7:
			who = pair->value;
			break;
		case 65:
			group = pair->value;
			break;
		}

		l = l->next;
	}

	if (!who)
		return;
	if (!group)
		group = "";

	if (!err || (err == 2)) { /* 0 = ok, 2 = already on serv list */
		f = yahoo_friend_find_or_new(gc, who);
		yahoo_update_status(gc, who, f);
		return;
	}

	decoded_group = yahoo_string_decode(gc, group, FALSE);
	buf = g_strdup_printf(_("Could not add buddy %s to group %s to the server list on account %s."),
				who, decoded_group, gaim_connection_get_display_name(gc));
	if (!gaim_conv_present_error(who, gaim_connection_get_account(gc), buf))
		gaim_notify_error(gc, NULL, _("Could not add buddy to server list"), buf);
	g_free(buf);
	g_free(decoded_group);
}

static void yahoo_process_p2p(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *who = NULL;
	char *base64 = NULL;
	char *decoded;
	int len;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 5:
			/* our identity */
			break;
		case 4:
			who = pair->value;
			break;
		case 1:
			/* who again, the master identity this time? */
			break;
		case 12:
			base64 = pair->value;
			/* so, this is an ip address. in base64. decoded it's in ascii.
			   after strtol, it's in reversed byte order. Who thought this up?*/
			break;
		/*
			TODO: figure these out
			yahoo: Key: 61          Value: 0
			yahoo: Key: 2   Value:
			yahoo: Key: 13          Value: 0
			yahoo: Key: 49          Value: PEERTOPEER
			yahoo: Key: 140         Value: 1
			yahoo: Key: 11          Value: -1786225828
		*/

		}

		l = l->next;
	}

	if (base64) {
		guint32 ip;
		char *tmp2;
		YahooFriend *f;

		gaim_base64_decode(base64, &decoded, &len);
		if (len) {
			char *tmp = gaim_str_binary_to_ascii(decoded, len);
			gaim_debug_info("yahoo", "Got P2P service packet (from server): who = %s, ip = %s\n", who, tmp);
			g_free(tmp);
		}

		tmp2 = g_strndup(decoded, len); /* so its \0 terminated...*/
		ip = strtol(tmp2, NULL, 10);
		g_free(tmp2);
		g_free(decoded);
		tmp2 = g_strdup_printf("%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff,
		                       (ip >> 24) & 0xff);
		f = yahoo_friend_find(gc, who);
		if (f)
			yahoo_friend_set_ip(f, tmp2);
		g_free(tmp2);
	}
}

static void yahoo_process_audible(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *who = NULL, *msg = NULL;
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			who = pair->value;
			break;
		case 5:
			/* us */
			break;
		case 230:
			/* the audible, in foo.bar.baz format */
			break;
		case 231:
			/* the text of the audible */
			msg = pair->value;
			break;
		case 232:
			/* weird number (md5 hash?), like 8ebab9094156135f5dcbaccbeee662a5c5fd1420 */
			break;
		}

		l = l->next;
	}

	if (!who || !msg)
		return;
	if (!g_utf8_validate(msg, -1, NULL)) {
		gaim_debug_misc("yahoo", "Warning, nonutf8 audible, ignoring!\n");
		return;
	}
	if (!yahoo_privacy_check(gc, who)) {
		gaim_debug_misc("yahoo", "Audible message from %s for %s dropped!\n", gc->account->username, who);
		return;
	}
	serv_got_im(gc, who, msg, 0, time(NULL));
}

static void yahoo_packet_process(GaimConnection *gc, struct yahoo_packet *pkt)
{
	switch (pkt->service) {
	case YAHOO_SERVICE_LOGON:
	case YAHOO_SERVICE_LOGOFF:
	case YAHOO_SERVICE_ISAWAY:
	case YAHOO_SERVICE_ISBACK:
	case YAHOO_SERVICE_GAMELOGON:
	case YAHOO_SERVICE_GAMELOGOFF:
	case YAHOO_SERVICE_CHATLOGON:
	case YAHOO_SERVICE_CHATLOGOFF:
	case YAHOO_SERVICE_YAHOO6_STATUS_UPDATE:
		yahoo_process_status(gc, pkt);
		break;
	case YAHOO_SERVICE_NOTIFY:
		yahoo_process_notify(gc, pkt);
		break;
	case YAHOO_SERVICE_MESSAGE:
	case YAHOO_SERVICE_GAMEMSG:
	case YAHOO_SERVICE_CHATMSG:
		yahoo_process_message(gc, pkt);
		break;
	case YAHOO_SERVICE_SYSMESSAGE:
		yahoo_process_sysmessage(gc, pkt);
			break;
	case YAHOO_SERVICE_NEWMAIL:
		yahoo_process_mail(gc, pkt);
		break;
	case YAHOO_SERVICE_NEWCONTACT:
		yahoo_process_contact(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTHRESP:
		yahoo_process_authresp(gc, pkt);
		break;
	case YAHOO_SERVICE_LIST:
		yahoo_process_list(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTH:
		yahoo_process_auth(gc, pkt);
		break;
	case YAHOO_SERVICE_ADDBUDDY:
		yahoo_process_addbuddy(gc, pkt);
		break;
	case YAHOO_SERVICE_IGNORECONTACT:
		yahoo_process_ignore(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFINVITE:
	case YAHOO_SERVICE_CONFADDINVITE:
		yahoo_process_conference_invite(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFDECLINE:
		yahoo_process_conference_decline(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFLOGON:
		yahoo_process_conference_logon(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFLOGOFF:
		yahoo_process_conference_logoff(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFMSG:
		yahoo_process_conference_message(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATONLINE:
		yahoo_process_chat_online(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATLOGOUT:
		yahoo_process_chat_logout(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATGOTO:
		yahoo_process_chat_goto(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATJOIN:
		yahoo_process_chat_join(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATLEAVE: /* XXX is this right? */
	case YAHOO_SERVICE_CHATEXIT:
		yahoo_process_chat_exit(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATINVITE: /* XXX never seen this one, might not do it right */
	case YAHOO_SERVICE_CHATADDINVITE:
		yahoo_process_chat_addinvite(gc, pkt);
		break;
	case YAHOO_SERVICE_COMMENT:
		yahoo_process_chat_message(gc, pkt);
		break;
	case YAHOO_SERVICE_P2PFILEXFER:
	case YAHOO_SERVICE_FILETRANSFER:
		yahoo_process_filetransfer(gc, pkt);
		break;
	case YAHOO_SERVICE_PEEPTOPEER:
		yahoo_process_p2p(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE:
		yahoo_process_picture(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_UPDATE:
		yahoo_process_picture_update(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_CHECKSUM:
		yahoo_process_picture_checksum(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_UPLOAD:
		yahoo_process_picture_upload(gc, pkt);
		break;
	case YAHOO_SERVICE_AUDIBLE:
		yahoo_process_audible(gc, pkt);
	default:
		gaim_debug(GAIM_DEBUG_ERROR, "yahoo",
				   "Unhandled service 0x%02x\n", pkt->service);
		break;
	}
}

static void yahoo_pending(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct yahoo_data *yd = gc->proto_data;
	char buf[1024];
	int len;

	len = read(yd->fd, buf, sizeof(buf));

	if (len <= 0) {
		gaim_connection_error(gc, _("Unable to read"));
		return;
	}

	yd->rxqueue = g_realloc(yd->rxqueue, len + yd->rxlen);
	memcpy(yd->rxqueue + yd->rxlen, buf, len);
	yd->rxlen += len;

	while (1) {
		struct yahoo_packet *pkt;
		int pos = 0;
		int pktlen;

		if (yd->rxlen < YAHOO_PACKET_HDRLEN)
			return;

		if (strncmp(yd->rxqueue, "YMSG", MIN(4, yd->rxlen)) != 0) {
			/* HEY! This isn't even a YMSG packet. What
			 * are you trying to pull? */
			guchar *start;

			gaim_debug_warning("yahoo", "Error in YMSG stream, got something not a YMSG packet!\n");

			start = memchr(yd->rxqueue + 1, 'Y', yd->rxlen - 1);
			if (start) {
				g_memmove(yd->rxqueue, start, yd->rxlen - (start - yd->rxqueue));
				yd->rxlen -= start - yd->rxqueue;
				continue;
			} else {
				g_free(yd->rxqueue);
				yd->rxqueue = NULL;
				yd->rxlen = 0;
				return;
			}
		}

		pos += 4; /* YMSG */
		pos += 2;
		pos += 2;

		pktlen = yahoo_get16(yd->rxqueue + pos); pos += 2;
		gaim_debug(GAIM_DEBUG_MISC, "yahoo",
				   "%d bytes to read, rxlen is %d\n", pktlen, yd->rxlen);

		if (yd->rxlen < (YAHOO_PACKET_HDRLEN + pktlen))
			return;

		yahoo_packet_dump(yd->rxqueue, YAHOO_PACKET_HDRLEN + pktlen);

		pkt = yahoo_packet_new(0, 0, 0);

		pkt->service = yahoo_get16(yd->rxqueue + pos); pos += 2;
		pkt->status = yahoo_get32(yd->rxqueue + pos); pos += 4;
		gaim_debug(GAIM_DEBUG_MISC, "yahoo",
				   "Yahoo Service: 0x%02x Status: %d\n",
				   pkt->service, pkt->status);
		pkt->id = yahoo_get32(yd->rxqueue + pos); pos += 4;

		yahoo_packet_read(pkt, yd->rxqueue + pos, pktlen);

		yd->rxlen -= YAHOO_PACKET_HDRLEN + pktlen;
		if (yd->rxlen) {
			char *tmp = g_memdup(yd->rxqueue + YAHOO_PACKET_HDRLEN + pktlen, yd->rxlen);
			g_free(yd->rxqueue);
			yd->rxqueue = tmp;
		} else {
			g_free(yd->rxqueue);
			yd->rxqueue = NULL;
		}

		yahoo_packet_process(gc, pkt);

		yahoo_packet_free(pkt);
	}
}

static void yahoo_got_connected(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	if (source < 0) {
		gaim_connection_error(gc, _("Unable to connect."));
		return;
	}

	yd = gc->proto_data;
	yd->fd = source;

	pkt = yahoo_packet_new(YAHOO_SERVICE_AUTH, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, gaim_normalize(gc->account, gaim_account_get_username(gaim_connection_get_account(gc))));
	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	gc->inpa = gaim_input_add(yd->fd, GAIM_INPUT_READ, yahoo_pending, gc);
}

static void yahoo_got_web_connected(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	if (source < 0) {
		gaim_connection_error(gc, _("Unable to connect."));
		return;
	}

	yd = gc->proto_data;
	yd->fd = source;

	pkt = yahoo_packet_new(YAHOO_SERVICE_WEBLOGIN, YAHOO_STATUS_WEBLOGIN, 0);

	yahoo_packet_hash(pkt, 0, gaim_normalize(gc->account, gaim_account_get_username(gaim_connection_get_account(gc))));
	yahoo_packet_hash(pkt, 1, gaim_normalize(gc->account, gaim_account_get_username(gaim_connection_get_account(gc))));
	yahoo_packet_hash(pkt, 6, yd->auth);
	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
	g_free(yd->auth);
	gc->inpa = gaim_input_add(yd->fd, GAIM_INPUT_READ, yahoo_pending, gc);
}

static void yahoo_web_pending(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	GaimAccount *account = gaim_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	char buf[2048], *i = buf;
	int len;
	GString *s;

	len = read(source, buf, sizeof(buf)-1);
	if (len <= 0  || (strncmp(buf, "HTTP/1.0 302", strlen("HTTP/1.0 302")) &&
			  strncmp(buf, "HTTP/1.1 302", strlen("HTTP/1.1 302")))) {
		gaim_connection_error(gc, _("Unable to read"));
		return;
	}

	s = g_string_sized_new(len);
	buf[sizeof(buf)-1] = '\0';

	while ((i = strstr(i, "Set-Cookie: "))) {
		i += strlen("Set-Cookie: ");
		for (;*i != ';' && *i != '\0'; i++)
			g_string_append_c(s, *i);

		g_string_append(s, "; ");
	}

	yd->auth = g_string_free(s, FALSE);
	gaim_input_remove(gc->inpa);
	close(source);
	/* Now we have our cookies to login with.  I'll go get the milk. */
	if (gaim_proxy_connect(account, "wcs2.msg.dcn.yahoo.com",
			       gaim_account_get_int(account, "port", YAHOO_PAGER_PORT),
			       yahoo_got_web_connected, gc) != 0) {
		gaim_connection_error(gc, _("Connection problem"));
		return;
	}	
}

static void yahoo_got_cookies(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct yahoo_data *yd = gc->proto_data;
	if (source < 0) {
		gaim_connection_error(gc, _("Unable to connect."));
		return;
	}
	write(source, yd->auth, strlen(yd->auth));
	g_free(yd->auth);
	gc->inpa = gaim_input_add(source, GAIM_INPUT_READ, yahoo_web_pending, gc);
}

static void yahoo_login_page_hash_iter(const char *key, const char *val, GString *url)
{
	if (!strcmp(key, "passwd"))
		return;
	url = g_string_append_c(url, '&');
	url = g_string_append(url, key);
	url = g_string_append_c(url, '=');
	if (!strcmp(key, ".save") || !strcmp(key, ".js"))
		url = g_string_append_c(url, '1');
	else if (!strcmp(key, ".challenge"))
		url = g_string_append(url, val);
	else
		url = g_string_append(url, gaim_url_encode(val));
}

static GHashTable *yahoo_login_page_hash(const char *buf, size_t len)
{
	GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	const char *c = buf;
	char *d;
	char name[64], value[64];
	int count;
	int input_len = strlen("<input ");
	int name_len = strlen("name=\"");
	int value_len = strlen("value=\"");
	while ((len > ((c - buf) + input_len))
			&& (c = strstr(c, "<input "))) {
		if (!(c = g_strstr_len(c, len - (c - buf), "name=\"")))
			continue;
		c += name_len;
		count = sizeof(name)-1;
		for (d = name; (len > ((c - buf) + 1)) && *c!='"'
				&& count; c++, d++, count--)
			*d = *c;
		*d = '\0';
		count = sizeof(value)-1;
		if (!(d = g_strstr_len(c, len - (c - buf), "value=\"")))
			continue;
		d += value_len;
		if (strchr(c, '>') < d)
			break;
		for (c = d, d = value; (len > ((c - buf) + 1))
				&& *c!='"' && count; c++, d++, count--)
			*d = *c;
		*d = '\0';
		g_hash_table_insert(hash, g_strdup(name), g_strdup(value));
	}
	return hash;
}

static void yahoo_login_page_cb(void *user_data, const char *buf, size_t len)
{
	GaimConnection *gc = (GaimConnection *)user_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	const char *sn = gaim_account_get_username(account);
	const char *pass = gaim_account_get_password(account);
	GHashTable *hash = yahoo_login_page_hash(buf, len);
	GString *url = g_string_new("GET http://login.yahoo.com/config/login?login=");
	char md5[33], *hashp = md5, *chal;
	int i;
	md5_byte_t result[16];
	md5_state_t ctx;

	url = g_string_append(url, sn);
	url = g_string_append(url, "&passwd=");

	md5_init(&ctx);
	md5_append(&ctx, pass, strlen(pass));
	md5_finish(&ctx, result);
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", result[i]);
		hashp += 2;
	}
	chal = g_strconcat(md5, g_hash_table_lookup(hash, ".challenge"), NULL);
	md5_init(&ctx);
	md5_append(&ctx, chal, strlen(chal));
	md5_finish(&ctx, result);
	hashp = md5;
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", result[i]);
		hashp += 2;
	}
	/*
	md5_init(&ctx);
	md5_append(&ctx, md5, strlen(md5));
	md5_finish(&ctx, result);
	hashp = md5;
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", result[i]);
		hashp += 2;
	}
	*/
	g_free(chal);
	
	url = g_string_append(url, md5);
	g_hash_table_foreach(hash, (GHFunc)yahoo_login_page_hash_iter, url);
	
	url = g_string_append(url, "&.hash=1&.md5=1 HTTP/1.1\r\n"
			      "Host: login.yahoo.com\r\n\r\n");
	g_hash_table_destroy(hash);
	yd->auth = g_string_free(url, FALSE);
	if (gaim_proxy_connect(account, "login.yahoo.com", 80, yahoo_got_cookies, gc) != 0) {
		gaim_connection_error(gc, _("Connection problem"));
		return;
	}
}

static void yahoo_server_check(GaimAccount *account)
{
	const char *server;

	server = gaim_account_get_string(account, "server", YAHOO_PAGER_HOST);

	if (strcmp(server, "scs.yahoo.com") == 0)
		gaim_account_set_string(account, "server", YAHOO_PAGER_HOST);
}

static void yahoo_picture_check(GaimAccount *account)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	const char *buddyicon;

	buddyicon = gaim_account_get_buddy_icon(account);
	yahoo_set_buddy_icon(gc, buddyicon);
}


static void yahoo_login(GaimAccount *account) {
	GaimConnection *gc = gaim_account_get_connection(account);
	struct yahoo_data *yd = gc->proto_data = g_new0(struct yahoo_data, 1);

	gc->flags |= GAIM_CONNECTION_HTML | GAIM_CONNECTION_NO_BGCOLOR | GAIM_CONNECTION_NO_URLDESC;

	gaim_connection_update_progress(gc, _("Connecting"), 1, 2);

	gaim_connection_set_display_name(gc, gaim_account_get_username(account));

	yd->fd = -1;
	yd->friends = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, yahoo_friend_free);
	yd->confs = NULL;
	yd->conf_id = 2;

	yahoo_server_check(account);
	yahoo_picture_check(account);

	if (gaim_account_get_bool(account, "yahoojp", FALSE)) {
		yd->jp = TRUE;
		if (gaim_proxy_connect(account,
		                       gaim_account_get_string(account, "serverjp",  YAHOOJP_PAGER_HOST),
		                       gaim_account_get_int(account, "port", YAHOO_PAGER_PORT),
		                       yahoo_got_connected, gc) != 0)
		{
			gaim_connection_error(gc, _("Connection problem"));
			return;
		}
	} else {
		yd->jp = FALSE;
		if (gaim_proxy_connect(account,
		                       gaim_account_get_string(account, "server",  YAHOO_PAGER_HOST),
		                       gaim_account_get_int(account, "port", YAHOO_PAGER_PORT),
		                       yahoo_got_connected, gc) != 0)
		{
			gaim_connection_error(gc, _("Connection problem"));
			return;
		}
	}
}

static void yahoo_close(GaimConnection *gc) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	GSList *l;

	for (l = yd->confs; l; l = l->next) {
		GaimConversation *conv = l->data;

		yahoo_conf_leave(yd, gaim_conversation_get_name(conv),
		                 gaim_connection_get_display_name(gc),
				 gaim_conv_chat_get_users(GAIM_CONV_CHAT(conv)));
	}
	g_slist_free(yd->confs);

	g_hash_table_destroy(yd->friends);
	if (yd->chat_name)
		g_free(yd->chat_name);

	if (yd->cookie_y)
		g_free(yd->cookie_y);
	if (yd->cookie_t)
		g_free(yd->cookie_t);

	if (yd->fd >= 0)
		close(yd->fd);

	if (yd->rxqueue)
		g_free(yd->rxqueue);
	yd->rxlen = 0;
	if (yd->picture_url)
		g_free(yd->picture_url);
	if (yd->picture_upload_todo)
		yahoo_buddy_icon_upload_data_free(yd->picture_upload_todo);
	if (yd->ycht)
		ycht_connection_close(yd->ycht);
	if (gc->inpa)
		gaim_input_remove(gc->inpa);
	g_free(yd);
}

static const char *yahoo_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "yahoo";
}

static void yahoo_list_emblems(GaimBuddy *b, char **se, char **sw, char **nw, char **ne)
{
	int i = 0;
	char *emblems[4] = {NULL,NULL,NULL,NULL};
	GaimAccount *account;
	GaimConnection *gc;
	struct yahoo_data *yd;
	YahooFriend *f;

	if (!b || !(account = b->account) || !(gc = gaim_account_get_connection(account)) ||
	  				     !(yd = gc->proto_data))
		return;

	f = yahoo_friend_find(gc, b->name);
	if (!f) {
		*se = "notauthorized";
		return;
	}

	if (b->present == GAIM_BUDDY_OFFLINE) {
		*se = "offline";
		return;
	} else {
		if (f->away)
			emblems[i++] = "away";
		if (f->sms)
			emblems[i++] = "wireless";
		if (yahoo_friend_get_game(f))
			emblems[i++] = "game";
	}
	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

static char *yahoo_get_status_string(enum yahoo_status a)
{
	switch (a) {
	case YAHOO_STATUS_BRB:
		return _("Be Right Back");
	case YAHOO_STATUS_BUSY:
		return _("Busy");
	case YAHOO_STATUS_NOTATHOME:
		return _("Not At Home");
	case YAHOO_STATUS_NOTATDESK:
		return _("Not At Desk");
	case YAHOO_STATUS_NOTINOFFICE:
		return _("Not In Office");
	case YAHOO_STATUS_ONPHONE:
		return _("On The Phone");
	case YAHOO_STATUS_ONVACATION:
		return _("On Vacation");
	case YAHOO_STATUS_OUTTOLUNCH:
		return _("Out To Lunch");
	case YAHOO_STATUS_STEPPEDOUT:
		return _("Stepped Out");
	case YAHOO_STATUS_INVISIBLE:
		return _("Invisible");
	case YAHOO_STATUS_IDLE:
		return _("Idle");
	case YAHOO_STATUS_OFFLINE:
		return _("Offline");
	default:
		return _("Online");
	}
}

static void yahoo_initiate_conference(GaimBlistNode *node, gpointer data) {

	GaimBuddy *buddy;
	GaimConnection *gc;

	GHashTable *components;
	struct yahoo_data *yd;
	int id;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);
	yd = gc->proto_data;
	id = yd->conf_id;

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(components, g_strdup("room"),
		g_strdup_printf("%s-%d", gaim_connection_get_display_name(gc), id));
	g_hash_table_replace(components, g_strdup("topic"), g_strdup("Join my conference..."));
	g_hash_table_replace(components, g_strdup("type"), g_strdup("Conference"));
	yahoo_c_join(gc, components);
	g_hash_table_destroy(components);

	yahoo_c_invite(gc, id, "Join my conference...", buddy->name);
}

static void yahoo_game(GaimBlistNode *node, gpointer data) {

	GaimBuddy *buddy;
	GaimConnection *gc;

	struct yahoo_data *yd;
	const char *game;
	char *game2;
	char *t;
	char url[256];
	YahooFriend *f;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);
	yd = (struct yahoo_data *) gc->proto_data;

	f = yahoo_friend_find(gc, buddy->name);
	if (!f)
		return;

	game = yahoo_friend_get_game(f);
	if (!game)
		return;

	t = game2 = g_strdup(strstr(game, "ante?room="));
	while (*t && *t != '\t')
		t++;
	*t = 0;
	g_snprintf(url, sizeof url, "http://games.yahoo.com/games/%s", game2);
	gaim_notify_uri(gc, url);
	g_free(game2);
}

static char *yahoo_status_text(GaimBuddy *b)
{
	YahooFriend *f = NULL;
	const char *msg;

	f = yahoo_friend_find(b->account->gc, b->name);
	if (!f)
		return g_strdup(_("Not on server list"));

	switch (f->status) {
	case YAHOO_STATUS_AVAILABLE:
		return NULL;
	case YAHOO_STATUS_IDLE:
		if (f->idle == -1)
			return g_strdup(yahoo_get_status_string(f->status));
		return NULL;
	case YAHOO_STATUS_CUSTOM:
		if (!(msg = yahoo_friend_get_status_message(f)))
			return NULL;
		return g_markup_escape_text(msg, strlen(msg));

	default:
		return g_strdup(yahoo_get_status_string(f->status));
 	}
}

char *yahoo_tooltip_text(GaimBuddy *b)
{
	YahooFriend *f;
	char *escaped, *status, *ret;

	f = yahoo_friend_find(b->account->gc, b->name);
	if (!f)
		status = g_strdup_printf("\n%s", _("Not on server list"));
	else
		switch (f->status) {
		case YAHOO_STATUS_IDLE:
			if (f->idle == -1) {
				status = g_strdup(yahoo_get_status_string(f->status));
				break;
			}
			return NULL;
		case YAHOO_STATUS_CUSTOM:
			if (!yahoo_friend_get_status_message(f))
				return NULL;
			status = g_strdup(yahoo_friend_get_status_message(f));
			break;
		default:
			status = g_strdup(yahoo_get_status_string(f->status));
			break;
		}

	escaped = g_markup_escape_text(status, strlen(status));
	ret = g_strdup_printf(_("\n<b>%s:</b> %s"), _("Status"), escaped);
	g_free(status);
	g_free(escaped);

	return ret;
}

static void yahoo_addbuddyfrommenu_cb(GaimBlistNode *node, gpointer data)
{
	GaimBuddy *buddy;
	GaimConnection *gc;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);

	yahoo_add_buddy(gc, buddy, NULL);
}


static void yahoo_chat_goto_menu(GaimBlistNode *node, gpointer data)
{
	GaimBuddy *buddy;
	GaimConnection *gc;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);

	yahoo_chat_goto(gc, buddy->name);
}


static GList *yahoo_buddy_menu(GaimBuddy *buddy)
{
	GList *m = NULL;
	GaimBlistNodeAction *act;

	GaimConnection *gc = gaim_account_get_connection(buddy->account);
	struct yahoo_data *yd = gc->proto_data;
	static char buf2[1024];
	YahooFriend *f;

	f = yahoo_friend_find(gc, buddy->name);

	if (!f && !yd->wm) {
		act = gaim_blist_node_action_new(_("Add Buddy"),
				yahoo_addbuddyfrommenu_cb, NULL);
		m = g_list_append(m, act);

		return m;

	} else if (f->status == YAHOO_STATUS_OFFLINE) {
		return NULL;
	}

	if (!yd->wm) {
		act = gaim_blist_node_action_new(_("Join in Chat"),
				yahoo_chat_goto_menu, NULL);
		m = g_list_append(m, act);
	}

	act = gaim_blist_node_action_new(_("Initiate Conference"),
			yahoo_initiate_conference, NULL);
	m = g_list_append(m, act);

	if (yahoo_friend_get_game(f)) {
		const char *game = yahoo_friend_get_game(f);
		char *room;
		char *t;

		if (!(room = strstr(game, "&follow="))) /* skip ahead to the url */
			return m;
		while (*room && *room != '\t')          /* skip to the tab */
			room++;
		t = room++;                             /* room as now at the name */
		while (*t != '\n')
			t++;                            /* replace the \n with a space */
		*t = ' ';
		g_snprintf(buf2, sizeof buf2, "%s", room);

		act = gaim_blist_node_action_new(buf2, yahoo_game, NULL);
		m = g_list_append(m, act);
	}

	return m;
}


static GList *yahoo_blist_node_menu(GaimBlistNode *node)
{
	if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		return yahoo_buddy_menu((GaimBuddy *) node);
	} else {
		return NULL;
	}
}


static void yahoo_act_id(GaimConnection *gc, const char *entry)
{
	struct yahoo_data *yd = gc->proto_data;

	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_IDACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 3, entry);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

	gaim_connection_set_display_name(gc, entry);
}

static void yahoo_show_act_id(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_input(gc, NULL, _("Active which ID?"), NULL,
					   gaim_connection_get_display_name(gc), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(yahoo_act_id),
					   _("Cancel"), NULL, gc);
}

static void yahoo_show_chat_goto(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_input(gc, NULL, _("Join who in chat?"), NULL,
					   "", FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(yahoo_chat_goto),
					   _("Cancel"), NULL, gc);
}

static GList *yahoo_actions(GaimPlugin *plugin, gpointer context) {
	GList *m = NULL;
	GaimPluginAction *act;

	act = gaim_plugin_action_new(_("Activate ID..."),
			yahoo_show_act_id);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Join user in chat..."),
			yahoo_show_chat_goto);
	m = g_list_append(m, act);

	return m;
}

static int yahoo_send_im(GaimConnection *gc, const char *who, const char *what, GaimConvImFlags flags)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_MESSAGE, YAHOO_STATUS_OFFLINE, 0);
	char *msg = yahoo_html_to_codes(what);
	char *msg2;
	gboolean utf8 = TRUE;
	int ret = 1;

	msg2 = yahoo_string_encode(gc, msg, &utf8);

	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 5, who);
	if (utf8)
		yahoo_packet_hash(pkt, 97, "1");
	yahoo_packet_hash(pkt, 14, msg2);

	yahoo_packet_hash(pkt,   63, ";0"); /* IMvironment */
	yahoo_packet_hash(pkt,   64, "0"); /* no idea */
	yahoo_packet_hash(pkt, 1002, "1"); /* no idea, Yahoo 6 or later only it seems */
	if (!yd->picture_url)
		yahoo_packet_hash(pkt, 206, "0"); /* 0 = no picture, 2 = picture, maybe 1 = avatar? */
	else
		yahoo_packet_hash(pkt, 206, "2");

	/* We may need to not send any packets over 2000 bytes, but I'm not sure yet. */
	if ((YAHOO_PACKET_HDRLEN + yahoo_packet_length(pkt)) <= 2000)
		yahoo_send_packet(yd, pkt);
	else
		ret = -E2BIG;

	yahoo_packet_free(pkt);

	g_free(msg);
	g_free(msg2);

	return ret;
}

int yahoo_send_typing(GaimConnection *gc, const char *who, int typ)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_NOTIFY, YAHOO_STATUS_TYPING, 0);
	yahoo_packet_hash(pkt, 49, "TYPING");
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 14, " ");
	yahoo_packet_hash(pkt, 13, typ == GAIM_TYPING ? "1" : "0");
	yahoo_packet_hash(pkt, 5, who);
	yahoo_packet_hash(pkt, 1002, "1");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	return 0;
}

static void yahoo_set_away(GaimConnection *gc, const char *state, const char *msg)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;
	int service;
	char s[4];
	char *conv_msg = NULL;
	char *conv_msg2 = NULL;

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (msg) {
		yd->current_status = YAHOO_STATUS_CUSTOM;
		gc->away = g_strndup(msg, YAHOO_MAX_STATUS_MESSAGE_LENGTH);
	} else if (state) {
		gc->away = g_strdup("");
		if (!strcmp(state, _("Available"))) {
			yd->current_status = YAHOO_STATUS_AVAILABLE;
		} else if (!strcmp(state, _("Be Right Back"))) {
			yd->current_status = YAHOO_STATUS_BRB;
		} else if (!strcmp(state, _("Busy"))) {
			yd->current_status = YAHOO_STATUS_BUSY;
		} else if (!strcmp(state, _("Not At Home"))) {
			yd->current_status = YAHOO_STATUS_NOTATHOME;
		} else if (!strcmp(state, _("Not At Desk"))) {
			yd->current_status = YAHOO_STATUS_NOTATDESK;
		} else if (!strcmp(state, _("Not In Office"))) {
			yd->current_status = YAHOO_STATUS_NOTINOFFICE;
		} else if (!strcmp(state, _("On The Phone"))) {
			yd->current_status = YAHOO_STATUS_ONPHONE;
		} else if (!strcmp(state, _("On Vacation"))) {
			yd->current_status = YAHOO_STATUS_ONVACATION;
		} else if (!strcmp(state, _("Out To Lunch"))) {
			yd->current_status = YAHOO_STATUS_OUTTOLUNCH;
		} else if (!strcmp(state, _("Stepped Out"))) {
			yd->current_status = YAHOO_STATUS_STEPPEDOUT;
		} else if (!strcmp(state, _("Invisible"))) {
			yd->current_status = YAHOO_STATUS_INVISIBLE;
		} else if (!strcmp(state, GAIM_AWAY_CUSTOM)) { /* this should never happen? */
			if (gc->is_idle) {
				yd->current_status = YAHOO_STATUS_IDLE;
			} else {
				yd->current_status = YAHOO_STATUS_AVAILABLE;
			}
		}
	} else if (gc->is_idle) {
		yd->current_status = YAHOO_STATUS_IDLE;
	} else {
		yd->current_status = YAHOO_STATUS_AVAILABLE;
	}

	if (yd->current_status == YAHOO_STATUS_AVAILABLE)
		service = YAHOO_SERVICE_ISBACK;
	else
		service = YAHOO_SERVICE_ISAWAY;

	pkt = yahoo_packet_new(service, YAHOO_STATUS_AVAILABLE, 0);
	g_snprintf(s, sizeof(s), "%d", yd->current_status);
	yahoo_packet_hash(pkt, 10, s);

	if ((yd->current_status == YAHOO_STATUS_CUSTOM) && gc->away) {
		conv_msg = yahoo_string_encode(gc, gc->away, NULL);
		conv_msg2 = gaim_markup_strip_html(conv_msg);
		yahoo_packet_hash(pkt, 19, conv_msg2);
	}

	if ((yd->current_status != YAHOO_STATUS_AVAILABLE) &&
	    (yd->current_status != YAHOO_STATUS_IDLE)) {
		if (gc->is_idle)
			yahoo_packet_hash(pkt, 47, "2");
		else
			yahoo_packet_hash(pkt, 47, "1");
	}

	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
	if (conv_msg)
		g_free(conv_msg);
	if (conv_msg2)
		g_free(conv_msg2);
}

static void yahoo_set_idle(GaimConnection *gc, int idle)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = NULL;
	char *msg = NULL, *msg2 = NULL;

	if (idle && yd->current_status == YAHOO_STATUS_AVAILABLE) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_AVAILABLE, 0);
		yd->current_status = YAHOO_STATUS_IDLE;
	} else if (!idle && yd->current_status == YAHOO_STATUS_IDLE) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_AVAILABLE, 0);
		yd->current_status = YAHOO_STATUS_AVAILABLE;
	} else {
		pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_AVAILABLE, 0);
	}

	if (pkt) {
		char buf[4];
		g_snprintf(buf, sizeof(buf), "%d", yd->current_status);
		yahoo_packet_hash(pkt, 10, buf);
		if (gc->away && yd->current_status == YAHOO_STATUS_CUSTOM) {
			msg = yahoo_string_encode(gc, gc->away, NULL);
			msg2 = gaim_unescape_html(msg);
			yahoo_packet_hash(pkt, 19, msg2);
			if (idle)
				yahoo_packet_hash(pkt, 47, "2");
			else
				yahoo_packet_hash(pkt, 47, "1"); /* fixme when available messages are possible */
		} else if (idle && (yd->current_status != YAHOO_STATUS_AVAILABLE) &&
			   (yd->current_status != YAHOO_STATUS_IDLE)) {
			yahoo_packet_hash(pkt, 47, "2");
		} else if (!idle && (yd->current_status != YAHOO_STATUS_AVAILABLE) &&
			   (yd->current_status != YAHOO_STATUS_IDLE)) {
			yahoo_packet_hash(pkt, 47, "1");
		}

		yahoo_send_packet(yd, pkt);
		yahoo_packet_free(pkt);
	}
	if (msg)
		g_free(msg);
	if (msg2)
		g_free(msg2);
}

static GList *yahoo_away_states(GaimConnection *gc)
{
	GList *m = NULL;
	struct yahoo_data *yd = gc->proto_data;

	m = g_list_append(m, _("Available"));
	if (!yd->wm) {
		m = g_list_append(m, _("Be Right Back"));
		m = g_list_append(m, _("Busy"));
		m = g_list_append(m, _("Not At Home"));
		m = g_list_append(m, _("Not At Desk"));
		m = g_list_append(m, _("Not In Office"));
		m = g_list_append(m, _("On The Phone"));
		m = g_list_append(m, _("On Vacation"));
		m = g_list_append(m, _("Out To Lunch"));
		m = g_list_append(m, _("Stepped Out"));
	}
	m = g_list_append(m, _("Invisible"));
	if (!yd->wm)
		m = g_list_append(m, GAIM_AWAY_CUSTOM);

	return m;
}

static void yahoo_keepalive(GaimConnection *gc)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_PING, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

	if (!yd->chat_online)
		return;

	if (yd->wm) {
		ycht_chat_send_keepalive(yd->ycht);
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATPING, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 109, gaim_connection_get_display_name(gc));
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

/* XXX - What's the deal with GaimGroup *foo? */
static void yahoo_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *foo)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;
	GaimGroup *g;
	char *group = NULL;
	char *group2 = NULL;

	if (!yd->logged_in)
		return;

	if (!yahoo_privacy_check(gc, buddy->name))
		return;

	if (foo)
		group = foo->name;
	if (!group) {
		g = gaim_find_buddys_group(gaim_find_buddy(gc->account, buddy->name));
		if (g)
			group = g->name;
		else
			group = "Buddies";
	}

	group2 = yahoo_string_encode(gc, group, NULL);
	pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, buddy->name);
	yahoo_packet_hash(pkt, 65, group2);
	yahoo_packet_hash(pkt, 14, "");
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
	g_free(group2);
}

static void yahoo_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	YahooFriend *f;
        struct yahoo_packet *pkt;
	GSList *buddies, *l;
	GaimGroup *g;
	gboolean remove = TRUE;
	char *cg;

	if (!(f = yahoo_friend_find(gc, buddy->name)))
		return;

	buddies = gaim_find_buddies(gaim_connection_get_account(gc), buddy->name);
	for (l = buddies; l; l = l->next) {
		g = gaim_find_buddys_group(l->data);
		if (gaim_utf8_strcasecmp(group->name, g->name)) {
			remove = FALSE;
			break;
		}
	}

	g_slist_free(buddies);

	if (remove)
		g_hash_table_remove(yd->friends, buddy->name);

	cg = yahoo_string_encode(gc, group->name, NULL);
	pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, buddy->name);
	yahoo_packet_hash(pkt, 65, cg);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
	g_free(cg);
}

static void yahoo_add_deny(GaimConnection *gc, const char *who) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;

	if (!yd->logged_in)
		return;
	/* It seems to work better without this */

	/* if (gc->account->perm_deny != 4)
		return; */

	if (!who || who[0] == '\0')
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_IGNORECONTACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 13, "1");
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_rem_deny(GaimConnection *gc, const char *who) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;

	if (!yd->logged_in)
		return;

	if (!who || who[0] == '\0')
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_IGNORECONTACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 13, "2");
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_set_permit_deny(GaimConnection *gc) {
	GaimAccount *acct;
	GSList *deny;

	acct = gc->account;

	switch (acct->perm_deny) {
		/* privacy 1 */
		case GAIM_PRIVACY_ALLOW_ALL:
			for (deny = acct->deny;deny;deny = deny->next)
				yahoo_rem_deny(gc, deny->data);
			break;
		/* privacy 3 */
		case GAIM_PRIVACY_ALLOW_USERS:
			for (deny = acct->deny;deny;deny = deny->next)
				yahoo_rem_deny(gc, deny->data);
			break;
		/* privacy 5 */
		case GAIM_PRIVACY_ALLOW_BUDDYLIST:
		/* privacy 4 */
		case GAIM_PRIVACY_DENY_USERS:
			for (deny = acct->deny;deny;deny = deny->next)
				yahoo_add_deny(gc, deny->data);
			break;
		/* privacy 2 */
		case GAIM_PRIVACY_DENY_ALL:
		default:
			break;
        }
}

static gboolean yahoo_unload_plugin(GaimPlugin *plugin)
{
	yahoo_dest_colorht();
	return TRUE;
}

static void yahoo_change_buddys_group(GaimConnection *gc, const char *who,
				   const char *old_group, const char *new_group)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	char *gpn, *gpo;

	/* Step 0:  If they aren't on the server list anyway,
	 *          don't bother letting the server know.
	 */
	if (!yahoo_friend_find(gc, who))
		return;

	/* If old and new are the same, we would probably
	 * end up deleting the buddy, which would be bad.
	 * This might happen because of the charset conversation.
	 */
	gpn = yahoo_string_encode(gc, new_group, NULL);
	gpo = yahoo_string_encode(gc, old_group, NULL);
	if (!strcmp(gpn, gpo)) {
		g_free(gpn);
		g_free(gpo);
		return;
	}

	/* Step 1:  Add buddy to new group. */
	pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, gpn);
	yahoo_packet_hash(pkt, 14, "");
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

#if 0
	/* Step 2:  Remove buddy from old group */
	pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, gpo);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
#endif
	g_free(gpn);
	g_free(gpo);
}

static void yahoo_rename_group(GaimConnection *gc, const char *old_name,
							   GaimGroup *group, GList *moved_buddies)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	char *gpn, *gpo;

	gpn = yahoo_string_encode(gc, group->name, NULL);
	gpo = yahoo_string_encode(gc, old_name, NULL);
	if (!strcmp(gpn, gpo)) {
		g_free(gpn);
		g_free(gpo);
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_GROUPRENAME, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 65, gpo);
	yahoo_packet_hash(pkt, 67, gpn);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
	g_free(gpn);
	g_free(gpo);
}

static GaimPlugin *my_protocol = NULL;

/********************************* Commands **********************************/

static GaimCmdRet
yahoogaim_cmd_buzz(GaimConversation *c, const gchar *cmd, gchar **args,
					gchar **error, void *data) {

	GaimAccount *account = gaim_conversation_get_account(c);
	const char *username = gaim_account_get_username(account);

	if (*args && args[0])
		return GAIM_CMD_RET_FAILED;

	gaim_debug(GAIM_DEBUG_INFO, "yahoo",
	           "Sending <ding> on account %s to buddy %s.\n", username, c->name);
	gaim_conv_im_send(GAIM_CONV_IM(c), "<ding>");
	gaim_conv_im_write(GAIM_CONV_IM(c), "", _("Buzz!!"), 
		GAIM_MESSAGE_NICK|GAIM_MESSAGE_RECV, time(NULL));
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet
yahoogaim_cmd_chat_join(GaimConversation *conv, const char *cmd,
                        char **args, char **error, void *data)
{
	GHashTable *comp;
	GaimConnection *gc;
	struct yahoo_data *yd;
	int id;

	if (!args || !args[0])
		return GAIM_CMD_RET_FAILED;

	gc = gaim_conversation_get_gc(conv);
	yd = gc->proto_data;
	id = yd->conf_id;
	gaim_debug(GAIM_DEBUG_INFO, "yahoo",
	           "Trying to join %s \n", args[0]);

	comp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(comp, g_strdup("room"),
	g_strdup_printf("%s", g_ascii_strdown(args[0], strlen(args[0]))));
	g_hash_table_replace(comp, g_strdup("type"), g_strdup("Chat"));

	yahoo_c_join(gc, comp);

	g_hash_table_destroy(comp);
	return GAIM_CMD_RET_OK;
}

/************************** Plugin Initialization ****************************/
static void
yahoogaim_register_commands(void)
{
	gaim_cmd_register("join", "s", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT |
	                  GAIM_CMD_FLAG_PRPL_ONLY,
	                  "prpl-yahoo", yahoogaim_cmd_chat_join,
	                  _("join &lt;room&gt;:  Join a chat room on the Yahoo network"), NULL);
	gaim_cmd_register("buzz", "", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_PRPL_ONLY,
	                  "prpl-yahoo", yahoogaim_cmd_buzz,
	                  _("buzz: Buzz a contact to get their attention"), NULL);
}
static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_CHAT_TOPIC,
	NULL, /* user_splits */
	NULL, /* protocol_options */
	{"png", 96, 96, 96, 96, GAIM_ICON_SCALE_SEND},
	yahoo_list_icon,
	yahoo_list_emblems,
	yahoo_status_text,
	yahoo_tooltip_text,
	yahoo_away_states,
	yahoo_blist_node_menu,
	yahoo_c_info,
	yahoo_c_info_defaults,
	yahoo_login,
	yahoo_close,
	yahoo_send_im,
	NULL, /* set info */
	yahoo_send_typing,
	yahoo_get_info,
	yahoo_set_away,
	yahoo_set_idle,
	NULL, /* change_passwd*/
	yahoo_add_buddy,
	NULL, /* add_buddies */
	yahoo_remove_buddy,
	NULL, /*remove_buddies */
	yahoo_add_permit,
	yahoo_add_deny,
	yahoo_rem_permit,
	yahoo_rem_deny,
	yahoo_set_permit_deny,
	NULL, /* warn */
	yahoo_c_join,
	NULL, /* reject chat invite */
	yahoo_get_chat_name,
	yahoo_c_invite,
	yahoo_c_leave,
	NULL, /* chat whisper */
	yahoo_c_send,
	yahoo_keepalive,
	NULL, /* register_user */
	NULL, /* get_cb_info */
	NULL, /* get_cb_away */
	NULL, /* alias_buddy */
	yahoo_change_buddys_group,
	yahoo_rename_group,
	NULL, /* buddy_free */
	NULL, /* convo_closed */
	gaim_normalize_nocase, /* normalize */
	yahoo_set_buddy_icon,
	NULL, /* void (*remove_group)(GaimConnection *gc, const char *group);*/
	NULL, /* char *(*get_cb_real_name)(GaimConnection *gc, int id, const char *who); */
	NULL, /* set_chat_topic */
	NULL, /* find_blist_chat */
	yahoo_roomlist_get_list,
	yahoo_roomlist_cancel,
	yahoo_roomlist_expand_category,
	NULL, /* can_receive_file */
	yahoo_send_file
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

	"prpl-yahoo",                                     /**< id             */
	"Yahoo",	                                      /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Yahoo Protocol Plugin"),
	                                                  /**  description    */
	N_("Yahoo Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	yahoo_unload_plugin,                              /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	yahoo_actions
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_bool_new(_("Yahoo Japan"), "yahoojp", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Pager host"), "server", YAHOO_PAGER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Japan Pager host"), "serverjp", YAHOOJP_PAGER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("Pager port"), "port", YAHOO_PAGER_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("File transfer host"), "xfer_host", YAHOO_XFER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Japan File transfer host"), "xferjp_host", YAHOOJP_XFER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("File transfer port"), "xfer_port", YAHOO_XFER_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Chat Room Locale"), "room_list_locale", YAHOO_ROOMLIST_LOCALE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#if 0
	option = gaim_account_option_string_new(_("Chat Room List Url"), "room_list", YAHOO_ROOMLIST_URL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("YCHT Host"), "ycht-server", YAHOO_YCHT_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("YCHT Port"), "ycht-port", YAHOO_YCHT_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#endif

	my_protocol = plugin;
	yahoogaim_register_commands();
	yahoo_init_colorht();
}

GAIM_INIT_PLUGIN(yahoo, init_plugin, info);
