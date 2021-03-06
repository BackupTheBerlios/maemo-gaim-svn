/*
 * $Header: /cvsroot/gaim/gaim/src/protocols/zephyr/error_message.c,v 1.2 2003/09/21 20:47:24 thekingant Exp $
 * $Source: /cvsroot/gaim/gaim/src/protocols/zephyr/error_message.c,v $
 * $Locker:  $
 *
 * Copyright 1987 by the Student Information Processing Board
 * of the Massachusetts Institute of Technology
 *
 * For copyright info, see "mit-sipb-copyright.h".
 */

#include "error_table.h"
#include "mit-sipb-copyright.h"
#include "com_err.h"
#include <sysdep.h>

static const char rcsid[] =
    "$Header: /cvsroot/gaim/gaim/src/protocols/zephyr/error_message.c,v 1.2 2003/09/21 20:47:24 thekingant Exp $";
static const char copyright[] =
    "Copyright 1986, 1987, 1988 by the Student Information Processing Board\nand the department of Information Systems\nof the Massachusetts Institute of Technology";

char *error_table_name_r __P((int, char *));

struct et_list * _et_list = (struct et_list *) NULL;

const char * error_message (code)
long	code;
{
    static char buf[COM_ERR_BUF_LEN];

    return(error_message_r(code, buf));
}

const char * error_message_r (code, buf)
long	code;
char	*buf;
{
    int offset;
    struct et_list *et;
    int table_num;
    int started = 0;
    char *cp, namebuf[6];

    offset = code & ((1<<ERRCODE_RANGE)-1);
    table_num = code - offset;
    if (!table_num)
	return strerror(offset);
    for (et = _et_list; et; et = et->next) {
	if (et->table->base == table_num) {
	    /* This is the right table */
	    if (et->table->n_msgs <= offset)
		break;
	    return(et->table->msgs[offset]);
	}
    }

    strcpy (buf, "Unknown code ");
    if (table_num) {
	strcat (buf, error_table_name_r (table_num, namebuf));
	strcat (buf, " ");
    }
    for (cp = buf; *cp; cp++)
	;
    if (offset >= 100) {
	*cp++ = '0' + offset / 100;
	offset %= 100;
	started++;
    }
    if (started || offset >= 10) {
	*cp++ = '0' + offset / 10;
	offset %= 10;
    }
    *cp++ = '0' + offset;
    *cp = '\0';
    return(buf);
}
