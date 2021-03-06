/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZSetLocation, ZUnsetLocation, and
 * ZFlushMyLocations functions.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZLocations.c,v $
 *	$Author: thekingant $
 *
 *	Copyright (c) 1987,1988,1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZLocations.c,v 1.4.2.1 2005/05/03 02:52:08 thekingant Exp $ */

#ifndef lint
static char rcsid_ZLocations_c[] =
    "$Zephyr: /afs/athena.mit.edu/astaff/project/zephyr/src/lib/RCS/ZLocations.c,v 1.30 90/12/20 03:04:39 raeburn Exp $";
#endif

#include "internal.h"

#include <pwd.h>

extern char *getenv();
extern int errno;

Code_t ZSetLocation(exposure)
    char *exposure;
{
    return (Z_SendLocation(LOGIN_CLASS, exposure, ZAUTH, 
			   "$sender logged in to $1 on $3 at $2"));
}

Code_t ZUnsetLocation()
{
    return (Z_SendLocation(LOGIN_CLASS, LOGIN_USER_LOGOUT, ZNOAUTH, 
			   "$sender logged out of $1 on $3 at $2"));
}

Code_t ZFlushMyLocations()
{
    return (Z_SendLocation(LOGIN_CLASS, LOGIN_USER_FLUSH, ZAUTH, ""));
}

static char host[MAXHOSTNAMELEN], mytty[MAXPATHLEN];
static int reenter = 0;

Code_t Z_SendLocation(class, opcode, auth, format)
    char *class;
    char *opcode;
    Z_AuthProc auth;
    char *format;
{
    int retval;
    time_t ourtime;
    ZNotice_t notice, retnotice;
    char *bptr[3], *p;
#ifndef X_DISPLAY_MISSING
    char *display;
#endif
    char *ttyp;
    struct hostent *hent;
    short wg_port = ZGetWGPort();

    (void) memset((char *)&notice, 0, sizeof(notice));
    notice.z_kind = ACKED;
    notice.z_port = (unsigned short) ((wg_port == -1) ? 0 : wg_port);
    notice.z_class = class;
    notice.z_class_inst = ZGetSender();
    notice.z_opcode = opcode;
    notice.z_sender = 0;
    notice.z_recipient = "";
    notice.z_num_other_fields = 0;
    notice.z_default_format = format;

    /*
      keep track of what we said before so that we can be consistent
      when changing location information.
      This is done mainly for the sake of the WindowGram client.
     */

    if (!reenter) {
	    if (gethostname(host, MAXHOSTNAMELEN) < 0)
		    return (errno);

	    hent = gethostbyname(host);
	    if (hent) {
	      (void) strncpy(host, hent->h_name, sizeof(host));
	      host[sizeof(host) - 1] = '\0';
	    }
#ifndef X_DISPLAY_MISSING
	    if ((display = getenv("DISPLAY")) && *display) {
		    (void) strncpy(mytty, display, sizeof(mytty));
	    } else {
#endif
		    ttyp = ttyname(0);
		    if (ttyp && *ttyp) {
			p = strchr(ttyp + 1, '/');
			strcpy(mytty, (p) ? p + 1 : ttyp);
		    } else {
			strcpy(mytty, "unknown");
		    }
#ifndef X_DISPLAY_MISSING
	    }
#endif
	    reenter = 1;
    }

    ourtime = time((time_t *)0);
    bptr[0] = host;
    bptr[1] = ctime(&ourtime);
    bptr[1][strlen(bptr[1])-1] = '\0';
    bptr[2] = mytty;

	
    if ((retval = ZSendList(&notice, bptr, 3, auth)) != ZERR_NONE)
	return (retval);

    retval = Z_WaitForNotice (&retnotice, ZCompareUIDPred, &notice.z_uid,
			      SRV_TIMEOUT);
    if (retval != ZERR_NONE)
      return retval;

    if (retnotice.z_kind == SERVNAK) {
	if (!retnotice.z_message_len) {
	    ZFreeNotice(&retnotice);
	    return (ZERR_SERVNAK);
	}
	if (!strcmp(retnotice.z_message, ZSRVACK_NOTSENT)) {
	    ZFreeNotice(&retnotice);
	    return (ZERR_AUTHFAIL);
	}
	if (!strcmp(retnotice.z_message, ZSRVACK_FAIL)) {
	    ZFreeNotice(&retnotice);
	    return (ZERR_LOGINFAIL);
	}
	ZFreeNotice(&retnotice);
	return (ZERR_SERVNAK);
    } 
	
    if (retnotice.z_kind != SERVACK) {
	ZFreeNotice(&retnotice);
	return (ZERR_INTERNAL);
    }

    if (!retnotice.z_message_len) {
	ZFreeNotice(&retnotice);
	return (ZERR_INTERNAL);
    }

    if (strcmp(retnotice.z_message, ZSRVACK_SENT) &&
	strcmp(retnotice.z_message, ZSRVACK_NOTSENT)) {
	ZFreeNotice(&retnotice);
	return (ZERR_INTERNAL);
    }

    ZFreeNotice(&retnotice);
	
    return (ZERR_NONE);
}
