;;
;;  portuguese.nsh
;;
;;  Portuguese (PT) language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Duarte Henriques <duarte.henriques@gmail.com>, 2003-2005.
;;  Version 3
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"O instalador j� est� a ser executado."
!define GAIM_IS_RUNNING			"Uma inst�ncia do Gaim j� est� a ser executada. Saia do Gaim e tente de novo."
!define GTK_INSTALLER_NEEDED			"O ambiente de GTK+ est� ausente ou precisa de ser actualizado.$\rPor favor instale a vers�o v${GTK_VERSION} ou mais recente do ambiente de GTK+."

; License Page
!define GAIM_LICENSE_BUTTON			"Seguinte >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) est� dispon�vel sob a licen�a GNU General Public License (GPL). O texto da licen�a � fornecido aqui meramente a t�tulo informativo. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Cliente de Mensagens Instant�neas Gaim (obrigat�rio)"
!define GTK_SECTION_TITLE			"Ambiente de Execu��o GTK+ (obrigat�rio)"
!define GTK_THEMES_SECTION_TITLE		"Temas do GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Nenhum tema"
!define GTK_WIMP_SECTION_TITLE		"Tema Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema Light House Blue"
!define GAIM_SHORTCUTS_SECTION_TITLE "Atalhos"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE "Ambiente de Trabalho"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE "Menu de Iniciar"
!define GAIM_SECTION_DESCRIPTION		"Ficheiros e bibliotecas principais do Gaim"
!define GTK_SECTION_DESCRIPTION		"Um conjunto de ferramentas de interface gr�fica multi-plataforma, usado pelo Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"Os Temas do GTK+ podem mudar a apar�ncia dos programas GTK+."
!define GTK_NO_THEME_DESC			"N�o instalar um tema do GTK+"
!define GTK_WIMP_THEME_DESC			"O tema GTK-Wimp (Windows impersonator, personificador do Windows) � um tema GTK+ que combina bem com o ambiente de trabalho do Windows."
!define GTK_BLUECURVE_THEME_DESC		"O tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"O tema Lighthouseblue."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION   "Atalhos para iniciar o Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC   "Criar um atalho para o Gaim no Ambiente de Trabalho"
!define GAIM_STARTMENU_SHORTCUT_DESC   "Criar uma entrada para o Gaim na Barra de Iniciar"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Foi encontrada uma vers�o antiga do ambiente de execu��o GTK+. Deseja actualiz�-lo?$\rNota: O Gaim poder� n�o funcionar se n�o o fizer."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Visite a P�gina Web do Gaim para Windows"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (remover apenas)"
!define GAIM_PROMPT_WIPEOUT			"A directoria antiga do Gaim est� prestes a ser removida. Deseja continuar?$\r$\rNota: Quaisquer plugins n�o-padr�o que poder� ter instalado ser�o removidos.$\rAs configura��es de utilizador do Gaim n�o ser�o afectadas."
!define GAIM_PROMPT_DIR_EXISTS		"A directoria de instala��o que especificou j� existe. Qualquer conte�do$\rser� removido. Deseja continuar?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro ao instalar o ambiente de execu��o GTK+."
!define GTK_BAD_INSTALL_PATH			"O caminho que digitou n�o pode ser acedido nem criado."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS	"N�o tem permiss�o para instalar um tema do GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"O desinstalador n�o encontrou entradas de registo do Gaim.$\r� prov�vel que outro utilizador tenha instalado este programa."
!define un.GAIM_UNINSTALL_ERROR_2		"N�o tem permiss�o para desinstalar este programa."
