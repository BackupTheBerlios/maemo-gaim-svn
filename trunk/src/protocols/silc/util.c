/*

  silcgaim_util.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2004 - 2005 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#include "silcincludes.h"
#include "silcclient.h"
#include "silcgaim.h"

/**************************** Utility Routines *******************************/

static char str[256], str2[256];

const char *silcgaim_silcdir(void)
{
	const char *hd = gaim_home_dir();
	memset(str, 0, sizeof(str));
	g_snprintf(str, sizeof(str) - 1, "%s" G_DIR_SEPARATOR_S ".silc", hd ? hd : "/tmp");
	return (const char *)str;
}

const char *silcgaim_session_file(const char *account)
{
	memset(str2, 0, sizeof(str2));
	g_snprintf(str2, sizeof(str2) - 1, "%s" G_DIR_SEPARATOR_S "%s_session",
		   silcgaim_silcdir(), account);
	return (const char *)str2;
}

gboolean silcgaim_ip_is_private(const char *ip)
{
	if (silc_net_is_ip4(ip)) {
		if (!strncmp(ip, "10.", 3)) {
			return TRUE;
		} else if (!strncmp(ip, "172.", 4) && strlen(ip) > 6) {
			char tmp[3];
			int s;
			memset(tmp, 0, sizeof(tmp));
			strncpy(tmp, ip + 4, 2);
			s = atoi(tmp);
			if (s >= 16 && s <= 31)
				return TRUE;
		} else if (!strncmp(ip, "192.168.", 8)) {
			return TRUE;
		}
	}

	return FALSE;
}

/* This checks stats for various SILC files and directories. First it
   checks if ~/.silc directory exist and is owned by the correct user. If
   it doesn't exist, it will create the directory. After that it checks if
   user's Public and Private key files exists and creates them if needed. */

gboolean silcgaim_check_silc_dir(GaimConnection *gc)
{
	char filename[256], file_public_key[256], file_private_key[256];
	char servfilename[256], clientfilename[256], friendsfilename[256];
	char pkd[256], prd[256];
	struct stat st;
	struct passwd *pw;

	pw = getpwuid(getuid());
	if (!pw) {
		gaim_debug_error("silc", "silc: %s\n", strerror(errno));
		return FALSE;
	}

	g_snprintf(filename, sizeof(filename) - 1, "%s", silcgaim_silcdir());
	g_snprintf(servfilename, sizeof(servfilename) - 1, "%s" G_DIR_SEPARATOR_S "serverkeys",
		   silcgaim_silcdir());
	g_snprintf(clientfilename, sizeof(clientfilename) - 1, "%s" G_DIR_SEPARATOR_S "clientkeys",
		   silcgaim_silcdir());
	g_snprintf(friendsfilename, sizeof(friendsfilename) - 1, "%s" G_DIR_SEPARATOR_S "friends",
		   silcgaim_silcdir());

	/*
	 * Check ~/.silc directory
	 */
	if ((g_stat(filename, &st)) == -1) {
		/* If dir doesn't exist */
		if (errno == ENOENT) {
			if (pw->pw_uid == geteuid()) {
				if ((g_mkdir(filename, 0755)) == -1) {
					gaim_debug_error("silc", "Couldn't create '%s' directory\n", filename);
					return FALSE;
				}
			} else {
				gaim_debug_error("silc", "Couldn't create '%s' directory due to a wrong uid!\n",
					filename);
				return FALSE;
			}
		} else {
			gaim_debug_error("silc", "Couldn't stat '%s' directory, error: %s\n", filename, strerror(errno));
			return FALSE;
		}
	} else {
#ifndef _WIN32
		/* Check the owner of the dir */
		if (st.st_uid != 0 && st.st_uid != pw->pw_uid) {
			gaim_debug_error("silc", "You don't seem to own '%s' directory\n",
				filename);
			return FALSE;
		}
#endif
	}

	/*
	 * Check ~./silc/serverkeys directory
	 */
	if ((g_stat(servfilename, &st)) == -1) {
		/* If dir doesn't exist */
		if (errno == ENOENT) {
			if (pw->pw_uid == geteuid()) {
				if ((g_mkdir(servfilename, 0755)) == -1) {
					gaim_debug_error("silc", "Couldn't create '%s' directory\n", servfilename);
					return FALSE;
				}
			} else {
				gaim_debug_error("silc", "Couldn't create '%s' directory due to a wrong uid!\n",
					servfilename);
				return FALSE;
			}
		} else {
			gaim_debug_error("silc", "Couldn't stat '%s' directory, error: %s\n",
							 servfilename, strerror(errno));
			return FALSE;
		}
	}

	/*
	 * Check ~./silc/clientkeys directory
	 */
	if ((g_stat(clientfilename, &st)) == -1) {
		/* If dir doesn't exist */
		if (errno == ENOENT) {
			if (pw->pw_uid == geteuid()) {
				if ((g_mkdir(clientfilename, 0755)) == -1) {
					gaim_debug_error("silc", "Couldn't create '%s' directory\n", clientfilename);
					return FALSE;
				}
			} else {
				gaim_debug_error("silc", "Couldn't create '%s' directory due to a wrong uid!\n",
					clientfilename);
				return FALSE;
			}
		} else {
			gaim_debug_error("silc", "Couldn't stat '%s' directory, error: %s\n",
							 clientfilename, strerror(errno));
			return FALSE;
		}
	}

	/*
	 * Check ~./silc/friends directory
	 */
	if ((g_stat(friendsfilename, &st)) == -1) {
		/* If dir doesn't exist */
		if (errno == ENOENT) {
			if (pw->pw_uid == geteuid()) {
				if ((g_mkdir(friendsfilename, 0755)) == -1) {
					gaim_debug_error("silc", "Couldn't create '%s' directory\n", friendsfilename);
					return FALSE;
				}
			} else {
				gaim_debug_error("silc", "Couldn't create '%s' directory due to a wrong uid!\n",
					friendsfilename);
				return FALSE;
			}
		} else {
			gaim_debug_error("silc", "Couldn't stat '%s' directory, error: %s\n",
							 friendsfilename, strerror(errno));
			return FALSE;
		}
	}

	/*
	 * Check Public and Private keys
	 */
	g_snprintf(pkd, sizeof(pkd), "%s" G_DIR_SEPARATOR_S "public_key.pub", silcgaim_silcdir());
	g_snprintf(prd, sizeof(prd), "%s" G_DIR_SEPARATOR_S "private_key.prv", silcgaim_silcdir());
	g_snprintf(file_public_key, sizeof(file_public_key) - 1, "%s",
		   gaim_account_get_string(gc->account, "public-key", pkd));
	g_snprintf(file_private_key, sizeof(file_public_key) - 1, "%s",
		   gaim_account_get_string(gc->account, "private-key", prd));

	if ((g_stat(file_public_key, &st)) == -1) {
		/* If file doesn't exist */
		if (errno == ENOENT) {
			gaim_connection_update_progress(gc, _("Creating SILC key pair..."), 1, 5);
			silc_create_key_pair(SILCGAIM_DEF_PKCS,
					     SILCGAIM_DEF_PKCS_LEN,
					     file_public_key, file_private_key, NULL,
					     (gc->account->password == NULL) ? "" : gc->account->password,
						 NULL, NULL, NULL, FALSE);
			g_stat(file_public_key, &st);
		} else {
			gaim_debug_error("silc", "Couldn't stat '%s' public key, error: %s\n",
							 file_public_key, strerror(errno));
			return FALSE;
		}
	}

#ifndef _WIN32
	/* Check the owner of the public key */
	if (st.st_uid != 0 && st.st_uid != pw->pw_uid) {
		gaim_debug_error("silc", "You don't seem to own your public key!?\n");
		return FALSE;
	}
#endif

	if ((g_stat(file_private_key, &st)) == -1) {
		/* If file doesn't exist */
		if (errno == ENOENT) {
			gaim_connection_update_progress(gc, _("Creating SILC key pair..."), 1, 5);
			silc_create_key_pair(SILCGAIM_DEF_PKCS,
					     SILCGAIM_DEF_PKCS_LEN,
					     file_public_key, file_private_key, NULL,
					     (gc->account->password == NULL) ? "" : gc->account->password,
						 NULL, NULL, NULL, FALSE);
			g_stat(file_private_key, &st);
		} else {
			gaim_debug_error("silc", "Couldn't stat '%s' private key, error: %s\n",
							 file_private_key, strerror(errno));
			return FALSE;
		}
	}

#ifndef _WIN32
	/* Check the owner of the private key */
	if (st.st_uid != 0 && st.st_uid != pw->pw_uid) {
		gaim_debug_error("silc", "You don't seem to own your private key!?\n");
		return FALSE;
	}

	/* Check the permissions for the private key */
	if ((st.st_mode & 0777) != 0600) {
		gaim_debug_warning("silc", "Wrong permissions in your private key file `%s'!\n"
			"Trying to change them ... ", file_private_key);
		if ((chmod(file_private_key, 0600)) == -1) {
			gaim_debug_error("silc",
				"Failed to change permissions for private key file!\n"
				"Permissions for your private key file must be 0600.\n");
			return FALSE;
		}
		gaim_debug_warning("silc", "Done.\n\n");
	}
#endif

	return TRUE;
}

#ifdef _WIN32
struct passwd *getpwuid(uid_t uid) {
	struct passwd *pwd = calloc(1, sizeof(struct passwd));
	return pwd;
}

uid_t getuid() {
	return 0;
}

uid_t geteuid() {
	return 0;
}
#endif

void silcgaim_show_public_key(SilcGaim sg,
			      const char *name, SilcPublicKey public_key,
			      GCallback callback, void *context)
{
	SilcPublicKeyIdentifier ident;
	SilcPKCS pkcs;
	char *fingerprint, *babbleprint;
	unsigned char *pk;
	SilcUInt32 pk_len, key_len = 0;
	GString *s;
	char *buf;

	ident = silc_pkcs_decode_identifier(public_key->identifier);
	if (!ident)
		return;

	pk = silc_pkcs_public_key_encode(public_key, &pk_len);
	fingerprint = silc_hash_fingerprint(NULL, pk, pk_len);
	babbleprint = silc_hash_babbleprint(NULL, pk, pk_len);

	if (silc_pkcs_alloc(public_key->name, &pkcs)) {
		key_len = silc_pkcs_public_key_set(pkcs, public_key);
		silc_pkcs_free(pkcs);
	}

	s = g_string_new("");
	if (ident->realname)
		/* Hint for translators: Please check the tabulator width here and in
		   the next strings (short strings: 2 tabs, longer strings 1 tab,
		   sum: 3 tabs or 24 characters) */
		g_string_append_printf(s, _("Real Name: \t%s\n"), ident->realname);
	if (ident->username)
		g_string_append_printf(s, _("User Name: \t%s\n"), ident->username);
	if (ident->email)
		g_string_append_printf(s, _("EMail: \t\t%s\n"), ident->email);
	if (ident->host)
		g_string_append_printf(s, _("Host Name: \t%s\n"), ident->host);
	if (ident->org)
		g_string_append_printf(s, _("Organization: \t%s\n"), ident->org);
	if (ident->country)
		g_string_append_printf(s, _("Country: \t%s\n"), ident->country);
	g_string_append_printf(s, _("Algorithm: \t%s\n"), public_key->name);
	g_string_append_printf(s, _("Key Length: \t%d bits\n"), (int)key_len);
	g_string_append_printf(s, "\n");
	g_string_append_printf(s, _("Public Key Fingerprint:\n%s\n\n"), fingerprint);
	g_string_append_printf(s, _("Public Key Babbleprint:\n%s"), babbleprint);

	buf = g_string_free(s, FALSE);

	gaim_request_action(NULL, _("Public Key Information"),
			    _("Public Key Information"),
			    buf, 0, context, 1,
			    _("Close"), callback);

	g_free(buf);
	silc_free(fingerprint);
	silc_free(babbleprint);
	silc_free(pk);
	silc_pkcs_free_identifier(ident);
}

SilcAttributePayload
silcgaim_get_attr(SilcDList attrs, SilcAttribute attribute)
{
	SilcAttributePayload attr = NULL;

	if (!attrs)
		return NULL;

	silc_dlist_start(attrs);
	while ((attr = silc_dlist_get(attrs)) != SILC_LIST_END)
		if (attribute == silc_attribute_get_attribute(attr))
			break;

	return attr;
}

void silcgaim_get_umode_string(SilcUInt32 mode, char *buf,
			       SilcUInt32 buf_size)
{
	memset(buf, 0, buf_size);
	if ((mode & SILC_UMODE_SERVER_OPERATOR) ||
	    (mode & SILC_UMODE_ROUTER_OPERATOR)) {
		strcat(buf, (mode & SILC_UMODE_SERVER_OPERATOR) ?
		       "[server operator] " :
		       (mode & SILC_UMODE_ROUTER_OPERATOR) ?
		       "[SILC operator] " : "[unknown mode] ");
	}
	if (mode & SILC_UMODE_GONE)
		strcat(buf, "[away] ");
	if (mode & SILC_UMODE_INDISPOSED)
		strcat(buf, "[indisposed] ");
	if (mode & SILC_UMODE_BUSY)
		strcat(buf, "[busy] ");
	if (mode & SILC_UMODE_PAGE)
		strcat(buf, "[wake me up] ");
	if (mode & SILC_UMODE_HYPER)
		strcat(buf, "[hyperactive] ");
	if (mode & SILC_UMODE_ROBOT)
		strcat(buf, "[robot] ");
	if (mode & SILC_UMODE_ANONYMOUS)
		strcat(buf, "[anonymous] ");
	if (mode & SILC_UMODE_BLOCK_PRIVMSG)
		strcat(buf, "[blocks private messages] ");
	if (mode & SILC_UMODE_DETACHED)
		strcat(buf, "[detached] ");
	if (mode & SILC_UMODE_REJECT_WATCHING)
		strcat(buf, "[rejects watching] ");
	if (mode & SILC_UMODE_BLOCK_INVITE)
		strcat(buf, "[blocks invites] ");
}

void silcgaim_get_chmode_string(SilcUInt32 mode, char *buf,
				SilcUInt32 buf_size)
{
	memset(buf, 0, buf_size);
	if (mode & SILC_CHANNEL_MODE_FOUNDER_AUTH)
		strcat(buf, "[permanent] ");
	if (mode & SILC_CHANNEL_MODE_PRIVATE)
		strcat(buf, "[private] ");
	if (mode & SILC_CHANNEL_MODE_SECRET)
		strcat(buf, "[secret] ");
	if (mode & SILC_CHANNEL_MODE_SECRET)
		strcat(buf, "[secret] ");
	if (mode & SILC_CHANNEL_MODE_PRIVKEY)
		strcat(buf, "[private key] ");
	if (mode & SILC_CHANNEL_MODE_INVITE)
		strcat(buf, "[invite only] ");
	if (mode & SILC_CHANNEL_MODE_TOPIC)
		strcat(buf, "[topic restricted] ");
	if (mode & SILC_CHANNEL_MODE_ULIMIT)
		strcat(buf, "[user count limit] ");
	if (mode & SILC_CHANNEL_MODE_PASSPHRASE)
		strcat(buf, "[passphrase auth] ");
	if (mode & SILC_CHANNEL_MODE_CHANNEL_AUTH)
		strcat(buf, "[public key auth] ");
	if (mode & SILC_CHANNEL_MODE_SILENCE_USERS)
		strcat(buf, "[users silenced] ");
	if (mode & SILC_CHANNEL_MODE_SILENCE_OPERS)
		strcat(buf, "[operators silenced] ");
}

void silcgaim_get_chumode_string(SilcUInt32 mode, char *buf,
				 SilcUInt32 buf_size)
{
	memset(buf, 0, buf_size);
	if (mode & SILC_CHANNEL_UMODE_CHANFO)
		strcat(buf, "[founder] ");
	if (mode & SILC_CHANNEL_UMODE_CHANOP)
		strcat(buf, "[operator] ");
	if (mode & SILC_CHANNEL_UMODE_BLOCK_MESSAGES)
		strcat(buf, "[blocks messages] ");
	if (mode & SILC_CHANNEL_UMODE_BLOCK_MESSAGES_USERS)
		strcat(buf, "[blocks user messages] ");
	if (mode & SILC_CHANNEL_UMODE_BLOCK_MESSAGES_ROBOTS)
		strcat(buf, "[blocks robot messages] ");
	if (mode & SILC_CHANNEL_UMODE_QUIET)
		strcat(buf, "[quieted] ");
}

void
silcgaim_parse_attrs(SilcDList attrs, char **moodstr, char **statusstr,
					 char **contactstr, char **langstr, char **devicestr,
					 char **tzstr, char **geostr)
{
	SilcAttributePayload attr;
	SilcAttributeMood mood = 0;
	SilcAttributeContact contact;
	SilcAttributeObjDevice device;
	SilcAttributeObjGeo geo;

	char tmp[1024];
	GString *s;

	*moodstr = NULL;
	*statusstr = NULL;
	*contactstr = NULL;
	*langstr = NULL;
	*devicestr = NULL;
	*tzstr = NULL;
	*geostr = NULL;

	if (!attrs)
		return;

	s = g_string_new("");
	attr = silcgaim_get_attr(attrs, SILC_ATTRIBUTE_STATUS_MOOD);
	if (attr && silc_attribute_get_object(attr, &mood, sizeof(mood))) {
		if (mood & SILC_ATTRIBUTE_MOOD_HAPPY)
			g_string_append_printf(s, "[%s] ", _("Happy"));
		if (mood & SILC_ATTRIBUTE_MOOD_SAD)
			g_string_append_printf(s, "[%s] ", _("Sad"));
		if (mood & SILC_ATTRIBUTE_MOOD_ANGRY)
			g_string_append_printf(s, "[%s] ", _("Angry"));
		if (mood & SILC_ATTRIBUTE_MOOD_JEALOUS)
			g_string_append_printf(s, "[%s] ", _("Jealous"));
		if (mood & SILC_ATTRIBUTE_MOOD_ASHAMED)
			g_string_append_printf(s, "[%s] ", _("Ashamed"));
		if (mood & SILC_ATTRIBUTE_MOOD_INVINCIBLE)
			g_string_append_printf(s, "[%s] ", _("Invincible"));
		if (mood & SILC_ATTRIBUTE_MOOD_INLOVE)
			g_string_append_printf(s, "[%s] ", _("In Love"));
		if (mood & SILC_ATTRIBUTE_MOOD_SLEEPY)
			g_string_append_printf(s, "[%s] ", _("Sleepy"));
		if (mood & SILC_ATTRIBUTE_MOOD_BORED)
			g_string_append_printf(s, "[%s] ", _("Bored"));
		if (mood & SILC_ATTRIBUTE_MOOD_EXCITED)
			g_string_append_printf(s, "[%s] ", _("Excited"));
		if (mood & SILC_ATTRIBUTE_MOOD_ANXIOUS)
			g_string_append_printf(s, "[%s] ", _("Anxious"));
	}
	if (strlen(s->str)) {
		*moodstr = s->str;
		g_string_free(s, FALSE);
	} else
		g_string_free(s, TRUE);

	attr = silcgaim_get_attr(attrs, SILC_ATTRIBUTE_STATUS_FREETEXT);
	memset(tmp, 0, sizeof(tmp));
	if (attr && silc_attribute_get_object(attr, tmp, sizeof(tmp)))
		*statusstr = g_strdup(tmp);

	s = g_string_new("");
	attr = silcgaim_get_attr(attrs, SILC_ATTRIBUTE_PREFERRED_CONTACT);
	if (attr && silc_attribute_get_object(attr, &contact, sizeof(contact))) {
		if (contact & SILC_ATTRIBUTE_CONTACT_CHAT)
			g_string_append_printf(s, "[%s] ", _("Chat"));
		if (contact & SILC_ATTRIBUTE_CONTACT_EMAIL)
			g_string_append_printf(s, "[%s] ", _("Email"));
		if (contact & SILC_ATTRIBUTE_CONTACT_CALL)
			g_string_append_printf(s, "[%s] ", _("Phone"));
		if (contact & SILC_ATTRIBUTE_CONTACT_PAGE)
			g_string_append_printf(s, "[%s] ", _("Paging"));
		if (contact & SILC_ATTRIBUTE_CONTACT_SMS)
			g_string_append_printf(s, "[%s] ", _("SMS"));
		if (contact & SILC_ATTRIBUTE_CONTACT_MMS)
			g_string_append_printf(s, "[%s] ", _("MMS"));
		if (contact & SILC_ATTRIBUTE_CONTACT_VIDEO)
			g_string_append_printf(s, "[%s] ", _("Video Conferencing"));
	}
	if (strlen(s->str)) {
		*contactstr = s->str;
		g_string_free(s, FALSE);
	} else
		g_string_free(s, TRUE);

	attr = silcgaim_get_attr(attrs, SILC_ATTRIBUTE_PREFERRED_LANGUAGE);
	memset(tmp, 0, sizeof(tmp));
	if (attr && silc_attribute_get_object(attr, tmp, sizeof(tmp)))
		*langstr = g_strdup(tmp);

	s = g_string_new("");
	attr = silcgaim_get_attr(attrs, SILC_ATTRIBUTE_DEVICE_INFO);
	memset(&device, 0, sizeof(device));
	if (attr && silc_attribute_get_object(attr, &device, sizeof(device))) {
		if (device.type == SILC_ATTRIBUTE_DEVICE_COMPUTER)
			g_string_append_printf(s, "%s: ", _("Computer"));
		if (device.type == SILC_ATTRIBUTE_DEVICE_MOBILE_PHONE)
			g_string_append_printf(s, "%s: ", _("Mobile Phone"));
		if (device.type == SILC_ATTRIBUTE_DEVICE_PDA)
			g_string_append_printf(s, "%s: ", _("PDA"));
		if (device.type == SILC_ATTRIBUTE_DEVICE_TERMINAL)
			g_string_append_printf(s, "%s: ", _("Terminal"));
		g_string_append_printf(s, "%s %s %s %s",
				device.manufacturer ? device.manufacturer : "",
				device.version ? device.version : "",
				device.model ? device.model : "",
				device.language ? device.language : "");
	}
	if (strlen(s->str)) {
		*devicestr = s->str;
		g_string_free(s, FALSE);
	} else
		g_string_free(s, TRUE);

	attr = silcgaim_get_attr(attrs, SILC_ATTRIBUTE_TIMEZONE);
	memset(tmp, 0, sizeof(tmp));
	if (attr && silc_attribute_get_object(attr, tmp, sizeof(tmp)))
		*tzstr = g_strdup(tmp);

	attr = silcgaim_get_attr(attrs, SILC_ATTRIBUTE_GEOLOCATION);
	memset(&geo, 0, sizeof(geo));
	if (attr && silc_attribute_get_object(attr, &geo, sizeof(geo)))
		*geostr = g_strdup_printf("%s %s %s (%s)",
				geo.longitude ? geo.longitude : "",
				geo.latitude ? geo.latitude : "",
				geo.altitude ? geo.altitude : "",
				geo.accuracy ? geo.accuracy : "");
}
