// Copyright 2018-2019 - Omar Sandoval
// SPDX-License-Identifier: GPL-3.0+

/**
 * @file
 *
 * Miscellanous internal helpers.
 *
 * Several of these are taken from the Linux kernel source.
 */

#ifndef DRGN_INTERNAL_H
#define DRGN_INTERNAL_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <elfutils/libdw.h>
#include <elfutils/version.h>

#include "drgn.h"
#include "error.h"

/**
 *
 * @defgroup Internals Internals
 *
 * Internal implementation.
 *
 * @{
 */

#ifndef LIBDRGN_PUBLIC
#define LIBDRGN_PUBLIC __attribute__((visibility("default")))
#endif

_Static_assert(sizeof(off_t) == 8 || sizeof(off_t) == 4,
	       "off_t is not 64 bits or 32 bits");
#define OFF_MAX (sizeof(off_t) == 8 ? INT64_MAX : INT32_MAX)

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#define __compiletime_error(message) __attribute__((__error__(message)))
#else
#define __compiletime_error(message)
#endif
#ifdef __OPTIMIZE__
# define __compiletime_assert(condition, msg, prefix, suffix)		\
	do {								\
		extern void prefix ## suffix(void) __compiletime_error(msg); \
		if (!(condition))					\
			prefix ## suffix();				\
	} while (0)
#else
# define __compiletime_assert(condition, msg, prefix, suffix) do { } while (0)
#endif
#define _compiletime_assert(condition, msg, prefix, suffix) \
	__compiletime_assert(condition, msg, prefix, suffix)
#define compiletime_assert(condition, msg) \
	_compiletime_assert(condition, msg, __compiletime_assert_, __LINE__)

#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:(-!!(e)); }))
#define BUILD_BUG_ON_MSG(cond, msg) compiletime_assert(!(cond), msg)

#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#define ___PASTE(a,b) a##b
#define __PASTE(a,b) ___PASTE(a,b)

#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

#define __typecheck(x, y) \
		(!!(sizeof((typeof(x) *)1 == (typeof(y) *)1)))

#define __is_constexpr(x) \
	(sizeof(int) == sizeof(*(8 ? ((void *)((long)(x) * 0l)) : (int *)8)))

#define __no_side_effects(x, y) \
	(__is_constexpr(x) && __is_constexpr(y))

#define __safe_cmp(x, y) \
	(__typecheck(x, y) && __no_side_effects(x, y))

#define __cmp(x, y, op)	((x) op (y) ? (x) : (y))

#define __cmp_once(x, y, unique_x, unique_y, op) ({	\
		typeof(x) unique_x = (x);		\
		typeof(y) unique_y = (y);		\
		__cmp(unique_x, unique_y, op); })

#define __careful_cmp(x, y, op) \
	__builtin_choose_expr(__safe_cmp(x, y), \
		__cmp(x, y, op), \
		__cmp_once(x, y, __UNIQUE_ID(__x), __UNIQUE_ID(__y), op))

#define min(x, y)	__careful_cmp(x, y, <)

#define max(x, y)	__careful_cmp(x, y, >)

#define __must_be_array(a)	BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	BUILD_BUG_ON_MSG(!__same_type(*(ptr), ((type *)0)->member) &&	\
			 !__same_type(*(ptr), void),			\
			 "pointer type mismatch in container_of()");	\
	((type *)(__mptr - offsetof(type, member))); })

#define __bitop(x, unique_x, op) ({					\
	__auto_type unique_x = (x);					\
									\
	(unsigned int)(sizeof(unique_x) <= sizeof(unsigned int) ?	\
		       __builtin_##op(unique_x) :			\
		       sizeof(unique_x) <= sizeof(unsigned long) ?	\
		       __builtin_##op##l(unique_x) :			\
		       __builtin_##op##ll(unique_x));			\
})

#define clz(x) __bitop(x, __UNIQUE_ID(__x), clz)
#define ctz(x) __bitop(x, __UNIQUE_ID(__x), ctz)

#define __fls(x, unique_x) ({						\
	__auto_type unique_x = (x);					\
									\
	unique_x ? 1U + ((8U * max(sizeof(unique_x),			\
				   sizeof(unsigned int)) - 1U) ^	\
			 clz(unique_x)) : 0U;				\
})

#define fls(x) __fls(x, __UNIQUE_ID(__x))

#define __next_power_of_two(x, unique_x) ({			\
	__auto_type unique_x = (x);				\
								\
	unique_x ? (typeof(unique_x))1 << fls(unique_x - 1) :	\
	(typeof(unique_x))1;					\
})

#define next_power_of_two(x) __next_power_of_two(x, __UNIQUE_ID(__x))

#define for_each_bit(i, mask)	\
	for (i = -1; mask && (i = ctz(mask), mask &= mask - 1, 1);)

void *realloc_array(void *ptr, size_t nmemb, size_t size);
void *malloc_array(size_t nmemb, size_t size);

/* bool resize_array(T **ptr, size_t n); */
#define resize_array(ptr, n) ({					\
	__auto_type _ptr = (ptr);				\
	typeof(*_ptr) _tmp;					\
	bool _success;						\
								\
	errno = 0;						\
	_tmp = realloc_array(*_ptr, (n), sizeof(**_ptr));	\
	if ((_success = _tmp || !errno))			\
		*_ptr = _tmp;					\
	_success;						\
})

static inline void *malloc64(uint64_t size)
{
	if (size > SIZE_MAX)
		return NULL;
	return malloc(size);
}

struct drgn_error *read_elf_section(Elf_Scn *scn, Elf_Data **ret);

/**
 * Return the word size of a program based on an ELF file.
 *
 * Note that this looks at the ELF header rather than determining this based on
 * machine type, but the file format @e should correspond to the architecture
 * word size.
 */
static inline uint8_t elf_word_size(Elf *elf)
{
	return elf_getident(elf, NULL)[EI_CLASS] == ELFCLASS64 ? 8 : 4;
}

/**
 * Return the endianness of a program based on an ELF file.
 *
 * Like @ref elf_word_size(), this only looks at the ELF header.
 */
static inline bool elf_is_little_endian(Elf *elf)
{
	return elf_getident(elf, NULL)[EI_DATA] == ELFDATA2LSB;
}

bool die_matches_filename(Dwarf_Die *die, const char *filename);

/**
 * Path component iterator.
 *
 * This iterates over the components of a file path, normalizing it in the
 * process (i.e., collapsing redundant "/" separators and "." and ".."
 * components).
 *
 * In order to do this in O(n) time and O(1) space, components are emitted in @b
 * reverse. So, "a/b/c" is emitted in the order "c", "b", "a".
 *
 * Absolute paths have an implicit empty component, so "/a/b" is emitted as "b",
 * "a", "".
 *
 * Relative paths are emitted relative to a hypothetical current directory. A
 * path referring to the current directory (e.g., "." or "a/..") does not emit
 * any components.
 *
 * ".." components above the current directory are included, so "a/b/../../../c"
 * is emitted as "c", "..". However, ".." components above an absolute path are
 * not meaningful, so "/a/b/../../../c" is emitted as "c", "".
 *
 * A empty path does not emit any components.
 */
struct path_iterator {
	/** Path to iterate. */
	const char *path;
	/**
	 * Length of the portion of the path which has not been consumed.
	 *
	 * Initialize this to the length of the path (e.g.,
	 * <tt>strlen(path)</tt> if the path is null-terminated).
	 */
	size_t len;
	/**
	 * Current number of ".." components.
	 *
	 * Initialize this to 0.
	 */
	size_t dot_dot;
};

/**
 * Get the next component from a @ref path_iterator.
 *
 * Components are emitted in reverse. This will never emit a "." component. It
 * will emit an empty ("") component only for an absolute path. It may emit ".."
 * components if there are any that go above the current directory.
 *
 * @param[in] it Iterator.
 * @param[out] component Returned component.
 * @param[out] component_len Length of @c component.
 * @return @c true if we returned a componenent, @c false if there were no more
 * components.
 */
bool path_iterator_next(struct path_iterator *it, const char **component,
			size_t *component_len);

/**
 * Return whether two paths, when normalized, are equal.
 *
 * A normalized path is a path with all redundant "/" separators and "." and
 * ".." components removed. E.g., "a/./b" is equal to "a//c/../b".
 *
 * @sa path_iterator
 */
bool normalized_path_eq(const char *path1, const char *path2);

/** @} */

struct drgn_lexer;
struct drgn_token;
struct drgn_error *drgn_lexer_c(struct drgn_lexer *lexer,
				struct drgn_token *token);

#endif /* DRGN_INTERNAL_H */
