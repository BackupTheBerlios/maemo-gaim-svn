/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZPending function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZPending.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZPending.c,v 1.2 2004/04/24 09:02:28 chipx86 Exp $ */

#ifndef lint
static char rcsid_ZPending_c[] = "$Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZPending.c,v 1.2 2004/04/24 09:02:28 chipx86 Exp $";
#endif

#include "internal.h"

int ZPending()
{
	int retval;
	
	if (ZGetFD() < 0) {
		errno = ZERR_NOPORT;
		return (-1);
	}
	
	if ((retval = Z_ReadEnqueue()) != ZERR_NONE) {
		errno = retval;
		return (-1);
	} 
	
	return(ZQLength());
}
