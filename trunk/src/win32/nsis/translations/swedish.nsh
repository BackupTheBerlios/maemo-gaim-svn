;;
;;  swedish.nsh
;;
;;  Swedish language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Tore Lundqvist <tlt@mima.x.se>, 2003.
;;  Author: Peter Hjalmarsson <xake@telia.com>, 2005.
;;  Version 3

; Make sure to update the GAIM_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Installationsprogrammet k�rs redan."
!define GAIM_IS_RUNNING			"En instans av Giam k�rs redan. Avsluta Gaim och f�rs�k igen."
!define GTK_INSTALLER_NEEDED			"K�rmilj�n GTK+ �r antingen inte installerat eller beh�ver uppgraderas.$\rVar god installera v${GTK_VERSION} eller h�gre av GTK+-k�rmilj�n."

; License Page
!define GAIM_LICENSE_BUTTON			"N�sta >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) �r utgivet under GPL. Licensen finns tillg�nglig h�r f�r infromationssyften enbart. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Snabbmeddelandeklient (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+-k�rmilj� (obligatorisk)"
!define GTK_THEMES_SECTION_TITLE		"GTK+-teman"
!define GTK_NOTHEME_SECTION_TITLE		"Inget tema"
!define GTK_WIMP_SECTION_TITLE		"Wimp-tema"
!define GTK_BLUECURVE_SECTION_TITLE	"Bluecurve-tema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue-tema"
!define GAIM_SHORTCUTS_SECTION_TITLE "Genv�gar"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE "Skrivbord"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE "Startmeny"
!define GAIM_SECTION_DESCRIPTION		"Gaims k�rnfiler och DLL:er"
!define GTK_SECTION_DESCRIPTION		"En GUI-verktygsupps�ttning f�r flera olika plattformar som Gaim anv�nder."
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+-teman kan �ndra k�nslan av och utseendet p� GTK+-applikationer."
!define GTK_NO_THEME_DESC			"Installera inte n�got GTK+-tema"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) ett GTK-tema som sm�lter bra in i Windows-milj�n."
!define GTK_BLUECURVE_THEME_DESC		"The Bluecurve-tema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"The Lighthouseblue-tema."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION   "Genv�gar f�r att starta Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC   "Skapar en genv�g till Gaim p� skrivbordet"
!define GAIM_STARTMENU_SHORTCUT_DESC   "Skapar ett till�gg i startmenyn f�r Gaim"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"En �ldre version av GTK+ runtime hittades, vill du uppgradera den?$\rOBS! Gaim kommer kanske inte att fungera om du inte uppgraderar."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Bes�k Windows-Gaims hemsida"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (enbart f�r avinstallation)"
!define GAIM_PROMPT_WIPEOUT			"Din gamla Gaim-katalog kommer att raderas, vill du forts�tta?$\r$\rOBS! om du har installerat n�gra extra insticksmoduler kommer de raderas.$\rGaims anv�ndarinst�llningar kommer inte p�verkas."
!define GAIM_PROMPT_DIR_EXISTS		"Den katalog du vill installera i finns redan. Allt i katalogen$\rkommer att raderas, vill du forts�tta?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fel vid installation av GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Den s�kv�g du angivit g�r inte att komma �t eller skapa."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Du har inte r�ttigheter att installera ett GTK+tema."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "Avinstalleraren kunde inte hitta registerv�rden f�r Gaim.$\rAntagligen har en annan anv�ndare installerat applikationen."
!define un.GAIM_UNINSTALL_ERROR_2         "Du har inte r�ttigheter att avinstallera den h�r applikationen."
