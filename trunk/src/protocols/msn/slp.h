/**
 * @file slp.h MSNSLP support
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _MSN_SLP_H_
#define _MSN_SLP_H_

#include "slpcall.h"

#include "ft.h"

void msn_xfer_progress_cb(MsnSlpCall *slpcall, gsize total_length, gsize
						  len, gsize offset);

MsnSlpCall * msn_slp_sip_recv(MsnSlpLink *slplink,
							  const char *body);

void send_bye(MsnSlpCall *slpcall, const char *type);

void msn_xfer_completed_cb(MsnSlpCall *slpcall,
						   const char *body, long long size);

void msn_xfer_cancel(GaimXfer *xfer);
void msn_xfer_end_cb(MsnSlpCall *slpcall);

void msn_queue_buddy_icon_request(MsnUser *user);

#endif /* _MSN_SLP_H_ */
