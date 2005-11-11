/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZSetFD function.
 *
 *	Created by:	Robert French
 *
 *	$Source: /cvsroot/gaim/gaim/src/protocols/zephyr/ZSetFD.c,v $
 *	$Author: chipx86 $
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZSetFD.c,v 1.2 2004/04/24 09:02:28 chipx86 Exp $ */

#ifndef lint
static char rcsid_ZSetFD_c[] = "$Header: /cvsroot/gaim/gaim/src/protocols/zephyr/ZSetFD.c,v 1.2 2004/04/24 09:02:28 chipx86 Exp $";
#endif

#include "internal.h"

Code_t ZSetFD(fd)
	int	fd;
{
	(void) ZClosePort();

	__Zephyr_fd = fd;
	__Zephyr_open = 0;
	
	return (ZERR_NONE);
}
