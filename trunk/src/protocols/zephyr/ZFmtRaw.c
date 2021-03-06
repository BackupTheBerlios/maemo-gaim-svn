/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZFormatRawNotice function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFmtRaw.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFmtRaw.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $ */

#ifndef lint
static char rcsid_ZFormatRawNotice_c[] = "$Id: ZFmtRaw.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $";
#endif

#include "internal.h"

Code_t ZFormatRawNotice(notice, buffer, ret_len)
    register ZNotice_t *notice;
    char **buffer;
    int *ret_len;
{
    char header[Z_MAXHEADERLEN];
    int hdrlen;
    Code_t retval;

    if ((retval = Z_FormatRawHeader(notice, header, sizeof(header),
				    &hdrlen, NULL, NULL)) != ZERR_NONE)
	return (retval);

    *ret_len = hdrlen+notice->z_message_len;

    /* *ret_len is never 0, don't have to worry about malloc(0) */
    if (!(*buffer = (char *) malloc((unsigned) *ret_len)))
	return (ENOMEM);

    (void) memcpy(*buffer, header, hdrlen);
    (void) memcpy(*buffer+hdrlen, notice->z_message, notice->z_message_len);

    return (ZERR_NONE);
}
