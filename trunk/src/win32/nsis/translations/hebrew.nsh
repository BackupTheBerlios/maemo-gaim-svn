;;
;;  hebrew.nsh
;;
;;  Hebrew language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1255
;;
;;  Author: Eugene Shcherbina <eugene@websterworlds.com>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			".�� ����� �� ����� ������ GTK+ �����$\r����� ���� v${GTK_VERSION} .GTK+ �� ����� ���� �� �����"

; License Page
!define GAIM_LICENSE_BUTTON			"��� >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) .������� ���� ��� ����� ���� ���� .GPL ������ ��� ������ $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"(����) .GAIM �����"
!define GTK_SECTION_TITLE			"(����) .GTK+ �����"
!define GTK_THEMES_SECTION_TITLE		"GTK+ ����� ��"
!define GTK_NOTHEME_SECTION_TITLE		"��� ����"
!define GTK_WIMP_SECTION_TITLE		"Wimp ����"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve ����"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue ����"
!define GAIM_SECTION_DESCRIPTION		".������� DLL-� GAIM ����"
!define GTK_SECTION_DESCRIPTION		".�����-���������� GUI ���"
!define GTK_THEMES_SECTION_DESCRIPTION	" .GTK+ ������ ����� �� ����� �� ������ GTK+ �����"
!define GTK_NO_THEME_DESC			".GTK+ �� ������ ���� ��"
!define GTK_WIMP_THEME_DESC			".�� ���� ������ ����� �� ������ GTK-Wimp (Windows impersonator)"
!define GTK_BLUECURVE_THEME_DESC		".Bluecurve ����� ��"
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	".Lighthouseblue����� ��"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"A?�����. ����� GTK+ ����� ���� ��$\rNote: .���� �� ����� �� �� GAIM"

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		".GAIM���� ���� ��"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"GAIM (����� ����)"
!define GAIM_PROMPT_WIPEOUT			"?����� ����� ������. ������ GAIM ������$\r$\rNote: ����: ������ �� ������ �����.$\r.������ ������ �� �����"
!define GAIM_PROMPT_DIR_EXISTS		"������ ������ ��� �����. �� ������� $\r?����. ������"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			".GTK+ ����� ������ �����"
!define GTK_BAD_INSTALL_PATH			".������ ������ �� ���� �������"

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		".GTK+ ��� �� ���� ������ ���� ��"

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		".GTK+ ������ �� ���� �� �������� ��$\r.���� ����� ������� ��� ����� �� ������ ����"
!define un.GAIM_UNINSTALL_ERROR_2		".��� �� ���� ����� ����� ���"
