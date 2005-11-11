/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZSendNotice function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZSendNot.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZSendNot.c,v 1.2 2004/04/24 09:02:28 chipx86 Exp $ */

#ifndef lint
static char rcsid_ZSendNotice_c[] = "$Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZSendNot.c,v 1.2 2004/04/24 09:02:28 chipx86 Exp $";
#endif

#include "internal.h"

Code_t ZSendNotice(notice, cert_routine)
    ZNotice_t *notice;
    Z_AuthProc cert_routine;
{
    return(ZSrvSendNotice(notice, cert_routine, Z_XmitFragment));
}

Code_t ZSrvSendNotice(notice, cert_routine, send_routine)
    ZNotice_t *notice;
    Z_AuthProc cert_routine;
    Code_t (*send_routine)();
{    
    Code_t retval;
    ZNotice_t newnotice;
    char *buffer;
    int len;

    if ((retval = ZFormatNotice(notice, &buffer, &len, 
				cert_routine)) != ZERR_NONE)
	return (retval);

    if ((retval = ZParseNotice(buffer, len, &newnotice)) != ZERR_NONE)
	return (retval);
    
    retval = Z_SendFragmentedNotice(&newnotice, len, cert_routine,
				    send_routine);

    free(buffer);

    return (retval);
}
