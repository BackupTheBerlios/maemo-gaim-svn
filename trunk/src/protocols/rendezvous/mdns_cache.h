/**
 * @file mdns_cache.h Multicast DNS resource record caching code.
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
 *
 */

#ifndef _MDNS_CACHE_H_
#define _MDNS_CACHE_H_

#include "internal.h"
#include "debug.h"

#include "mdns.h"

/**
 *
 */
void mdns_cache_add(const ResourceRecord *rr);

/**
 *
 */
void mdns_cache_remove(gchar *name, unsigned short type);

/**
 *
 */
void mdns_cache_remove_all();

/**
 *
 */
void mdns_cache_respond(int fd, const Question *q);

#endif /* _MDNS_CACHE_H_ */
