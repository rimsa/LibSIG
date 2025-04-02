/*--------------------------------------------------------------------*/
/*--- LibSIG                                                       ---*/
/*---                                                    symbols.c ---*/
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

#define DEFAULT_POOL_SIZE 4096 // 4k symbols

typedef struct _symbols_hash symbols_hash;
struct _symbols_hash {
	UInt size, entries;
	UniqueSymbol** table;
};

symbols_hash pool;

static
void delete_symbol(UniqueSymbol* symbol) {
	LSG_ASSERT(symbol != 0);

	if (symbol->name)
		LSG_FREE(symbol->name);

	LSG_DATA_FREE(symbol, sizeof(UniqueSymbol));
}

static
HChar* next_line(Int fd) {
	Int s, idx;
	HChar c;
	static HChar buffer[1024];

	idx = 0;
	VG_(memset)(&buffer, 0, sizeof(buffer));

	while (True) {
		LSG_ASSERT(idx >= 0 && idx < ((sizeof(buffer) / sizeof(HChar))-1));
		s = VG_(read)(fd, &c, 1);
		if (s == 0 || c == '\n')
			break;

		// Ignore carriage returns.
		if (c == '\r')
			continue;

		buffer[idx++] = c;
	}

	return idx > 0 ? buffer : 0;
}

static
void read_symbol_names(void) {
	Int fd;
	Addr addr;
	HChar* line;
	HChar* name;
	UniqueSymbol* symbol;

	if (LSG_(clo).symbols_file) {
		fd = VG_(fd_open)(LSG_(clo).symbols_file, VKI_O_RDONLY, 0);
		if (fd < 0)
			tl_assert(0);

		while ((line = next_line(fd))) {
			name = VG_(strchr)(line, ',');
			if (name == 0)
				continue;
			*name = 0;
			++name;

			addr = VG_(strtoull16)(line, 0);
			if (addr != 0 && *name != 0) {
				symbol = LSG_(get_symbol)(addr);
				symbol->name = LSG_STRDUP("lsg.symbols.rsn.1", name);
			}
		}

		VG_(close)(fd);
	}
}

static __inline__
UInt symbols_hash_idx(Addr addr, UInt size) {
	return addr % size;
}

static
void resize_symbols_pool(void) {
	Int i, new_size, conflicts1 = 0;
	UniqueSymbol **new_table, *curr, *next;
	UInt new_idx;

	// increase table by 50%.
	new_size  = (Int) (1.5f * pool.size);
	new_table = (UniqueSymbol**) LSG_MALLOC("lsg.symbols.rsp.1",
					(new_size * sizeof(UniqueSymbol*)));
	VG_(memset)(new_table, 0, (new_size * sizeof(UniqueSymbol*)));

	for (i = 0; i < pool.size; i++) {
		if (pool.table[i] == 0)
			continue;

		curr = pool.table[i];
		while (curr != 0) {
			next = curr->chain;

			new_idx = symbols_hash_idx(curr->addr, new_size);

			curr->chain = new_table[new_idx];
			new_table[new_idx] = curr;
			if (curr->chain)
				conflicts1++;

			curr = next;
		}
	}

	LSG_FREE(pool.table);

	LSG_DEBUG(0, "Resize symbols pool: %u => %d (entries %u, conflicts %d)\n",
				pool.size, new_size,
				pool.entries, conflicts1);

	pool.size  = new_size;
	pool.table = new_table;
}

static
UniqueSymbol* lookup_symbol(Addr addr) {
	UniqueSymbol* symbol;
	UInt idx;

	LSG_ASSERT(addr != 0);

	idx = symbols_hash_idx(addr, pool.size);
	symbol = pool.table[idx];

	while (symbol) {
		if (symbol->addr == addr)
			break;

		symbol = symbol->chain;
	}

	return symbol;
}

void LSG_(init_symbols_pool)(void) {
	Int size;

	pool.size = DEFAULT_POOL_SIZE;
	pool.entries = 0;

	size = pool.size * sizeof(UniqueSymbol*);
	pool.table = (UniqueSymbol**) LSG_MALLOC("lsg.symbols.isp.1", size);
	VG_(memset)(pool.table, 0, size);

	// read symbols names.
	read_symbol_names();
}

void LSG_(destroy_symbols_pool)(void) {
	Int i;

	for (i = 0; i < pool.size; i++) {
		UniqueSymbol* symbol = pool.table[i];
		while (symbol) {
			UniqueSymbol* next = symbol->chain;
			delete_symbol(symbol);
			symbol = next;

			pool.entries--;
		}
	}

	LSG_ASSERT(pool.entries == 0);

	LSG_FREE(pool.table);
	pool.table = 0;
}

UniqueSymbol* LSG_(get_symbol)(Addr addr) {
	UniqueSymbol* symbol = lookup_symbol(addr);
	if (symbol) {
		LSG_ASSERT(symbol->addr == addr);
	} else {
		UInt idx;

		/* check fill degree of symbols pool and resize if needed (>80%) */
		pool.entries++;
		if (10 * pool.entries / pool.size > 8)
			resize_symbols_pool();

		// Create the symbol.
		symbol = (UniqueSymbol*) LSG_MALLOC("lsg.symbols.gs.1", sizeof(UniqueSymbol));
		VG_(memset)(symbol, 0, sizeof(UniqueSymbol));
		symbol->addr = addr;

		/* insert into symbols pool */
		idx = symbols_hash_idx(addr, pool.size);
		symbol->chain = pool.table[idx];
		pool.table[idx] = symbol;
	}

	return symbol;
}

UniqueSymbol* LSG_(find_symbol)(Addr addr) {
	return lookup_symbol(addr);
}

Addr LSG_(symbol_addr)(UniqueSymbol* symbol) {
	LSG_ASSERT(symbol != 0);
	return symbol->addr;
}

const HChar* LSG_(symbol_name)(UniqueSymbol* symbol) {
	LSG_ASSERT(symbol != 0);
	return symbol->name;
}

Bool LSG_(symbols_cmp)(UniqueSymbol* i1, UniqueSymbol* i2) {
	return i1 && i2 && i1->addr == i2->addr;
}
