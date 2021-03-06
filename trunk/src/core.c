/**
 * @file core.c Gaim Core API
 * @ingroup core
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
#include "internal.h"
#include "connection.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "network.h"
#include "plugin.h"
#include "pounce.h"
#include "prefs.h"
#include "privacy.h"
#include "proxy.h"
#include "signals.h"
#include "sslconn.h"
#include "sound.h"

struct GaimCore
{
	char *ui;

	void *reserved;
};

static GaimCoreUiOps *_ops  = NULL;
static GaimCore      *_core = NULL;

STATIC_PROTO_INIT

gboolean
gaim_core_init(const char *ui)
{
	GaimCoreUiOps *ops;
	GaimCore *core;

	g_return_val_if_fail(ui != NULL, FALSE);
	g_return_val_if_fail(gaim_get_core() == NULL, FALSE);

	_core = core = g_new0(GaimCore, 1);
	core->ui = g_strdup(ui);
	core->reserved = NULL;

	ops = gaim_core_get_ui_ops();

	/* The signals subsystem is important and should be first. */
	gaim_signals_init();

	gaim_signal_register(core, "quitting", gaim_marshal_VOID, NULL, 0);

	/* Initialize all static protocols. */
	static_proto_init();

	gaim_prefs_init();

	if (ops != NULL) {
		if (ops->ui_prefs_init != NULL)
			ops->ui_prefs_init();

		if (ops->debug_ui_init != NULL)
			ops->debug_ui_init();
	}

	gaim_accounts_init();
	gaim_connections_init();
	gaim_conversations_init();
	gaim_blist_init();
	gaim_log_init();
	gaim_buddy_icons_init();
	gaim_network_init();
	gaim_privacy_init();
	gaim_pounces_init();
	gaim_proxy_init();
	gaim_sound_init();
	gaim_ssl_init();

	if (ops != NULL && ops->ui_init != NULL)
		ops->ui_init();

	return TRUE;
}

void
gaim_core_quit(void)
{
	GaimCoreUiOps *ops;
	GaimCore *core = gaim_get_core();
#ifndef GAIM_SMALL_SCREEN
	g_return_if_fail(core != NULL);
#else
    if(core != NULL){
#endif

	/* The self destruct sequence has been initiated */
	gaim_signal_emit(gaim_get_core(), "quitting");

	/* Transmission ends */
	gaim_connections_disconnect_all();

	/* Save .xml files, remove signals, etc. */
	gaim_ssl_uninit();
	gaim_pounces_uninit();
	gaim_blist_uninit();
	gaim_conversations_uninit();
	gaim_connections_uninit();
	gaim_buddy_icons_uninit();
	gaim_accounts_uninit();
	gaim_prefs_uninit();

	gaim_debug(GAIM_DEBUG_INFO, "main", "Unloading all plugins\n");
	gaim_plugins_destroy_all();

#ifdef GAIM_SMALL_SCREEN
    }
#endif
	ops = gaim_core_get_ui_ops();
	if (ops != NULL && ops->quit != NULL)
		ops->quit();

	if(core != NULL) gaim_signals_uninit();

	if (core != NULL && core->ui != NULL) {
		g_free(core->ui);
		core->ui = NULL;
	}

	if(core != NULL) g_free(core);

	_core = NULL;
}

gboolean
gaim_core_quit_cb(gpointer unused)
{
	gaim_core_quit();

	return FALSE;
}

const char *
gaim_core_get_version(void)
{
	return VERSION;
}

const char *
gaim_core_get_ui(void)
{
	GaimCore *core = gaim_get_core();

	g_return_val_if_fail(core != NULL, NULL);

	return core->ui;
}

GaimCore *
gaim_get_core(void)
{
	return _core;
}

void
gaim_core_set_ui_ops(GaimCoreUiOps *ops)
{
	_ops = ops;
}

GaimCoreUiOps *
gaim_core_get_ui_ops(void)
{
	return _ops;
}
