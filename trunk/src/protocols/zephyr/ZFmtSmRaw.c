/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZFormatSmallRawNotice function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFmtSmRaw.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFmtSmRaw.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $ */

#ifndef lint
static char rcsid_ZFormatRawNotice_c[] = "$Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFmtSmRaw.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $";
#endif

#include "internal.h"

Code_t ZFormatSmallRawNotice(notice, buffer, ret_len)
    ZNotice_t *notice;
    ZPacket_t buffer;
    int *ret_len;
{
    Code_t retval;
    int hdrlen;
    
    if ((retval = Z_FormatRawHeader(notice, buffer, Z_MAXHEADERLEN,
				    &hdrlen, NULL, NULL)) != ZERR_NONE)
	return (retval);

    *ret_len = hdrlen+notice->z_message_len;

    if (*ret_len > Z_MAXPKTLEN)
	return (ZERR_PKTLEN);

    (void) memcpy(buffer+hdrlen, notice->z_message, notice->z_message_len);

    return (ZERR_NONE);
}
