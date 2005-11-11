/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZFreeNotice function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFreeNot.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFreeNot.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $ */

#ifndef lint
static char rcsid_ZFreeNotice_c[] = "$Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZFreeNot.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $";
#endif

#include "internal.h"

Code_t ZFreeNotice(notice)
    ZNotice_t *notice;
{
    free(notice->z_packet);
    return 0;
}
