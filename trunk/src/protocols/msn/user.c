/**
 * @file user.c User functions
 *
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
 */
#include "msn.h"
#include "user.h"
#include "slp.h"

MsnUser *
msn_user_new(MsnUserList *userlist, const char *passport,
			 const char *store_name)
{
	MsnUser *user;

	user = g_new0(MsnUser, 1);

	user->userlist = userlist;

	msn_user_set_passport(user, passport);
	msn_user_set_store_name(user, store_name);

	/*
	 * XXX This seems to reset the friendly name from what it should be
	 *     to the passport when moving users. So, screw it :)
	 */
#if 0
	if (name != NULL)
		msn_user_set_name(user, name);
#endif

	return user;
}

void
msn_user_destroy(MsnUser *user)
{
	g_return_if_fail(user != NULL);

	if (user->clientcaps != NULL)
		g_hash_table_destroy(user->clientcaps);

	if (user->group_ids != NULL)
		g_list_free(user->group_ids);

	if (user->msnobj != NULL)
		msn_object_destroy(user->msnobj);

	if (user->passport != NULL)
		g_free(user->passport);

	if (user->friendly_name != NULL)
		g_free(user->friendly_name);

	if (user->store_name != NULL)
		g_free(user->store_name);

	if (user->phone.home != NULL)
		g_free(user->phone.home);

	if (user->phone.work != NULL)
		g_free(user->phone.work);

	if (user->phone.mobile != NULL)
		g_free(user->phone.mobile);

	g_free(user);
}

void
msn_user_update(MsnUser *user)
{
	GaimConnection *gc;

	gc = user->userlist->session->account->gc;

	if (user->online)
		serv_got_update(gc, user->passport, TRUE, 0, 0, user->idle, user->status);
	else
		serv_got_update(gc, user->passport, FALSE, 0, 0, 0, 0);
}

void
msn_user_set_state(MsnUser *user, const char *state)
{
	GaimConnection *gc;
	int status = 0;
	int idle = 0;
	GaimBuddy *b;

	gc = user->userlist->session->account->gc;

	if ((b = gaim_find_buddy(gc->account, user->passport)) != NULL)
		status |= ((((b->uc) >> 1) & 0xF0) << 1);

	if (!g_ascii_strcasecmp(state, "BSY"))
		status |= UC_UNAVAILABLE | (MSN_BUSY << 1);
	else if (!g_ascii_strcasecmp(state, "IDL"))
	{
		status |= UC_UNAVAILABLE | (MSN_IDLE << 1);
		idle = -1;
	}
	else if (!g_ascii_strcasecmp(state, "BRB"))
		status |= UC_UNAVAILABLE | (MSN_BRB << 1);
	else if (!g_ascii_strcasecmp(state, "AWY"))
		status |= UC_UNAVAILABLE | (MSN_AWAY << 1);
	else if (!g_ascii_strcasecmp(state, "PHN"))
		status |= UC_UNAVAILABLE | (MSN_PHONE << 1);
	else if (!g_ascii_strcasecmp(state, "LUN"))
		status |= UC_UNAVAILABLE | (MSN_LUNCH << 1);

	user->status = status;
	user->idle = idle;
}

void
msn_user_set_passport(MsnUser *user, const char *passport)
{
	g_return_if_fail(user != NULL);

	if (user->passport != NULL)
		g_free(user->passport);

	user->passport = g_strdup(passport);
}

void
msn_user_set_friendly_name(MsnUser *user, const char *name)
{
	g_return_if_fail(user != NULL);

	if (user->friendly_name != NULL)
		g_free(user->friendly_name);

	user->friendly_name = g_strdup(name);
}

void
msn_user_set_store_name(MsnUser *user, const char *name)
{
	g_return_if_fail(user != NULL);

	if (user->store_name != NULL)
		g_free(user->store_name);

	user->store_name = g_strdup(name);
}

void
msn_user_set_buddy_icon(MsnUser *user, const char *filename)
{
	struct stat st;
	FILE *fp;
	MsnObject *msnobj = msn_user_get_object(user);

	g_return_if_fail(user != NULL);

	if (filename == NULL || g_stat(filename, &st) == -1)
	{
		msn_user_set_object(user, NULL);
	}
	else if ((fp = g_fopen(filename, "rb")) != NULL)
	{
		unsigned char *buf;
		SHA_CTX ctx;
		gsize len;
		char *base64;
		unsigned char digest[20];

		if (msnobj == NULL)
		{
			msnobj = msn_object_new(TRUE);
			msn_object_set_local(msnobj);
			msn_object_set_type(msnobj, MSN_OBJECT_USERTILE);
			msn_object_set_location(msnobj, "TFR2C2.tmp");
			msn_object_set_creator(msnobj, msn_user_get_passport(user));

			msn_user_set_object(user, msnobj);
		}

		msn_object_set_real_location(msnobj, filename);

		buf = g_malloc(st.st_size);
		len = fread(buf, 1, st.st_size, fp);

		fclose(fp);

		/* Compute the SHA1D field. */
		memset(digest, 0, sizeof(digest));

		shaInit(&ctx);
		shaUpdate(&ctx, buf, st.st_size);
		shaFinal(&ctx, digest);
		g_free(buf);

		base64 = gaim_base64_encode(digest, sizeof(digest));
		msn_object_set_sha1d(msnobj, base64);
		g_free(base64);

		msn_object_set_size(msnobj, st.st_size);

		/* Compute the SHA1C field. */
		buf = g_strdup_printf(
			"Creator%sSize%dType%dLocation%sFriendly%sSHA1D%s",
			msn_object_get_creator(msnobj),
			msn_object_get_size(msnobj),
			msn_object_get_type(msnobj),
			msn_object_get_location(msnobj),
			msn_object_get_friendly(msnobj),
			msn_object_get_sha1d(msnobj));

		memset(digest, 0, sizeof(digest));

		shaInit(&ctx);
		shaUpdate(&ctx, buf, strlen(buf));
		shaFinal(&ctx, digest);
		g_free(buf);

		base64 = gaim_base64_encode(digest, sizeof(digest));
		msn_object_set_sha1c(msnobj, base64);
		g_free(base64);
	}
	else
	{
		gaim_debug_error("msn", "Unable to open buddy icon %s!\n", filename);
		msn_user_set_object(user, NULL);
	}
}

void
msn_user_add_group_id(MsnUser *user, int id)
{
	MsnUserList *userlist;
	GaimAccount *account;
	GaimBuddy *b;
	GaimGroup *g;
	const char *passport;
	const char *group_name;

	g_return_if_fail(user != NULL);
	g_return_if_fail(id >= 0);

	user->group_ids = g_list_append(user->group_ids, GINT_TO_POINTER(id));

	userlist = user->userlist;
	account = userlist->session->account;
	passport = msn_user_get_passport(user);

	group_name = msn_userlist_find_group_name(userlist, id);

	g = gaim_find_group(group_name);

	if ((id == 0) && (g == NULL))
	{
		g = gaim_group_new(group_name);
		gaim_blist_add_group(g, NULL);
	}

	b = gaim_find_buddy_in_group(account, passport, g);

	if (b == NULL)
	{
		b = gaim_buddy_new(account, passport, NULL);

		gaim_blist_add_buddy(b, NULL, g, NULL);
	}

	b->proto_data = user;
}

void
msn_user_remove_group_id(MsnUser *user, int id)
{
	g_return_if_fail(user != NULL);
	g_return_if_fail(id >= 0);

	user->group_ids = g_list_remove(user->group_ids, GINT_TO_POINTER(id));
}

void
msn_user_set_home_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.home != NULL)
		g_free(user->phone.home);

	user->phone.home = (number == NULL ? NULL : g_strdup(number));
}

void
msn_user_set_work_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.work != NULL)
		g_free(user->phone.work);

	user->phone.work = (number == NULL ? NULL : g_strdup(number));
}

void
msn_user_set_mobile_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.mobile != NULL)
		g_free(user->phone.mobile);

	user->phone.mobile = (number == NULL ? NULL : g_strdup(number));
}

void
msn_user_set_object(MsnUser *user, MsnObject *obj)
{
	g_return_if_fail(user != NULL);

	if (user->msnobj != NULL)
		msn_object_destroy(user->msnobj);

	user->msnobj = obj;

	if (user->list_op & MSN_LIST_FL_OP)
		msn_queue_buddy_icon_request(user);
}

void
msn_user_set_client_caps(MsnUser *user, GHashTable *info)
{
	g_return_if_fail(user != NULL);
	g_return_if_fail(info != NULL);

	if (user->clientcaps != NULL)
		g_hash_table_destroy(user->clientcaps);

	user->clientcaps = info;
}

const char *
msn_user_get_passport(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->passport;
}

const char *
msn_user_get_friendly_name(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->friendly_name;
}

const char *
msn_user_get_store_name(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->store_name;
}

const char *
msn_user_get_home_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.home;
}

const char *
msn_user_get_work_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.work;
}

const char *
msn_user_get_mobile_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.mobile;
}

MsnObject *
msn_user_get_object(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->msnobj;
}

GHashTable *
msn_user_get_client_caps(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->clientcaps;
}
