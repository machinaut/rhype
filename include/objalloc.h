/*
 * Copyright (C) 2005 Jimi Xenidis <jimix@watson.ibm.com>, IBM Corporation
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 $Id$
 * Page allocator.
 */

#ifndef _OBJALLOC_H
#define _OBJALLOC_H

#include <config.h>
#include <types.h>
#include <pgalloc.h>
extern void init_allocator(struct pg_alloc *pa);
extern void *halloc(uval size);
extern void hfree(void *ptr, uval size);

struct objcache;
extern void cache_init(struct pg_alloc *pa, struct objcache *cache, uval size);
extern void cache_free(struct objcache *cache, void *ptr);
extern void *cache_alloc(struct objcache *hdr);
extern void cache_clean(struct objcache *cache);
extern void cache_assert(struct objcache *cache);
extern void halloc_assert(void);

#endif /* ! _OBJALLOC_H */