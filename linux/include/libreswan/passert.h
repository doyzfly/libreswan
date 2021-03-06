/*
 * Panic, for libreswan.
 *
 * Copyright (C) 1998-2002  D. Hugh Redelmeier.
 * Copyright (C) 2003  Michael Richardson <mcr@freeswan.org>
 * Copyright (C) 2013 Paul Wouters <paul@libreswan.org>
 * Copyright (C) 2015-2016 Andrew Cagney <cagney@gnu.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/lgpl.txt>.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 */

#include "libreswan.h"

#ifndef _LIBRESWAN_PASSERT_H
#define _LIBRESWAN_PASSERT_H
/* our versions of assert: log result */

typedef void (*libreswan_passert_fail_t)(const char *pred_str,
					 const char *file_str,
					 unsigned long line_no) NEVER_RETURNS;

extern libreswan_passert_fail_t libreswan_passert_fail;

#define impossible()  libreswan_passert_fail("impossible", __FILE__, __LINE__)

extern void libreswan_switch_fail(int n,
				  const char *file_str,
				  unsigned long line_no) NEVER_RETURNS;

#define bad_case(n) libreswan_switch_fail((int) (n), __FILE__, __LINE__)

#define passert(pred) {							\
		/* Shorter if(!(pred)) suppresses -Wparen */		\
		if (pred) {} else {					\
			libreswan_passert_fail(#pred, __FILE__,		\
					       __LINE__);		\
		}							\
	}


/*
 * Check/log a pexpect failure to the "panic" channel.
 *
 * Notes:
 *
 * According to C99, the expansion of PEXPECT_LOG(FMT) will include a
 * stray comma vis: "pexpect_log(file, line, FMT,)".  Plenty of
 * workarounds.
 *
 * "pexpect()" does use the shorter statement "if(!(pred))" in the
 * below as it will suppresses -Wparen (i.e., assignment in if
 * statement).
 */

extern void pexpect_log(const char *file_str, unsigned long line_no,
			const char *fmt, ...)  PRINTF_LIKE(3);
#define PEXPECT_LOG(FMT, ...) pexpect_log(__FILE__, __LINE__, FMT,  __VA_ARGS__)

#define pexpect(pred) {							\
		if (pred) {} else {					\
			PEXPECT_LOG("%s", #pred);			\
		}							\
	}

/* evaluate x exactly once; assert that err_t result is NULL; */
#define happy(x) { \
		err_t ugh = x; \
		if (ugh != NULL) \
			libreswan_passert_fail(ugh, __FILE__, __LINE__); \
	}

#endif /* _LIBRESWAN_PASSERT_H */
