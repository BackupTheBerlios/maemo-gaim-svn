/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZCompareUIDPred function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZCmpUIDP.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZCmpUIDP.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $ */

#ifndef lint
static char rcsid_ZCompareUIDPred_c[] = "$Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZCmpUIDP.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $";
#endif

#include "internal.h"

int ZCompareUIDPred(notice, uid)
    ZNotice_t *notice;
    void *uid;
{
    return (ZCompareUID(&notice->z_uid, (ZUnique_Id_t *) uid));
}

int ZCompareMultiUIDPred(notice, uid)
    ZNotice_t *notice;
    void *uid;
{
    return (ZCompareUID(&notice->z_multiuid, (ZUnique_Id_t *) uid));
}
