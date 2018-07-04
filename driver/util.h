/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __UTIL_H__
#define __UTIL_H__

/* NOTE: this must be included in "top level" file (wherever SQL types are
 * used  */
#if defined(_WIN32) || defined (WIN32)
/* FIXME: why isn't this included in sql/~ext.h? */
/* win function parameter attributes */
#include <windows.h>
#include <tchar.h>
#endif /* _WIN32/WIN32 */

#include <inttypes.h>
#include <wchar.h>
#include <assert.h>

#include "sql.h"
#include "sqlext.h"

/* export attribute for internal functions used for testing */
#ifndef TEST_API /* Release builds define this to an empty macro */
#ifdef DRIVER_BUILD
#define TEST_API	__declspec(dllexport)
#else /* _EXPORTS */
#define TEST_API	__declspec(dllimport)
#endif /* _EXPORTS */
#define TESTING		/* compiles in the testing code */
#endif /* TEST_API */

/*
 * Assert two integral types have same storage and sign.
 */
#define ASSERT_INTEGER_TYPES_EQUAL(a, b) \
	assert((sizeof(a) == sizeof(b)) && \
		( (0 < (a)0 - 1 && 0 < (b)0 - 1) || \
			(0 > (a)0 - 1 && 0 > (b)0 - 1) ))

/*
 * Stringifying in two preproc. passes
 */
#define _STR(_x)	# _x
#define STR(_x)		_STR(_x)

/*
 * Turn a C static string to wide char string.
 */
#define _MK_WPTR(_cstr_)	(L ## _cstr_)
#define MK_WPTR(_cstr_)		_MK_WPTR(_cstr_)

#define MK_CPTR(_cstr_)		_cstr_

#ifdef UNICODE
#define MK_TPTR		MK_WPTR
#else /* UNICODE */
#define MK_TPTR		MK_CPTR
#endif /* UNICODE */

#if !defined(_WIN32) && !defined (WIN32)
#define _T			MK_TPTR
#endif /* _WIN32/WIN32 */


typedef struct cstr {
	SQLCHAR *str;
	size_t cnt;
} cstr_st;

/*
 * Trims leading and trailing WS of a wide string of 'chars' length.
 * 0-terminator should not be counted (as it's a non-WS).
 */
const SQLWCHAR *wtrim_ws(const SQLWCHAR *wstr, size_t *chars);
const SQLCHAR *trim_ws(const SQLCHAR *cstr, size_t *chars);
/*
 * Copy converted strings from SQLWCHAR to char, for ANSI strings.
 */
int ansi_w2c(const SQLWCHAR *src, char *dst, size_t chars);
/*
 * Compare two SQLWCHAR object, case INsensitive.
 */
int wmemncasecmp(const SQLWCHAR *a, const SQLWCHAR *b, size_t len);
/*
 * Compare two zero-terminated SQLWCHAR* objects, until a 0 is encountered in
 * either of them or until 'count' characters are evaluated. If 'count'
 * parameter is negative, it is ignored.
 *
 * This is useful in comparing SQL strings which the API allows to be passed
 * either as 0-terminated or not (SQL_NTS).
 * The function does a single pass (no length evaluation of the strings).
 * wmemcmp() might read over the boundary of one of the objects, if the
 * provided 'count' paramter is not the minimum of the strings' length.
 */
int wszmemcmp(const SQLWCHAR *a, const SQLWCHAR *b, long count);

/*
 * wcsstr() variant for non-NTS.
 */
const SQLWCHAR *wcsnstr(const SQLWCHAR *hay, size_t len, SQLWCHAR needle);

typedef struct wstr {
	SQLWCHAR *str;
	size_t cnt;
} wstr_st;

/*
 * Turns a static C string into a wstr_st.
 */
#ifndef __cplusplus /* no MSVC support for compound literals with /TP */
#	define MK_WSTR(_s)	((wstr_st){.str = MK_WPTR(_s), .cnt = sizeof(_s) - 1})
#	define MK_CSTR(_s)	((cstr_st){.str = _s, .cnt = sizeof(_s) - 1})
#else /* !__cplusplus */
#	define WSTR_INIT(_s)	{MK_WPTR(_s), sizeof(_s) - 1}
#	define CSTR_INIT(_s)	{(SQLCHAR *)_s, sizeof(_s) - 1}
#endif /* !__cplusplus */
/*
 * Test equality of two wstr_st objects.
 */
#define EQ_WSTR(s1, s2) \
	((s1)->cnt == (s2)->cnt && wmemcmp((s1)->str, (s2)->str, (s1)->cnt) == 0)
/*
 * Same as EQ_WSTR, but case INsensitive.
 */
#define EQ_CASE_WSTR(s1, s2) \
	((s1)->cnt == (s2)->cnt && \
		wmemncasecmp((s1)->str, (s2)->str, (s1)->cnt) == 0)

BOOL wstr2bool(wstr_st *val);
BOOL wstr2ullong(wstr_st *val, unsigned long long *out);
BOOL wstr2llong(wstr_st *val, long long *out);

/* converts the int types to a C or wide string, returning the string length */
size_t i64tot(int64_t i64, void *buff, BOOL wide);
size_t ui64tot(uint64_t ui64, void *buff, BOOL wide);


#ifdef _WIN32
/*
 * "[D]oes not null-terminate an output string if the input string length is
 * explicitly specified without a terminating null character. To
 * null-terminate an output string for this function, the application should
 * pass in -1 or explicitly count the terminating null character for the input
 * string."
 * "If successful, returns the number of bytes written" or required (if
 * _ubytes == 0), OR "0 if it does not succeed".
 */
#define WCS2U8(_wstr, _wchars, _u8, _ubytes) \
	WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, \
		_wstr, _wchars, _u8, _ubytes, \
		NULL, NULL)
#define WCS2U8_BUFF_INSUFFICIENT \
	(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
#define WCS2U8_ERRNO() GetLastError()

#else /* _WIN32 */
#error "unsupported platform" /* TODO */
/* "[R]eturns the number of bytes written into the multibyte output
 * string, excluding the terminating NULL (if any)".  Copies until \0 is
 * met in wstr or buffer runs out.  If \0 is met, it's copied, but not
 * counted in return. (silly fn) */
/* "[T]he multibyte character string at mbstr is null-terminated
 * only if wcstombs encounters a wide-character null character
 * during conversion." */
// wcstombs(charp, wstr, octet_length);
#endif /* _WIN32 */

#ifdef UNICODE
typedef wstr_st tstr_st;
#else /* UNICODE */
typedef cstr_st tstr_st;
#endif /* UNICODE */


/*
 * JSON-escapes a string.
 * If string len is 0, it assumes a NTS.
 * If output buffer (jout) is NULL, it returns the buffer size needed for
 * escaping.
 * Returns number of used bytes in buffer (which might be less than buffer
 * size, if some char needs an escaping longer than remaining space).
 */
size_t json_escape(const char *jin, size_t inlen, char *jout, size_t outlen);

/*
 * Copy a WSTR back to application.
 * The WSTR must not count the 0-tem.
 * The function checks against the correct size of available bytes, copies the
 * wstr according to avaialble space and indicates the available bytes to copy
 * back into provided buffer (if not NULL).
 */
SQLRETURN write_wstr(SQLHANDLE hnd, SQLWCHAR *dest, wstr_st *src,
	SQLSMALLINT /*B*/avail, SQLSMALLINT /*B*/*usedp);

/*
 * Printing aids.
 */

/*
 * w/printf() desriptors for char/wchar_t *
 * "WPrintF Wide/Char Pointer _ DESCriptor"
 */
#ifdef _WIN32
/* funny M$ 'inverted' logic */
/* wprintf wide_t pointer descriptor */
#	define WPFWP_DESC		L"%s"
#	define WPFWP_LDESC		L"%.*s"
/* printf wide_t pointer descriptor */
#	define PFWP_DESC		"%S"
#	define PFWP_LDESC		"%.*S"
/* wprintf char pointer descriptor */
#	define WPFCP_DESC		L"%S"
#	define WPFCP_LDESC		L"%.*S"
/* printf char pointer descriptor */
#	define PFCP_DESC		"%s"
#	define PFCP_LDESC		"%.*s"
#else /* _WIN32 */
/* wprintf wide_t pointer descriptor */
#	define WPFWP_DESC		L"%S"
#	define WPFWP_LDESC		L"%.*S"
/* printf wide_t pointer descriptor */
#	define PFWP_DESC		"%S"
#	define PFWP_LDESC		"%.*S"
/* wprintf char pointer descriptor */
#	define WPFCP_DESC		L"%s"
#	define WPFCP_LDESC		L"%.*s"
/* printf char pointer descriptor */
#	define PFCP_DESC		"%s"
#	define PFCP_LDESC		"%.*s"
#endif /* _WIN32 */


/* SNTPRINTF: function to printf into a string */
#ifdef UNICODE
#define SNTPRINTF	_snwprintf
#define TSTRCHR		wcschr
#else /* UNICODE */
#define SNTPRINTF	snprintf
#define TSTRCHR		strchr
#endif /* UNICODE */


/*
 * Descriptors to be used with STPRINTF for TCHAR pointer type.
 * "SNTPRINTF Tchar Pointer Descriptor [with Length]"
 */
#ifdef UNICODE
#define STPD	WPFWP_DESC
#define STPDL	WPFWP_LDESC
#else /* UNICODE */
#define STPD	PFCP_DESC
#define STPDL	PFCP_LDESC
#endif /* UNICODE */



#endif /* __UTIL_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
