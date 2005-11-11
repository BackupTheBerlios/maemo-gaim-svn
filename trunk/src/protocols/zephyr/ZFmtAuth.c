/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZFormatAuthenticNotice function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFmtAuth.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987,1988 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFmtAuth.c,v 1.3 2004/04/24 09:02:27 chipx86 Exp $ */

#ifndef lint
static char rcsid_ZFormatAuthenticNotice_c[] = "$Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFmtAuth.c,v 1.3 2004/04/24 09:02:27 chipx86 Exp $";
#endif

#include "internal.h"

#ifdef ZEPHYR_USES_KERBEROS
Code_t ZFormatAuthenticNotice(notice, buffer, buffer_len, len, session)
    ZNotice_t *notice;
    register char *buffer;
    register int buffer_len;
    int *len;
    C_Block session;
{
    ZNotice_t newnotice;
    char *ptr;
    int retval, hdrlen;

    newnotice = *notice;
    newnotice.z_auth = 1;
    newnotice.z_authent_len = 0;
    newnotice.z_ascii_authent = "";

    if ((retval = Z_FormatRawHeader(&newnotice, buffer, buffer_len,
				    &hdrlen, &ptr, NULL)) != ZERR_NONE)
	return (retval);

#ifdef NOENCRYPTION
    newnotice.z_checksum = 0;
#else
    newnotice.z_checksum =
	(ZChecksum_t)des_quad_cksum(buffer, NULL, ptr - buffer, 0, (C_Block*)session);
#endif
    if ((retval = Z_FormatRawHeader(&newnotice, buffer, buffer_len,
				    &hdrlen, NULL, NULL)) != ZERR_NONE)
	return (retval);

    ptr = buffer+hdrlen;

    if (newnotice.z_message_len+hdrlen > buffer_len)
	return (ZERR_PKTLEN);

    (void) memcpy(ptr, newnotice.z_message, newnotice.z_message_len);

    *len = hdrlen+newnotice.z_message_len;

    if (*len > Z_MAXPKTLEN)
	return (ZERR_PKTLEN);

    return (ZERR_NONE);
}
#endif
