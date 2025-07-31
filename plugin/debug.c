/*--------------------------------------------------------------------*/
/*--- LibSIG                                                       ---*/
/*---                                                      debug.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of LibSIG, a dynamic library signature tool.

   Copyright (C) 2025, Andrei Rimsa (andrei@cefetmg.br)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

#include "global.h"

/* If debugging mode of, dummy functions are provided (see below)
 */
#if LSG_ENABLE_DEBUG

#if LSG_DEBUG_MEM
void* LSG_(malloc)(const HChar* cc, UWord s, const HChar* f) {
	void* p;

	LSG_UNUSED(cc);

	LSG_DEBUG(3, "Malloc(%lu) in %s: ", s, f);
	p = VG_(malloc)(cc, s);
	LSG_DEBUG(3, "%p\n", p);
	return p;
}

void* LSG_(realloc)(const HChar* cc, void* p, UWord s, const HChar* f) {
	LSG_UNUSED(cc);

	if (p != 0)
		LSG_DEBUG(3, "Free in %s: %p\n", f, p);

	LSG_DEBUG(3, "Malloc(%lu) in %s: ", s, f);
	p = VG_(realloc)(cc, p, s);
	LSG_DEBUG(3, "%p\n", p);
	return p;
}

void LSG_(free)(void* p, const HChar* f) {
	LSG_DEBUG(3, "Free in %s: %p\n", f, p);
	VG_(free)(p);
}

HChar* LSG_(strdup)(const HChar* cc, const HChar* s, const HChar* f) {
	HChar* p;

	LSG_UNUSED(cc);

	LSG_DEBUG(3, "Strdup(%s) in %s: ", s, f);
	p = VG_(strdup)(cc, s);
	LSG_DEBUG(3, "%p\n", p);
	return p;
}
#endif

#endif
