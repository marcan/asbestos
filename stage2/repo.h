/*  repo.h - convenience macros and defines for lv1 repo

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef REPO_H
#define REPO_H

#include "types.h"


#define _PS(s) (s "\0\0\0\0\0\0\0\0")
#define S2I(s) ( \
	(((u64)_PS(s)[0])<<56) | \
	(((u64)_PS(s)[1])<<48) | \
	(((u64)_PS(s)[2])<<40) | \
	(((u64)_PS(s)[3])<<32) | \
	(((u64)_PS(s)[4])<<24) | \
	(((u64)_PS(s)[5])<<16) | \
	(((u64)_PS(s)[6])<<8) | \
	(((u64)_PS(s)[7])<<0))

#define PS3_LPAR_ID_PME 1

#define FIELD_FIRST(s, i) ((S2I(s)>>32) + (i))
#define FIELD(s, i) (S2I(s) + (i))

#endif
