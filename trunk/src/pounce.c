/**
 * @file pounce.c Buddy Pounce API
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
#include "conversation.h"
#include "debug.h"
#include "pounce.h"

#include "debug.h"
#include "pounce.h"
#include "util.h"

typedef struct
{
	GString *buffer;

	GaimPounce *pounce;
	GaimPounceEvent events;

	char *ui_name;
	char *pouncee;
	char *protocol_id;
	char *event_type;
	char *action_name;
	char *param_name;
	char *account_name;

} PounceParserData;

typedef struct
{
	char *name;

	gboolean enabled;

	GHashTable *atts;

} GaimPounceActionData;

typedef struct
{
	char *ui;
	GaimPounceCb cb;
	void (*new_pounce)(GaimPounce *);
	void (*free_pounce)(GaimPounce *);

} GaimPounceHandler;


static GHashTable *pounce_handlers = NULL;
static GList      *pounces = NULL;
static guint       pounces_save_timer = 0;
static gboolean    pounces_loaded = FALSE;


static GaimPounceActionData *
find_action_data(const GaimPounce *pounce, const char *name)
{
	GaimPounceActionData *action;

	g_return_val_if_fail(pounce != NULL, NULL);
	g_return_val_if_fail(name   != NULL, NULL);

	action = g_hash_table_lookup(pounce->actions, name);

	return action;
}

static void
free_action_data(gpointer data)
{
	GaimPounceActionData *action_data = data;

	g_free(action_data->name);

	g_hash_table_destroy(action_data->atts);

	g_free(action_data);
}

static gboolean
pounces_save_cb(gpointer unused)
{
	gaim_pounces_sync();
	pounces_save_timer = 0;

	return FALSE;
}

static void
schedule_pounces_save(void)
{
	if (!pounces_save_timer)
		pounces_save_timer = gaim_timeout_add(5000, pounces_save_cb, NULL);
}

GaimPounce *
gaim_pounce_new(const char *ui_type, GaimAccount *pouncer,
				const char *pouncee, GaimPounceEvent event)
{
	GaimPounce *pounce;
	GaimPounceHandler *handler;

	g_return_val_if_fail(ui_type != NULL, NULL);
	g_return_val_if_fail(pouncer != NULL, NULL);
	g_return_val_if_fail(pouncee != NULL, NULL);
	g_return_val_if_fail(event   != 0,    NULL);

	pounce = g_new0(GaimPounce, 1);

	pounce->ui_type  = g_strdup(ui_type);
	pounce->pouncer  = pouncer;
	pounce->pouncee  = g_strdup(pouncee);
	pounce->events   = event;

	pounce->actions  = g_hash_table_new_full(g_str_hash, g_str_equal,
											 g_free, free_action_data);

	handler = g_hash_table_lookup(pounce_handlers, pounce->ui_type);

	if (handler != NULL && handler->new_pounce != NULL)
		handler->new_pounce(pounce);

	pounces = g_list_append(pounces, pounce);

	return pounce;
}

void
gaim_pounce_destroy(GaimPounce *pounce)
{
	GaimPounceHandler *handler;

	g_return_if_fail(pounce != NULL);

	handler = g_hash_table_lookup(pounce_handlers, pounce->ui_type);

	pounces = g_list_remove(pounces, pounce);

	if (pounce->ui_type != NULL) g_free(pounce->ui_type);
	if (pounce->pouncee != NULL) g_free(pounce->pouncee);

	g_hash_table_destroy(pounce->actions);

	if (handler != NULL && handler->free_pounce != NULL)
		handler->free_pounce(pounce);

	g_free(pounce);

	schedule_pounces_save();
}

void
gaim_pounce_destroy_all_by_account(GaimAccount *account)
{
	GaimAccount *pouncer;
	GaimPounce *pounce;
	GList *l, *l_next;

	
	g_return_if_fail(account != NULL);
	
	for (l = gaim_pounces_get_all(); l != NULL; l = l_next) {
		pounce = (GaimPounce *)l->data;
		l_next = l->next;
		
		pouncer = gaim_pounce_get_pouncer(pounce);
		if (pouncer == account)
			gaim_pounce_destroy(pounce);
	}
}

void
gaim_pounce_set_events(GaimPounce *pounce, GaimPounceEvent events)
{
	g_return_if_fail(pounce != NULL);
	g_return_if_fail(pounce != GAIM_POUNCE_NONE);

	pounce->events = events;

	schedule_pounces_save();
}

void
gaim_pounce_set_pouncer(GaimPounce *pounce, GaimAccount *pouncer)
{
	g_return_if_fail(pounce  != NULL);
	g_return_if_fail(pouncer != NULL);

	pounce->pouncer = pouncer;

	schedule_pounces_save();
}

void
gaim_pounce_set_pouncee(GaimPounce *pounce, const char *pouncee)
{
	g_return_if_fail(pounce  != NULL);
	g_return_if_fail(pouncee != NULL);

	if (pounce->pouncee != NULL)
		g_free(pounce->pouncee);

	pounce->pouncee = (pouncee == NULL ? NULL : g_strdup(pouncee));

	schedule_pounces_save();
}

void
gaim_pounce_set_save(GaimPounce *pounce, gboolean save)
{
	g_return_if_fail(pounce != NULL);

	pounce->save = save;

	schedule_pounces_save();
}

void
gaim_pounce_action_register(GaimPounce *pounce, const char *name)
{
	GaimPounceActionData *action_data;

	g_return_if_fail(pounce != NULL);
	g_return_if_fail(name   != NULL);

	if (g_hash_table_lookup(pounce->actions, name) != NULL)
		return;

	action_data = g_new0(GaimPounceActionData, 1);

	action_data->name    = g_strdup(name);
	action_data->enabled = FALSE;
	action_data->atts    = g_hash_table_new_full(g_str_hash, g_str_equal,
												 g_free, g_free);

	g_hash_table_insert(pounce->actions, g_strdup(name), action_data);

	schedule_pounces_save();
}

void
gaim_pounce_action_set_enabled(GaimPounce *pounce, const char *action,
							   gboolean enabled)
{
	GaimPounceActionData *action_data;

	g_return_if_fail(pounce != NULL);
	g_return_if_fail(action != NULL);

	action_data = find_action_data(pounce, action);

	g_return_if_fail(action_data != NULL);

	action_data->enabled = enabled;

	schedule_pounces_save();
}

void
gaim_pounce_action_set_attribute(GaimPounce *pounce, const char *action,
								 const char *attr, const char *value)
{
	GaimPounceActionData *action_data;

	g_return_if_fail(pounce != NULL);
	g_return_if_fail(action != NULL);
	g_return_if_fail(attr   != NULL);

	action_data = find_action_data(pounce, action);

	g_return_if_fail(action_data != NULL);

	if (value == NULL)
		g_hash_table_remove(action_data->atts, attr);
	else
		g_hash_table_insert(action_data->atts, g_strdup(attr),
							g_strdup(value));

	schedule_pounces_save();
}

void
gaim_pounce_set_data(GaimPounce *pounce, void *data)
{
	g_return_if_fail(pounce != NULL);

	pounce->data = data;
}

GaimPounceEvent
gaim_pounce_get_events(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, GAIM_POUNCE_NONE);

	return pounce->events;
}

GaimAccount *
gaim_pounce_get_pouncer(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->pouncer;
}

const char *
gaim_pounce_get_pouncee(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->pouncee;
}

gboolean
gaim_pounce_get_save(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, FALSE);

	return pounce->save;
}

gboolean
gaim_pounce_action_is_enabled(const GaimPounce *pounce, const char *action)
{
	GaimPounceActionData *action_data;

	g_return_val_if_fail(pounce != NULL, FALSE);
	g_return_val_if_fail(action != NULL, FALSE);

	action_data = find_action_data(pounce, action);

	g_return_val_if_fail(action_data != NULL, FALSE);

	return action_data->enabled;
}

const char *
gaim_pounce_action_get_attribute(const GaimPounce *pounce,
								 const char *action, const char *attr)
{
	GaimPounceActionData *action_data;

	g_return_val_if_fail(pounce != NULL, NULL);
	g_return_val_if_fail(action != NULL, NULL);
	g_return_val_if_fail(attr   != NULL, NULL);

	action_data = find_action_data(pounce, action);

	g_return_val_if_fail(action_data != NULL, NULL);

	return g_hash_table_lookup(action_data->atts, attr);
}

void *
gaim_pounce_get_data(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->data;
}

void
gaim_pounce_execute(const GaimAccount *pouncer, const char *pouncee,
					GaimPounceEvent events)
{
	GaimPounce *pounce;
	GaimPounceHandler *handler;
	GList *l, *l_next;
	char *norm_pouncee;

	g_return_if_fail(pouncer != NULL);
	g_return_if_fail(pouncee != NULL);
	g_return_if_fail(events  != GAIM_POUNCE_NONE);

	norm_pouncee = g_strdup(gaim_normalize(pouncer, pouncee));

	for (l = gaim_pounces_get_all(); l != NULL; l = l_next)
	{
		pounce = (GaimPounce *)l->data;
		l_next = l->next;

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!gaim_utf8_strcasecmp(gaim_normalize(pouncer, gaim_pounce_get_pouncee(pounce)),
								  norm_pouncee))
		{
			handler = g_hash_table_lookup(pounce_handlers, pounce->ui_type);

			if (handler != NULL && handler->cb != NULL)
			{
				handler->cb(pounce, events, gaim_pounce_get_data(pounce));

				if (!gaim_pounce_get_save(pounce))
					gaim_pounce_destroy(pounce);
			}
		}
	}

	g_free(norm_pouncee);
}

GaimPounce *
gaim_find_pounce(const GaimAccount *pouncer, const char *pouncee,
				 GaimPounceEvent events)
{
	GaimPounce *pounce = NULL;
	GList *l;
	char *norm_pouncee;

	g_return_val_if_fail(pouncer != NULL, NULL);
	g_return_val_if_fail(pouncee != NULL, NULL);
	g_return_val_if_fail(events  != GAIM_POUNCE_NONE, NULL);

	norm_pouncee = g_strdup(gaim_normalize(pouncer, pouncee));

	for (l = gaim_pounces_get_all(); l != NULL; l = l->next)
	{
		pounce = (GaimPounce *)l->data;

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!gaim_utf8_strcasecmp(gaim_normalize(pouncer, gaim_pounce_get_pouncee(pounce)),
								  norm_pouncee))
		{
			break;
		}

		pounce = NULL;
	}

	g_free(norm_pouncee);

	return pounce;
}

/* XML Stuff */
static void
free_parser_data(gpointer user_data)
{
	PounceParserData *data = user_data;

	if (data->buffer != NULL)
		g_string_free(data->buffer, TRUE);

	if (data->ui_name      != NULL) g_free(data->ui_name);
	if (data->pouncee      != NULL) g_free(data->pouncee);
	if (data->protocol_id  != NULL) g_free(data->protocol_id);
	if (data->event_type   != NULL) g_free(data->event_type);
	if (data->action_name  != NULL) g_free(data->action_name);
	if (data->param_name   != NULL) g_free(data->param_name);
	if (data->account_name != NULL) g_free(data->account_name);

	g_free(data);
}

static void
start_element_handler(GMarkupParseContext *context,
					  const gchar *element_name,
					  const gchar **attribute_names,
					  const gchar **attribute_values,
					  gpointer user_data, GError **error)
{
	PounceParserData *data = user_data;
	GHashTable *atts;
	int i;

	atts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	for (i = 0; attribute_names[i] != NULL; i++) {
		g_hash_table_insert(atts, g_strdup(attribute_names[i]),
							g_strdup(attribute_values[i]));
	}

	if (data->buffer != NULL) {
		g_string_free(data->buffer, TRUE);
		data->buffer = NULL;
	}

	if (!strcmp(element_name, "pounce")) {
		const char *ui = g_hash_table_lookup(atts, "ui");

		if (ui == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'ui' parameter for pounce!\n");
		}
		else
			data->ui_name = g_strdup(ui);

		data->events = 0;
	}
	else if (!strcmp(element_name, "account")) {
		const char *protocol_id = g_hash_table_lookup(atts, "protocol");

		if (protocol_id == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'protocol' parameter for account!\n");
		}
		else
			data->protocol_id = g_strdup(protocol_id);
	}
	else if (!strcmp(element_name, "event")) {
		const char *type = g_hash_table_lookup(atts, "type");

		if (type == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'type' parameter for event!\n");
		}
		else
			data->event_type = g_strdup(type);
	}
	else if (!strcmp(element_name, "action")) {
		const char *type = g_hash_table_lookup(atts, "type");

		if (type == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'type' parameter for action!\n");
		}
		else
			data->action_name = g_strdup(type);
	}
	else if (!strcmp(element_name, "param")) {
		const char *param_name = g_hash_table_lookup(atts, "name");

		if (param_name == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Unset 'name' parameter for param!\n");
		}
		else
			data->param_name = g_strdup(param_name);
	}

	g_hash_table_destroy(atts);
}

static void
end_element_handler(GMarkupParseContext *context, const gchar *element_name,
					gpointer user_data,  GError **error)
{
	PounceParserData *data = user_data;
	gchar *buffer = NULL;

	if (data->buffer != NULL) {
		buffer = g_string_free(data->buffer, FALSE);
		data->buffer = NULL;
	}

	if (!strcmp(element_name, "account")) {
		g_free(data->account_name);
		data->account_name = g_strdup(buffer);
	}
	else if (!strcmp(element_name, "pouncee")) {
		g_free(data->pouncee);
		data->pouncee = g_strdup(buffer);
	}
	else if (!strcmp(element_name, "event")) {
		if (!strcmp(data->event_type, "sign-on"))
			data->events |= GAIM_POUNCE_SIGNON;
		else if (!strcmp(data->event_type, "sign-off"))
			data->events |= GAIM_POUNCE_SIGNOFF;
		else if (!strcmp(data->event_type, "away"))
			data->events |= GAIM_POUNCE_AWAY;
		else if (!strcmp(data->event_type, "return-from-away"))
			data->events |= GAIM_POUNCE_AWAY_RETURN;
		else if (!strcmp(data->event_type, "idle"))
			data->events |= GAIM_POUNCE_IDLE;
		else if (!strcmp(data->event_type, "return-from-idle"))
			data->events |= GAIM_POUNCE_IDLE_RETURN;
		else if (!strcmp(data->event_type, "start-typing"))
			data->events |= GAIM_POUNCE_TYPING;
		else if (!strcmp(data->event_type, "stop-typing"))
			data->events |= GAIM_POUNCE_TYPING_STOPPED;

		g_free(data->event_type);
		data->event_type = NULL;
	}
	else if (!strcmp(element_name, "action")) {
		if (data->pounce != NULL) {
			gaim_pounce_action_register(data->pounce, data->action_name);
			gaim_pounce_action_set_enabled(data->pounce, data->action_name, TRUE);
		}

		g_free(data->action_name);
		data->action_name = NULL;
	}
	else if (!strcmp(element_name, "param")) {
		if (data->pounce != NULL) {
			gaim_pounce_action_set_attribute(data->pounce, data->action_name,
											 data->param_name, buffer);
		}

		g_free(data->param_name);
		data->param_name = NULL;
	}
	else if (!strcmp(element_name, "events")) {
		GaimAccount *account;

		account = gaim_accounts_find(data->account_name, data->protocol_id);

		g_free(data->account_name);
		g_free(data->protocol_id);

		data->account_name = NULL;
		data->protocol_id  = NULL;

		if (account == NULL) {
			gaim_debug(GAIM_DEBUG_ERROR, "pounce",
					   "Account for pounce not found!\n");
		}
		else {
			gaim_debug(GAIM_DEBUG_INFO, "pounce",
					   "Creating pounce: %s, %s\n", data->ui_name,
					   data->pouncee);

			data->pounce = gaim_pounce_new(data->ui_name, account,
										   data->pouncee, data->events);
		}

		g_free(data->pouncee);
		data->pouncee = NULL;
	}
	else if (!strcmp(element_name, "save")) {
		gaim_pounce_set_save(data->pounce, TRUE);
	}
	else if (!strcmp(element_name, "pounce")) {
		data->pounce = NULL;
		data->events = 0;

		if (data->ui_name      != NULL) g_free(data->ui_name);
		if (data->pouncee      != NULL) g_free(data->pouncee);
		if (data->protocol_id  != NULL) g_free(data->protocol_id);
		if (data->event_type   != NULL) g_free(data->event_type);
		if (data->action_name  != NULL) g_free(data->action_name);
		if (data->param_name   != NULL) g_free(data->param_name);
		if (data->account_name != NULL) g_free(data->account_name);

		data->ui_name      = NULL;
		data->pounce       = NULL;
		data->protocol_id  = NULL;
		data->event_type   = NULL;
		data->action_name  = NULL;
		data->param_name   = NULL;
		data->account_name = NULL;
	}

	if (buffer != NULL)
		g_free(buffer);
}

static void
text_handler(GMarkupParseContext *context, const gchar *text,
			 gsize text_len, gpointer user_data, GError **error)
{
	PounceParserData *data = user_data;

	if (data->buffer == NULL)
		data->buffer = g_string_new_len(text, text_len);
	else
		g_string_append_len(data->buffer, text, text_len);
}

static GMarkupParser pounces_parser =
{
	start_element_handler,
	end_element_handler,
	text_handler,
	NULL,
	NULL
};

gboolean
gaim_pounces_load(void)
{
	gchar *filename = g_build_filename(gaim_user_dir(), "pounces.xml", NULL);
	gchar *contents = NULL;
	gsize length;
	GMarkupParseContext *context;
	GError *error = NULL;
	PounceParserData *parser_data;

	if (filename == NULL) {
		pounces_loaded = TRUE;
		return FALSE;
	}

	if (!g_file_get_contents(filename, &contents, &length, &error)) {
		gaim_debug(GAIM_DEBUG_ERROR, "pounces",
				   "Error reading pounces: %s\n", error->message);

		g_free(filename);
		g_error_free(error);

		pounces_loaded = TRUE;
		return FALSE;
	}

	parser_data = g_new0(PounceParserData, 1);

	context = g_markup_parse_context_new(&pounces_parser, 0,
										 parser_data, free_parser_data);

	if (!g_markup_parse_context_parse(context, contents, length, NULL)) {
		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);

		pounces_loaded = TRUE;

		return FALSE;
	}

	if (!g_markup_parse_context_end_parse(context, NULL)) {
		gaim_debug(GAIM_DEBUG_ERROR, "pounces", "Error parsing %s\n",
				   filename);

		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);
		pounces_loaded = TRUE;

		return FALSE;
	}

	g_markup_parse_context_free(context);
	g_free(contents);
	g_free(filename);

	pounces_loaded = TRUE;

	return TRUE;
}

static void
write_action_parameter(gpointer key, gpointer value, gpointer user_data)
{
	const char *name, *param_value;
	char *escaped;
	FILE *fp;

	param_value = (const char *)value;
	name        = (const char *)key;
	fp          = (FILE *)user_data;

	escaped = g_markup_escape_text(param_value, -1);
	fprintf(fp, "    <param name='%s'>%s</param>\n", name, escaped);
	g_free(escaped);
}

static void
write_action_parameter_list(gpointer key, gpointer value, gpointer user_data)
{
	GaimPounceActionData *action_data;
	const char *action;
	FILE *fp;

	action_data = (GaimPounceActionData *)value;
	action      = (const char *)key;
	fp          = (FILE *)user_data;

	if (!action_data->enabled)
		return;

	fprintf(fp, "   <action type='%s'", action);

	if (g_hash_table_size(action_data->atts) == 0) {
		fprintf(fp, "/>\n");
	}
	else {
		fprintf(fp, ">\n");

		g_hash_table_foreach(action_data->atts, write_action_parameter, fp);

		fprintf(fp, "   </action>\n");
	}
}

static void
gaim_pounces_write(FILE *fp, GaimPounce *pounce)
{
	GaimAccount *pouncer;
	GaimPounceEvent events;
	char *pouncer_name;

	pouncer = gaim_pounce_get_pouncer(pounce);
	events  = gaim_pounce_get_events(pounce);

	pouncer_name = g_markup_escape_text(gaim_account_get_username(pouncer), -1);

	fprintf(fp, " <pounce ui='%s'>\n", pounce->ui_type);
	fprintf(fp, "  <account protocol='%s'>%s</account>\n",
			pouncer->protocol_id, pouncer_name);
	fprintf(fp, "  <pouncee>%s</pouncee>\n", gaim_pounce_get_pouncee(pounce));
	fprintf(fp, "  <events>\n");

	if (events & GAIM_POUNCE_SIGNON)
		fprintf(fp, "   <event type='sign-on'/>\n");
	if (events & GAIM_POUNCE_SIGNOFF)
		fprintf(fp, "   <event type='sign-off'/>\n");
	if (events & GAIM_POUNCE_AWAY)
		fprintf(fp, "   <event type='away'/>\n");
	if (events & GAIM_POUNCE_AWAY_RETURN)
		fprintf(fp, "   <event type='return-from-away'/>\n");
	if (events & GAIM_POUNCE_IDLE)
		fprintf(fp, "   <event type='idle'/>\n");
	if (events & GAIM_POUNCE_IDLE_RETURN)
		fprintf(fp, "   <event type='return-from-idle'/>\n");
	if (events & GAIM_POUNCE_TYPING)
		fprintf(fp, "   <event type='start-typing'/>\n");
	if (events & GAIM_POUNCE_TYPING_STOPPED)
		fprintf(fp, "   <event type='stop-typing'/>\n");

	fprintf(fp, "  </events>\n");
	fprintf(fp, "  <actions>\n");

	g_hash_table_foreach(pounce->actions, write_action_parameter_list, fp);

	fprintf(fp, "  </actions>\n");

	if (gaim_pounce_get_save(pounce))
		fprintf(fp, "  <save/>\n");

	fprintf(fp, " </pounce>\n");

	g_free(pouncer_name);
}

void
gaim_pounces_sync(void)
{
	FILE *fp;
	struct stat st;
	const char *user_dir = gaim_user_dir();
	char *filename;
	char *filename_real;

	if (!pounces_loaded) {
		gaim_debug(GAIM_DEBUG_WARNING, "pounces",
				   "Writing pounces to disk.\n");
		schedule_pounces_save();
		return;
	}

	if (user_dir == NULL)
		return;

	gaim_debug(GAIM_DEBUG_INFO, "pounces", "Writing pounces to disk.\n");

	fp = g_fopen(user_dir, "r");

	if (fp == NULL)
		g_mkdir(user_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	else
		fclose(fp);

	filename = g_build_filename(user_dir, "pounces.xml.save", NULL);

	if ((fp = g_fopen(filename, "w")) != NULL) {
		GList *l;

		fprintf(fp, "<?xml version='1.0' encoding='UTF-8' ?>\n\n");
		fprintf(fp, "<pounces version='1.0'>\n");

		for (l = gaim_pounces_get_all(); l != NULL; l = l->next)
			gaim_pounces_write(fp, l->data);

		fprintf(fp, "</pounces>\n");

		fclose(fp);
		chmod(filename, S_IRUSR | S_IWUSR);
	}
	else {
		gaim_debug(GAIM_DEBUG_ERROR, "pounces", "Unable to write %s\n",
				   filename);
		g_free(filename);
		return;
	}

	if (g_stat(filename, &st) || (st.st_size == 0)) {
		gaim_debug_error("pounces", "Failed to save pounces\n");
		g_unlink(filename);
		g_free(filename);
		return;
	}

	filename_real = g_build_filename(user_dir, "pounces.xml", NULL);

	if (g_rename(filename, filename_real) < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "pounces", "Error renaming %s to %s\n",
				   filename, filename_real);
	}

	g_free(filename);
	g_free(filename_real);
}

void
gaim_pounces_register_handler(const char *ui, GaimPounceCb cb,
							  void (*new_pounce)(GaimPounce *pounce),
							  void (*free_pounce)(GaimPounce *pounce))
{
	GaimPounceHandler *handler;

	g_return_if_fail(ui != NULL);
	g_return_if_fail(cb != NULL);

	handler = g_new0(GaimPounceHandler, 1);

	handler->ui          = g_strdup(ui);
	handler->cb          = cb;
	handler->new_pounce  = new_pounce;
	handler->free_pounce = free_pounce;

	g_hash_table_insert(pounce_handlers, g_strdup(ui), handler);
}

void
gaim_pounces_unregister_handler(const char *ui)
{
	g_return_if_fail(ui != NULL);

	g_hash_table_remove(pounce_handlers, ui);
}

GList *
gaim_pounces_get_all(void)
{
	return pounces;
}

static void
free_pounce_handler(gpointer user_data)
{
	GaimPounceHandler *handler = (GaimPounceHandler *)user_data;

	g_free(handler->ui);
	g_free(handler);
}

static void
buddy_state_cb(GaimBuddy *buddy, GaimPounceEvent event)
{
	gaim_pounce_execute(buddy->account, buddy->name, event);
}

static void
buddy_typing_cb(GaimConversation *conv, void *data)
{
	GaimAccount *account = gaim_conversation_get_account(conv);
	const char *name     = gaim_conversation_get_name(conv);

	if (gaim_find_buddy(account, name) != NULL)
	{
		GaimPounceEvent event;

		event = (gaim_conv_im_get_typing_state(GAIM_CONV_IM(conv)) == GAIM_TYPING
				 ? GAIM_POUNCE_TYPING : GAIM_POUNCE_TYPING_STOPPED);

		gaim_pounce_execute(account, name, event);
	}
}

void *
gaim_pounces_get_handle(void)
{
	static int pounce_handle;

	return &pounce_handle;
}

void
gaim_pounces_init(void)
{
	void *blist_handle = gaim_blist_get_handle();
	void *conv_handle  = gaim_conversations_get_handle();
	void *handle       = gaim_pounces_get_handle();

	pounce_handlers = g_hash_table_new_full(g_str_hash, g_str_equal,
											g_free, free_pounce_handler);

	gaim_signal_connect(blist_handle, "buddy-idle",
						handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_POUNCE_IDLE));
	gaim_signal_connect(blist_handle, "buddy-unidle",
						handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_POUNCE_IDLE_RETURN));
	gaim_signal_connect(blist_handle, "buddy-away",
						handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_POUNCE_AWAY));
	gaim_signal_connect(blist_handle, "buddy-back",
						handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_POUNCE_AWAY_RETURN));
	gaim_signal_connect(blist_handle, "buddy-signed-on",
						handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_POUNCE_SIGNON));
	gaim_signal_connect(blist_handle, "buddy-signed-off",
						handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_POUNCE_SIGNOFF));

	gaim_signal_connect(conv_handle, "buddy-typing",
						handle, GAIM_CALLBACK(buddy_typing_cb), NULL);
	gaim_signal_connect(conv_handle, "buddy-typing-stopped",
						handle, GAIM_CALLBACK(buddy_typing_cb), NULL);
}

void
gaim_pounces_uninit()
{
	if (pounces_save_timer != 0) {
		gaim_timeout_remove(pounces_save_timer);
		pounces_save_timer = 0;
		gaim_pounces_sync();
	}

	gaim_signals_disconnect_by_handle(gaim_pounces_get_handle());
}
