/*--------------------------------------------------------------------*/
/*--- LibSIG                                                       ---*/
/*---                                                        clo.c ---*/
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

#include "config.h" // for VG_PREFIX

#include "global.h"



/*------------------------------------------------------------*/
/*--- Function specific configuration options              ---*/
/*------------------------------------------------------------*/

#define CONFIG_DEFAULT -1
#define CONFIG_FALSE    0
#define CONFIG_TRUE     1

/*--------------------------------------------------------------------*/
/*--- Command line processing                                      ---*/
/*--------------------------------------------------------------------*/

Bool LSG_(process_cmd_line_option)(const HChar* arg) {
	const HChar* opt;

	if VG_STR_CLO(arg, "--bound", opt) {
		Addr addr;
		SizeT length;
		HChar* tmp;

		if ((tmp = VG_(strchr)(opt, '+'))) {
			*tmp = 0;
			length = VG_(strtoll10)(++tmp, 0);
		} else {
			length = 1;
		}
		LSG_ASSERT(length > 0);

		addr = VG_(strtoull16)(opt, 0);
		LSG_ASSERT(addr > 0);

		LSG_(clo).bound_range.start = addr;
		LSG_(clo).bound_range.end = addr + length;
	}
	else if VG_STR_CLO(arg, "--records", LSG_(clo).records_file) {}
#if LSG_ENABLE_DEBUG
	else if VG_INT_CLO(arg, "--ct-verbose", LSG_(clo).verbose) {}
#endif
	else
		return False;

	return True;
}

void LSG_(print_usage)(void) {
	VG_(printf)(
"\n   library signature options:\n"
"    --bound=<address>[+<length>]   Define a range bound to track; if not defined, use the\n"
"                                   text section of the program as the bounding range\n"
"                                   (This option can not be used multiple times)\n"
"    --records=<f>                  The output file with all recorded bounds\n"
"                                   (Use %%p to bind the pid to the file, e.g.: records-%%p.out)\n"
	);
}

void LSG_(print_debug_usage)(void) {
	VG_(printf)(
#if LSG_ENABLE_DEBUG
	"    --ct-verbose=<level>       Verbosity of standard debug output [0]\n"
#else
	"    (none)\n"
#endif
	);
}

void LSG_(set_clo_defaults)(void) {
	/* Default values for command line arguments */
	LSG_(clo).bound_range.start = 0;
	LSG_(clo).bound_range.end = 0;

	LSG_(clo).records_file = 0;

#if LSG_ENABLE_DEBUG
	LSG_(clo).verbose = 0;
#endif
}
