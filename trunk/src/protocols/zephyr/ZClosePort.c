/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZClosePort function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZClosePort.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZClosePort.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $ */

#include "internal.h"

#ifndef lint
static const char rcsid_ZClosePort_c[] = "$Id: ZClosePort.c,v 1.2 2004/04/24 09:02:27 chipx86 Exp $";
#endif

Code_t ZClosePort()
{
    if (__Zephyr_fd >= 0 && __Zephyr_open)
	(void) close(__Zephyr_fd);

    __Zephyr_fd = -1;
    __Zephyr_open = 0;
	
    return (ZERR_NONE);
}
