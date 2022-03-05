// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2019-2022 Cyril Hrubis <metan@ucw.cz>
 */
#ifndef MT_COMMON__
#define MT_COMMON__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MT_ERROR(str) do { \
	fprintf(stderr, "%s: %i: "str"\n", __FILE__, __LINE__); \
	abort(); \
} while (0)

#define MT_ERROR_MALLOC MT_ERROR("Malloc failed :(")

typedef int mt_coord;

/*
 * Swap a and b using an intermediate variable
 */
#define MT_SWAP(a, b) do { \
	typeof(a) tmp = b; \
	b = a;             \
	a = tmp;           \
} while (0)

#define MT_MIN(a, b) ({ \
	typeof(a) _a = (a); \
	typeof(b) _b = (b); \
	_a < _b ? _a : _b; \
})

#define MT_MAX(a, b) ({ \
	typeof(a) _a = (a); \
	typeof(b) _b = (b); \
	_a > _b ? _a : _b; \
})

#endif /* MT_COMMON__ */
