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

#define PHOTO_SUPPORT 1

#include "internal.h"
#include "debug.h"
#include "notify.h"
#include "util.h"
#if PHOTO_SUPPORT
#include "imgstore.h"
#endif

#include "yahoo.h"
#include "yahoo_friend.h"


typedef struct {
	GaimConnection *gc;
	char *name;
} YahooGetInfoData;


typedef enum profile_lang_id {
	XX, DA, DE, EL, 
	EN, EN_GB, 
	ES_AR, ES_ES, ES_MX, ES_US,
	FR_CA, FR_FR, 
	IT, JA, KO, NO, PT, SV, 
	ZH_CN, ZH_HK, ZH_TW, ZH_US
} profile_lang_id_t;

typedef struct profile_lang_node {
	profile_lang_id_t lang;
	char *last_updated_string;
	char *det;
} profile_lang_node_t;

typedef struct profile_strings_node {
	profile_lang_id_t lang;
	char *lang_string;                   /* Only to make debugging output saner */
	char *charset;
	char *yahoo_id_string;
	char *private_string;
	char *no_answer_string;
	char *my_email_string;
	char *realname_string;
	char *location_string;
	char *age_string;
	char *maritalstatus_string;
	char *gender_string;
	char *occupation_string;
	char *hobbies_string;
	char *latest_news_string;
	char *favorite_quote_string;
	char *links_string;
	char *no_home_page_specified_string;
	char *home_page_string;
	char *no_cool_link_specified_string;
	char *cool_link_1_string;
	char *cool_link_2_string;
	char *cool_link_3_string;
	char *dummy;
} profile_strings_node_t;


typedef enum profile_state {
	PROFILE_STATE_DEFAULT,
	PROFILE_STATE_NOT_FOUND,
	PROFILE_STATE_UNKNOWN_LANGUAGE
} profile_state_t;


typedef struct {
	YahooGetInfoData *info_data;
	char *url_buffer;
	GString *s;
	char *photo_url_text;
	char *profile_url_text;
	char *tooltip_text;
	const profile_strings_node_t *strings;
	const char *last_updated_string;
	const char *title;
	profile_state_t profile_state;
} YahooGetInfoStepTwoData;


/* Strings to determine the profile "language" (more accurately "locale").
 * Strings in this list must be in the original charset in the profile.
 * The "Last Updated" string is used, but sometimes is not sufficient to
 * distinguish 2 locales with this (e.g., ES_ES from ES_US, or FR_CA from
 * FR_FR, or EL from EN_GB), in which case a second string is added and
 * such special cases must be placed before the more general case.
 */
static const profile_lang_node_t profile_langs[] = {
	{ DA,    "Opdateret sidste gang&nbsp;",                          NULL },
	{ DE,    "Letzter Update&nbsp;",                                 NULL },
	{ EL,    "Last Updated:",              "http://gr.profiles.yahoo.com" },
	{ EN_GB, "Last Update&nbsp;",                      "Favourite Quote" },
	{ EN,    "Last Update:",                                        NULL },
	{ EN,    "Last Update&nbsp;",                                   NULL },
	{ ES_AR, "\332ltima actualizaci\363n&nbsp;",                     NULL },
	{ ES_ES, "Actualizada el&nbsp;",       "http://es.profiles.yahoo.com" },
	{ ES_MX, "Actualizada el &nbsp;",      "http://mx.profiles.yahoo.com" },
	{ ES_US, "Actualizada el &nbsp;",                                NULL },
	{ FR_CA, "Derni\xe8re mise \xe0 jour", "http://cf.profiles.yahoo.com" },
	{ FR_FR, "Derni\xe8re mise \xe0 jour",                           NULL },
	{ IT,    "Ultimo aggiornamento&nbsp;",                           NULL },
	{ JA,    "\xba\xc7\xbd\xaa\xb9\xb9\xbf\xb7\xc6\xfc\xa1\xa7",     NULL },
	{ KO,    "\xb0\xbb\xbd\xc5\x20\xb3\xaf\xc2\xa5&nbsp;",           NULL },
	{ NO,    "Sist oppdatert&nbsp;",                                 NULL },
	{ PT,    "\332ltima atualiza\347\343o&nbsp;",                    NULL },
	{ SV,    "Senast uppdaterad&nbsp;",                              NULL },
	{ ZH_CN, "\xd7\xee\xba\xf3\xd0\xde\xb8\xc4\xc8\xd5\xc6\xda",     NULL },
	{ ZH_HK, "\xb3\xcc\xaa\xf1\xa7\xf3\xb7\x73\xae\xc9\xb6\xa1",     NULL },
	{ ZH_US, "\xb3\xcc\xab\xe1\xad\xd7\xa7\xef\xa4\xe9\xb4\xc1", "http://chinese.profiles.yahoo.com" },
	{ ZH_TW, "\xb3\xcc\xab\xe1\xad\xd7\xa7\xef\xa4\xe9\xb4\xc1",     NULL },
	{ XX,     NULL,                                                  NULL }
};

/* Strings in this list must be in UTF-8; &nbsp;'s should be specified as spaces. */
static const profile_strings_node_t profile_strings[] = {
	{ DA, "da", "ISO-8859-1",
		"Yahoo! ID:",
		"Privat",
		"Intet svar",
		"Min Email",
		"Rigtige navn:",
		"Opholdssted:",
		"Alder:",
		"Ægteskabelig status:",
		"Køn:",
		"Erhverv:",
		"Hobbyer:",
		"Sidste nyt:",
		"Favoritcitat",
		"Links",
		"Ingen hjemmeside specificeret",
		"Forside:",
		"Intet cool link specificeret",
		"Cool link 1:",
		"Cool link 2:",
		"Cool link 3:",
		NULL
	},
	{ DE, "de", "ISO-8859-1",
		"Yahoo!-ID:",
		"Privat",
		"Keine Antwort",
		"Meine E-Mail",
		"Realer Name:",
		"Ort:",
		"Alter:",
		"Familienstand:",
		"Geschlecht:",
		"Beruf:",
		"Hobbys:",
		"Neuste Nachrichten:",
		"Mein Lieblingsspruch",
		"Links",
		"Keine Homepage angegeben",
		"Homepage:",
		"Keinen coolen Link angegeben",
		"Cooler Link 1:",
		"Cooler Link 2:",
		"Cooler Link 3:",
		NULL
	},
	{ EL, "el", "ISO-8859-7", /* EL is identical to EN, except no_answer_string */
		"Yahoo! ID:",
		"Private",
		"Καμία απάντηση",
		"My Email",
		"Real Name:",
		"Location:",
		"Age:",
		"Marital Status:",
		"Gender:",
		"Occupation:",
		"Hobbies:",
		"Latest News",
		"Favorite Quote",
		"Links",
		"No home page specified",
		"Home Page:",
		"No cool link specified",
		"Cool Link 1:",
		"Cool Link 2:",
		"Cool Link 3:",
		NULL
	},
	{ EN, "en", "ISO-8859-1",
		"Yahoo! ID:",
		"Private",
		"No Answer",
		"My Email:",
		"Real Name:",
		"Location:",
		"Age:",
		"Marital Status:",
		"Gender:",
		"Occupation:",
		"Hobbies",
		"Latest News",
		"Favorite Quote",
		"Links",
		"No home page specified",
		"Home Page:",
		"No cool link specified",
		"Cool Link 1",
		"Cool Link 2",
		"Cool Link 3",
		NULL
	},
	{ EN_GB, "en_GB", "ISO-8859-1", /* Same as EN except spelling of "Favourite" */
		"Yahoo! ID:",
		"Private",
		"No Answer",
		"My Email:",
		"Real Name:",
		"Location:",
		"Age:",
		"Marital Status:",
		"Gender:",
		"Occupation:",
		"Hobbies",
		"Latest News",
		"Favourite Quote",
		"Links",
		"No home page specified",
		"Home Page:",
		"No cool link specified",
		"Cool Link 1",
		"Cool Link 2",
		"Cool Link 3",
		NULL
	},
	{ ES_AR, "es_AR", "ISO-8859-1",
		"Usuario de Yahoo!:",
		"Privado",
		"No introdujiste una respuesta",
		"Mi dirección de correo electrónico",
		"Nombre real:",
		"Ubicación:",
		"Edad:",
		"Estado civil:",
		"Sexo:",
		"Ocupación:",
		"Pasatiempos:",
		"Últimas noticias:",
		"Tu cita favorita",
		"Enlaces",
		"Ninguna página de inicio especificada",
		"Página de inicio:",
		"Ningún enlace preferido",
		"Enlace genial 1:",
		"Enlace genial 2:",
		"Enlace genial 3:",
		NULL
	},
	{ ES_ES, "es_ES", "ISO-8859-1",
		"ID de Yahoo!:",
		"Privado",
		"Sin respuesta",
		"Mi correo-e",
		"Nombre verdadero:",
		"Lugar:",
		"Edad:",
		"Estado civil:",
		"Sexo:",
		"Ocupación:",
		"Aficiones:",
		"Ultimas Noticias:",
		"Tu cita Favorita",
		"Enlace",
		"Ninguna página personal especificada",
		"Página de Inicio:",
		"Ningún enlace preferido",
		"Enlaces Preferidos 1:",
		"Enlaces Preferidos 2:",
		"Enlaces Preferidos 3:",
		NULL
	},
	{ ES_MX, "es_MX", "ISO-8859-1",
		"ID de Yahoo!:",
		"Privado",
		"Sin responder",
		"Mi Dirección de correo-e",
		"Nombre real:",
		"Ubicación:",
		"Edad:",
		"Estado civil:",
		"Sexo:",
		"Ocupación:",
		"Pasatiempos:",
		"Ultimas Noticias:",
		"Su cita favorita",
		"Enlaces",
		"Ninguna Página predefinida",
		"Página web:",
		"Ningún Enlace preferido",
		"Enlaces Preferidos 1:",
		"Enlaces Preferidos 2:",
		"Enlaces Preferidos 3:",
		NULL
	},
	{ ES_US, "es_US", "ISO-8859-1",
		"ID de Yahoo!:",
		"Privado",
		"No introdujo una respuesta",
		"Mi Dirección de correo-e",
		"Nombre real:",
		"Localidad:",
		"Edad:",
		"Estado civil:",
		"Sexo:",
		"Ocupación:",
		"Pasatiempos:",
		"Ultimas Noticias:",
		"Su cita Favorita",
		"Enlaces",
		"Ninguna Página de inicio predefinida",
		"Página de inicio:",
		"Ningún Enlace preferido",
		"Enlaces Preferidos 1:",
		"Enlaces Preferidos 2:",
		"Enlaces Preferidos 3:",
		NULL
	},
	{ FR_CA, "fr_CA", "ISO-8859-1",
		"Compte Yahoo!:",
		"Privé",
		"Sans réponse",
		"Mon courriel",
		"Nom réel:",
		"Lieu:",
		"Âge:",
		"État civil:",
		"Sexe:",
		"Profession:",
		"Passe-temps:",
		"Actualités:",
		"Citation préférée",
		"Liens",
		"Pas de mention d'une page personnelle",
		"Page personnelle:",
		"Pas de mention d'un lien favori",
		"Lien préféré 1:",
		"Lien préféré 2:",
		"Lien préféré 3:",
		NULL
	},
	{ FR_FR, "fr_FR", "ISO-8859-1",
		"Compte Yahoo!:",
		"Privé",
		"Sans réponse",
		"Mon E-mail",
		"Nom réel:",
		"Lieu:",
		"Âge:",
		"Situation de famille:",
		"Sexe:",
		"Profession:",
		"Centres d'intérêts:",
		"Actualités:",
		"Citation préférée",
		"Liens",
		"Pas de mention d'une page perso",
		"Page perso:",
		"Pas de mention d'un lien favori",
		"Lien préféré 1:",
		"Lien préféré 2:",
		"Lien préféré 3:",
		NULL
	},
	{ IT, "it", "ISO-8859-1",
		"ID Yahoo!:",
		"Non pubblica",
		"Nessuna risposta",
		"La mia e-mail",
		"Nome vero:",
		"Località:",
		"Eta':",
		"Stato civile:",
		"Sesso:",
		"Occupazione:",
		"Hobby:",
		"Ultime notizie:",
		"Citazione preferita",
		"Link",
		"Nessuna home page specificata",
		"Inizio:",
		"Nessun link specificato",
		"Link Preferiti 1:",
		"Link Preferiti 2:",
		"Link Preferiti 3:",
		NULL
	},
	{ JA, "ja", "EUC-JP",
		"Yahoo! JAPAN ID：",
		"非公開",
		"無回答",
		"メール：",
		"名前：",
		"住所：",
		"年齢：",
		"未婚/既婚：",
		"性別：",
		"職業：",
		"趣味：",
		"最近の出来事：",
		NULL,
#if 0
		"おすすめサイト",
#else
		"自己PR", /* "Self description" comes before "Links" for yahoo.co.jp */
#endif
		NULL,
		NULL,
		NULL,
		"おすすめサイト1：",
		"おすすめサイト2：",
		"おすすめサイト3：",
		NULL
	},
	{ KO, "ko", "EUC-KR",
		"야후! ID:",
		"비공개",
		"비공개",
		"My Email",
		"실명:",
		"거주지:",
		"나이:",
		"결혼 여부:",
		"성별:",
		"직업:",
		"취미:",
		"자기 소개:",
		"좋아하는 명언",
		"링크",
		"홈페이지를 지정하지 않았습니다.",
		"홈페이지:",
		"추천 사이트가 없습니다.",
		"추천 사이트 1:",
		"추천 사이트 2:",
		"추천 사이트 3:",
		NULL
	},
	{ NO, "no", "ISO-8859-1",
		"Yahoo! ID:",
		"Privat",
		"Ikke noe svar",
		"Min e-post",
		"Virkelig navn:",
		"Sted:",
		"Alder:",
		"Sivilstatus:",
		"Kjønn:",
		"Yrke:",
		"Hobbyer:",
		"Siste nytt:",
		"Yndlingssitat",
		"Lenker",
		"Ingen hjemmeside angitt",
		"Hjemmeside:",
		"No cool link specified",
		"Bra lenke 1:",
		"Bra lenke 2:",
		"Bra lenke 3:",
		NULL
	},
	{ PT, "pt", "ISO-8859-1",
		"ID Yahoo!:",
		"Particular",
		"Sem resposta",
		"Meu e-mail",
		"Nome verdadeiro:",
		"Local:",
		"Idade:",
		"Estado civil:",
		"Sexo:",
		"Ocupação:",
		"Hobbies:",
		"Últimas notícias:",
		"Frase favorita",
		"Links",
		"Nenhuma página pessoal especificada",
		"Página pessoal:",
		"Nenhum site legal especificado",
		"Site legal 1:",
		"Site legal 2:",
		"Site legal 3:",
		NULL
	},
	{ SV, "sv", "ISO-8859-1",
		"Yahoo!-ID:",
		"Privat",
		"Inget svar",
		"Min mail",
		"Riktigt namn:",
		"Plats:",
		"Ålder:",
		"Civilstånd:",
		"Kön:",
		"Yrke:",
		"Hobby:",
		"Senaste nytt:",
		"Favoritcitat",
		"Länkar",
		"Ingen hemsida specificerad",
		"Hemsida:",
		"Ingen cool länk specificerad",
		"Coola länkar 1:",
		"Coola länkar 2:",
		"Coola länkar 3:",
		NULL
	},
	{ ZH_CN, "zh_CN", "GB2312",
		"Yahoo! ID:",
		"没有提供",
		"没有回答",
		"个人电邮地址",
		"真实姓名:",
		"所在地点:",
		"年龄:",
		"婚姻状况:",
		"性别:",
		"职业:",
		"业余爱好:",
		"个人近况:",
		"喜欢的引言",
		"链接",
		"没有个人主页",
		"个人主页:",
		"没有推荐网站链接",
		"推荐网站链接 1:",
		"推荐网站链接 2:",
		"推荐网站链接 3:",
		NULL
	},
	{ ZH_HK, "zh_HK", "Big5",
		"Yahoo! ID:",
		"私人的",
		"沒有回答",
		"電子信箱",
		"真實姓名:",
		"地點:",
		"年齡:",
		"婚姻狀況:",
		"性別:",
		"職業:",
		"嗜好:",
		"最新消息:",
		"最喜愛的股票叫價", /* [sic] Yahoo!'s translators don't check context */
		"連結",
		"沒有注明個人網頁", /* [sic] */
		"個人網頁:",
		"沒有注明 Cool 連結", /* [sic] */
		"Cool 連結 1:", /* TODO */
		"Cool 連結 2:", /* TODO */
		"Cool 連結 3:", /* TODO */
		NULL
	},
	{ ZH_TW, "zh_TW", "Big5",
		"帳 號:",
		"沒有提供",
		"沒有回應",
		"電子信箱",
		"姓名:",
		"地點:",
		"年齡:",
		"婚姻狀態:",
		"性別:",
		"職業:",
		"興趣:",
		"個人近況:",
		"喜歡的名句",
		"連結",
		"沒有個人網頁",
		"個人網頁:",
		"沒有推薦網站連結",
		"推薦網站連結 1:",
		"推薦網站連結 2:",
		"推薦網站連結 3:",
		NULL
	},
	{ ZH_US, "zh_US", "Big5", /* ZH_US is like ZH_TW, but also a bit like ZH_HK */
		"Yahoo! ID:",
		"沒有提供",
		"沒有回答",
		"個人Email地址",
		"真實姓名:",
		"地點:",
		"年齡:",
		"婚姻狀態:",
		"性別:",
		"職業:",
		"嗜好:",
		"個人近況:",
		"喜歡的名句",
		"連結",
		"沒有個人網頁",
		"個人網頁:",
		"沒有推薦網站連結",
		"推薦網站連結 1:", /* TODO */
		"推薦網站連結 2:", /* TODO */
		"推薦網站連結 3:", /* TODO */
		NULL
	},
	{ XX, NULL, NULL, NULL, NULL, NULL, NULL },
};

static char *yahoo_remove_nonbreaking_spaces(char *str)
{
	char *p;
	while ((p = strstr(str, "&nbsp;")) != NULL) {
		*p = ' '; /* Turn &nbsp;'s into ordinary blanks */
		p += 1;
		memmove(p, p + 5, strlen(p + 5));
		str[strlen(str) - 5] = '\0';
	}
	return str;
}

static char *yahoo_tooltip_info_text(YahooGetInfoData *info_data) {
	GString *s = g_string_sized_new(80); /* wild guess */
	GaimBuddy *b;
	YahooFriend *f;

	g_string_printf(s, "<span style=\"font-size: larger\"><b>%s</b></span><br>",
			info_data->name);
	b = gaim_find_buddy(gaim_connection_get_account(info_data->gc),
			info_data->name);

	if (b) {
		char *statustext = yahoo_tooltip_text(b);
		if(b->alias && b->alias[0]) {
			char *aliastext = g_markup_escape_text(b->alias, -1);
			g_string_append_printf(s, _("<b>Alias:</b> %s<br>"), aliastext);
			g_free(aliastext);
		}
		if (b->idle > 0) {
			char *idletime = gaim_str_seconds_to_string(time(NULL) - b->idle);
			g_string_append_printf(s, _("<b>%s:</b> %s<br>"), _("Idle"),
					idletime);
			g_free(idletime);
		}
		if (statustext) {
			g_string_append_printf(s, "%s<br>", statustext);
			g_free(statustext);
		}
		if ((f = yahoo_friend_find(info_data->gc, b->name))) {
			const char *ip;
			if ((ip = yahoo_friend_get_ip(f)))
				g_string_append_printf(s, _("<b>IP Address:</b> %s<br>"), ip);
		}
	}
	return g_string_free(s, FALSE);
}

#if PHOTO_SUPPORT

static char *yahoo_get_photo_url(const char *url_text, const char *name) {
	GString *s = g_string_sized_new(strlen(name) + 8);
	char *p;
	char *it = NULL;

	/*g_string_printf(s, " alt=\"%s\">", name);*/
	/* Y! newformat */
	g_string_printf(s, " alt=%s>", name);
	p = strstr(url_text, s->str);

	if (p) {
		/* Search backwards for "http://". This is stupid, but it works. */
		for (; !it && p > url_text; p -= 1) {
			/*if (strncmp(p, "\"http://", 8) == 0) {*/
			/* Y! newformat*/
			if (strncmp(p, "=http://", 8) == 0) {
				char *q;
				p += 1; /* skip only the ' ' */
				q = strchr(p, ' ');
				if (q) {
					it = g_strndup(p, q - p);
				}
			}
		}
	}

	g_string_free(s, TRUE);
	return it;
}

static void yahoo_got_photo(void *data, const char *url_text, size_t len);

#endif /* PHOTO_SUPPORT */

static void yahoo_got_info(void *data, const char *url_text, size_t len)
{
	YahooGetInfoData *info_data = (YahooGetInfoData *)data;
	char *p;
	char buf[1024];
#if PHOTO_SUPPORT
	YahooGetInfoStepTwoData *info2_data;
	char *photo_url_text = NULL;
#else
	gboolean found = FALSE;
	char *stripped;
	int stripped_len;
	char *last_updated_utf8_string = NULL;
#endif
	const char *last_updated_string = NULL;
	char *url_buffer;
	GString *s;
	char *tooltip_text = NULL;
	char *profile_url_text = NULL;
	int lang, strid;
	struct yahoo_data *yd;
	const profile_strings_node_t *strings = NULL;
	const char *title;
	profile_state_t profile_state = PROFILE_STATE_DEFAULT;

	if (!GAIM_CONNECTION_IS_VALID(info_data->gc)) {
		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	gaim_debug_info("yahoo", "In yahoo_got_info\n");

	yd = info_data->gc->proto_data;
	title = (yd->jp? _("Yahoo! Japan Profile") :
					 _("Yahoo! Profile"));

	/* Get the tooltip info string */
	tooltip_text = yahoo_tooltip_info_text(info_data);
	
	/* We failed to grab the profile URL.  This is not expected to actually
	 * happen except under unusual error conditions, as Yahoo is observed
	 * to send back HTML, with a 200 status code.
	 */
	if (url_text == NULL || strcmp(url_text, "") == 0) {
		g_snprintf(buf, 1024, "<html><body>%s<b>%s</b></body></html>",
				tooltip_text, _("Error retrieving profile"));

		gaim_notify_userinfo(info_data->gc, info_data->name, NULL, title,
			NULL, buf, NULL, NULL);

		g_free(profile_url_text);
		g_free(tooltip_text);
		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	/* Construct the correct profile URL */
	s = g_string_sized_new(80); /* wild guess */
	g_string_printf(s, "%s%s", (yd->jp? YAHOOJP_PROFILE_URL: YAHOO_PROFILE_URL),
		info_data->name);
	profile_url_text = g_string_free(s, FALSE);
	s = NULL;

	/* We don't yet support the multiple link level of the warning page for
	 * 'adult' profiles, not to mention the fact that yahoo wants you to be
	 * logged in (on the website) to be able to view an 'adult' profile.  For
	 * now, just tell them that we can't help them, and provide a link to the
	 * profile if they want to do the web browser thing.
	 */
	p = strstr(url_text, "Adult Profiles Warning Message");
	if (!p) {
		p = strstr(url_text, "Adult Content Warning"); /* TITLE element */
	}
	if (p) {
		g_snprintf(buf, 1024, "<html><body>%s<b>%s</b><br><br>\n"
					"%s<br><a href=\"%s\">%s</a></body></html>",
				tooltip_text,
				_("Sorry, profiles marked as containing adult content "
				  "are not supported at this time."),
				_("If you wish to view this profile, "
				  "you will need to visit this link in your web browser"),
				profile_url_text, profile_url_text);

		gaim_notify_userinfo(info_data->gc, info_data->name, NULL, title, 
				NULL, buf, NULL, NULL);

		g_free(profile_url_text);
		g_free(tooltip_text);
		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	/* Check whether the profile is written in a supported language */
	for (lang = 0;; lang += 1) {
		last_updated_string = profile_langs[lang].last_updated_string;
	if (!last_updated_string) break;
		p = strstr(url_text, last_updated_string);
		if (p && profile_langs[lang].det && !strstr(url_text, profile_langs[lang].det)) {
			p = NULL;
		}
	if (p) break;
	}
	if (p) {
		for (strid = 0; profile_strings[strid].lang != XX; strid += 1) {
			if (profile_strings[strid].lang == profile_langs[lang].lang) break;
		}
		strings = profile_strings + strid;
		gaim_debug_info("yahoo", "detected profile lang = %s (%d)\n", profile_strings[strid].lang_string, lang);
	}

	/* Every user may choose his/her own profile language, and this language
	 * has nothing to do with the preferences of the user which looks at the
	 * profile. We try to support all languages, but nothing is guaranteed.
	 * If we cannot determine the language, it means either (1) the profile
	 * is written in an unsupported language, (2) our language support is
	 * out of date, or (3) the user is not found, or (4) Y! have changed their
	 * webpage layout
	 */
	if (!p || strings->lang == XX) {
		if (!strstr(url_text, "Yahoo! Member Directory - User not found")
				&& !strstr(url_text, "was not found on this server.")
				&& !strstr(url_text, "\xb8\xf8\xb3\xab\xa5\xd7\xa5\xed\xa5\xd5\xa5\xa3\xa1\xbc\xa5\xeb\xa4\xac\xb8\xab\xa4\xc4\xa4\xab\xa4\xea\xa4\xde\xa4\xbb\xa4\xf3")) {
			profile_state = PROFILE_STATE_UNKNOWN_LANGUAGE;
		} else {
			profile_state = PROFILE_STATE_NOT_FOUND;
		}
	}

#if PHOTO_SUPPORT
	photo_url_text = yahoo_get_photo_url(url_text, info_data->name);
#endif

	url_buffer = g_strdup(url_text);

	/*
	 * gaim_markup_strip_html() doesn't strip out character entities like &nbsp;
	 * and &#183;
	*/
	yahoo_remove_nonbreaking_spaces(url_buffer);
#if 1
	while ((p = strstr(url_buffer, "&#183;")) != NULL) {
		memmove(p, p + 6, strlen(p + 6));
		url_buffer[strlen(url_buffer) - 6] = '\0';
	}
#endif

	/* nuke the nasty \r's */
	gaim_str_strip_cr(url_buffer);

#if PHOTO_SUPPORT
	/* Marshall the existing state */
	info2_data = g_malloc(sizeof(YahooGetInfoStepTwoData));
	info2_data->info_data = info_data;
	info2_data->url_buffer = url_buffer;
	info2_data->s = s;
	info2_data->photo_url_text = photo_url_text;
	info2_data->profile_url_text = profile_url_text;
	info2_data->tooltip_text = tooltip_text;
	info2_data->strings = strings;
	info2_data->last_updated_string = last_updated_string;
	info2_data->title = title;
	info2_data->profile_state = profile_state;

	/* Try to put the photo in there too, if there's one */
	if (photo_url_text) {
		/* User-uploaded photos use a different server that requires the Host
		 * header, but Yahoo Japan will use the "chunked" content encoding if
		 * we specify HTTP 1.1. So we have to specify 1.0 & fix gaim_url_fetch
		 */
		gaim_url_fetch(photo_url_text, FALSE, NULL, FALSE, yahoo_got_photo,
				info2_data);
	} else {
		/* Emulate a callback */
		yahoo_got_photo(info2_data, NULL, 0);
	}
}

static void yahoo_got_photo(void *data, const char *url_text, size_t len)
{
	YahooGetInfoStepTwoData *info2_data = (YahooGetInfoStepTwoData *)data;
	gboolean found = FALSE;
	int id = -1;

	/* Temporary variables */
	char *p = NULL;
	char *stripped;
	int stripped_len;
	char *last_updated_utf8_string = NULL;

	/* Unmarshall the saved state */
	YahooGetInfoData *info_data = info2_data->info_data;
	char *url_buffer = info2_data->url_buffer;
	GString *s = info2_data->s;
	char *photo_url_text = info2_data->photo_url_text;
	char *profile_url_text = info2_data->profile_url_text;
	char *tooltip_text = info2_data->tooltip_text;
	const profile_strings_node_t *strings = info2_data->strings;
	const char *last_updated_string = info2_data->last_updated_string;
	const char *title = info2_data->title;
	profile_state_t profile_state = info2_data->profile_state;

	/* We continue here from yahoo_got_info, as if nothing has happened */
#endif /* PHOTO_SUPPORT */

	/* Jun 29 05 Bleeter: Y! changed their profile pages. Terminators now seem to be */
	/* </dd> and not \n. The prpl's need to be audited before it can be moved */
	/* in to gaim_markup_strip_html*/
	char *fudged_buffer;
	fudged_buffer = gaim_strcasereplace(url_buffer, "</dd>", "</dd><br>");
	/* nuke the html, it's easier than trying to parse the horrid stuff */
	stripped = gaim_markup_strip_html(fudged_buffer);
	stripped_len = strlen(stripped);

	gaim_debug_misc("yahoo", "stripped = %p\n", stripped);
	gaim_debug_misc("yahoo", "url_buffer = %p\n", url_buffer);

	/* convert to utf8 */
	if (strings && strings->charset != XX) {
		p = g_convert(stripped, -1, "utf-8", strings->charset,
				NULL, NULL, NULL);
		if (!p) {
			p = g_locale_to_utf8(stripped, -1, NULL, NULL, NULL);
			if (!p) {
				p = g_convert(stripped, -1, "utf-8", "windows-1252",
						NULL, NULL, NULL);
			}
		}
		if (p) {
			g_free(stripped);
			stripped = gaim_utf8_ncr_decode(p);
			stripped_len = strlen(stripped);
			g_free(p);
		}
	}
	p = NULL;

	/* "Last updated" should also be converted to utf8 and with &nbsp; killed */
	if (strings && strings->charset != XX) {
		last_updated_utf8_string = g_convert(last_updated_string, -1, "utf-8",
				strings->charset, NULL, NULL, NULL);
		yahoo_remove_nonbreaking_spaces(last_updated_utf8_string);

		gaim_debug_misc("yahoo", "after utf8 conversion: stripped = (%s)\n", stripped);
	}

	/* gonna re-use the memory we've already got for url_buffer */
	/* no we're not */
	s = g_string_sized_new(strlen(url_buffer));

	if (profile_state == PROFILE_STATE_DEFAULT) {

#if 0
	/* extract their Yahoo! ID and put it in. Don't bother marking has_info as
	 * true, since the Yahoo! ID will always be there */
	if (!gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->yahoo_id_string, 10, "\n", 0,
			NULL, _("Yahoo! ID"), 0, NULL))
		;
#endif

#if PHOTO_SUPPORT

	/* Try to put the photo in there too, if there's one and is readable */
	if (data && url_text && len != 0) {
		if (strstr(url_text, "400 Bad Request")
				|| strstr(url_text, "403 Forbidden")
				|| strstr(url_text, "404 Not Found")) {

			gaim_debug_info("yahoo", "Error getting %s: %s\n",
					photo_url_text, url_text);
		} else {
			gaim_debug_info("yahoo", "%s is %d bytes\n", photo_url_text, len);
			id = gaim_imgstore_add(url_text, len, NULL);
			g_string_append_printf(s, "<img id=\"%d\"><br>", id);
		}
	}

#endif /* PHOTO_SUPPORT */

	/* extract their Email address and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->my_email_string, 1, " ", 0,
			strings->private_string, _("Email"), 0, NULL);

	/* extract the Nickname if it exists */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			"Nickname:", 1, "\n", '\n',
			NULL, _("Nickname"), 0, NULL);

	/* extract their RealName and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->realname_string, 1, "\n", '\n',
			NULL, _("Realname"), 0, NULL);

	/* extract their Location and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->location_string, 2, "\n", '\n',
			NULL, _("Location"), 0, NULL);

	/* extract their Age and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->age_string, 3, "\n", '\n',
			NULL, _("Age"), 0, NULL);

	/* extract their MaritalStatus and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->maritalstatus_string, 3, "\n", '\n',
			strings->no_answer_string, _("Marital Status"), 0, NULL);

	/* extract their Gender and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->gender_string, 3, "\n", '\n',
			strings->no_answer_string, _("Gender"), 0, NULL);

	/* extract their Occupation and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->occupation_string, 2, "\n", '\n',
			NULL, _("Occupation"), 0, NULL);

	/* Hobbies, Latest News, and Favorite Quote are a bit different, since
	 * the values can contain embedded newlines... but any or all of them
	 * can also not appear.  The way we delimit them is to successively
	 * look for the next one that _could_ appear, and if all else fails,
	 * we end the section by looking for the 'Links' heading, which is the
	 * next thing to follow this bunch.  (For Yahoo Japan, we check for
	 * the "Description" ("Self PR") heading instead of "Links".)
	 */

	if (!gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->hobbies_string, 1, strings->latest_news_string,
			'\n', "\n", _("Hobbies"), 0, NULL))
	{
		if (!gaim_markup_extract_info_field(stripped, stripped_len, s,
				strings->hobbies_string, 1, strings->favorite_quote_string,
				'\n', "\n", _("Hobbies"), 0, NULL))
		{
			found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
					strings->hobbies_string, 1, strings->links_string,
					'\n', "\n", _("Hobbies"), 0, NULL);
		}
		else
			found = TRUE;
	}
	else
		found = TRUE;

	if (!gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->latest_news_string, 1, strings->favorite_quote_string,
			'\n', "\n", _("Latest News"), 0, NULL))
	{
		found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
				strings->latest_news_string, 1, strings->links_string,
				'\n', "\n", _("Latest News"), 0, NULL);
	}
	else
		found = TRUE;

	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			strings->favorite_quote_string, 1, strings->links_string,
			'\n', "\n", _("Favorite Quote"), 0, NULL);

	/* Home Page will either be "No home page specified",
	 * or "Home Page: " and a link.
	 * For Yahoo! Japan, if there is no home page specified,
	 * neither "No home page specified" nor "Home Page:" is shown.
	 */
	if (strings->home_page_string) {
		p = !strings->no_home_page_specified_string? NULL:
			strstr(stripped, strings->no_home_page_specified_string);
		if(!p)
		{
			found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
					strings->home_page_string, 1, "\n", 0, NULL,
					_("Home Page"), 1, NULL);
		}
	}

	/* Cool Link {1,2,3} is also different.  If "No cool link specified"
	 * exists, then we have none.  If we have one however, we'll need to
	 * check and see if we have a second one.  If we have a second one,
	 * we have to check to see if we have a third one.
	 */
	p = !strings->no_cool_link_specified_string? NULL:
		strstr(stripped,strings->no_cool_link_specified_string);
	if (!p)
	{
		if (gaim_markup_extract_info_field(stripped, stripped_len, s,
				strings->cool_link_1_string, 1, "\n", 0, NULL,
				_("Cool Link 1"), 1, NULL))
		{
			found = TRUE;
			if (gaim_markup_extract_info_field(stripped, stripped_len, s,
					strings->cool_link_2_string, 1, "\n", 0, NULL,
					_("Cool Link 2"), 1, NULL))
			{
				gaim_markup_extract_info_field(stripped, stripped_len, s,
						strings->cool_link_3_string, 1, "\n", 0, NULL,
						_("Cool Link 3"), 1, NULL);
			}
		}
	}

	/* see if Member Since is there, and if so, extract it. */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			"Member Since:", 1, last_updated_utf8_string,
			'\n', NULL, _("Member Since"), 0, NULL);

	/* extract the Last Updated date and put it in */
	found |= gaim_markup_extract_info_field(stripped, stripped_len, s,
			last_updated_utf8_string, 1, " ", '\n', NULL,
			_("Last Update"), 0, NULL);
	} /* if (profile_state == PROFILE_STATE_DEFAULT) */

	if(!found)
	{
		g_string_append_printf(s, "<br><b>");
		g_string_append_printf(s, _("User information for %s unavailable"),
				info_data->name);
		g_string_append_printf(s, "</b><br>");

		if (profile_state == PROFILE_STATE_UNKNOWN_LANGUAGE) {
			g_string_append_printf(s, "%s<br><br>",
					_("Sorry, this profile seems to be in a language "
					  "that is not supported at this time."));

		} else if (profile_state == PROFILE_STATE_NOT_FOUND) {
			GaimBuddy *b = gaim_find_buddy
					(gaim_connection_get_account(info_data->gc),
							info_data->name);
			YahooFriend *f = NULL;
			if (b) {
				/* Someone on the buddy list can be "not on server list",
				 * in which case the user may or may not actually exist.
				 * Hence this extra step.
				 */
				f = yahoo_friend_find(b->account->gc, b->name);
			}
			g_string_append_printf(s, "%s<br><br>",
				f?  _("Could not retrieve the user's profile. "
					  "This most likely is a temporary server-side problem. "
					  "Please try again later."):
					_("Could not retrieve the user's profile. "
					  "This most likely means that the user does not exist; "
					  "however, Yahoo! sometimes does fail to find a user's "
					  "profile. If you know that the user exists, "
					  "please try again later."));
		
		} else {
			g_string_append_printf(s, "%s<br><br>",
					_("The user's profile is empty."));
		}
	}

	/* put a link to the actual profile URL */
	g_string_append_printf(s, _("<b>%s:</b> "), _("Profile URL"));
	g_string_append_printf(s, "<br><a href=\"%s\">%s</a><br>",
			profile_url_text, profile_url_text);

	/* finish off the html at the end */
	g_string_append(s, "</body></html>\n");
	g_free(stripped);

	/* Put the Yahoo! ID, nickname, idle time, and status message in */
	g_string_prepend(s, tooltip_text);

	/* finish off the html at the beginning */
	g_string_prepend(s, "<html><body>\n");

	/* show it to the user */
	gaim_notify_userinfo(info_data->gc, info_data->name, NULL, title,
						  NULL, s->str, NULL, NULL);

	g_free(last_updated_utf8_string);
	g_free(url_buffer);
	g_free(fudged_buffer);
	g_string_free(s, TRUE);
	g_free(profile_url_text);
	g_free(tooltip_text);
	g_free(info_data->name);
	g_free(info_data);

#if PHOTO_SUPPORT
	g_free(photo_url_text);
	g_free(info2_data);
	if (id != -1)
		gaim_imgstore_unref(id);
#endif
}

void yahoo_get_info(GaimConnection *gc, const char *name)
{
	struct yahoo_data *yd = gc->proto_data;
	YahooGetInfoData *data;
	char *url;

	data       = g_new0(YahooGetInfoData, 1);
	data->gc   = gc;
	data->name = g_strdup(name);

	url = g_strdup_printf("%s%s",
			(yd->jp? YAHOOJP_PROFILE_URL: YAHOO_PROFILE_URL), name);

	gaim_url_fetch(url, TRUE, NULL, FALSE, yahoo_got_info, data);

	g_free(url);
}
