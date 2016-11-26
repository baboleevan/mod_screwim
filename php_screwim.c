/*
 * Copuright (c) 2016 JoungKyun.Kim <http://oops.org>. All rights reserved.
 *
 * This file is part of mod_screwim
 *
 * This program is forked from PHP screw who made by Kunimasa Noda in
 * PM9.com, Inc. and, his information is follows:
 *   http://www.pm9.com
 *   kuni@pm9.com
 *   https://sourceforge.net/projects/php-screw/
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/file.h"
#include "ext/standard/info.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef unsigned long  ULong; /* 32 bits or more */

#include "php_screwim.h"

ZEND_DECLARE_MODULE_GLOBALS(screwim)

static void php_screwim_init_globals(zend_screwim_globals *screwim_globals)
{
	screwim_globals->enabled = 0;
}

typedef struct screw_data {
	char * buf;
	size_t len;
} SCREWData;

SCREWData screwdata_init (void) {
	SCREWData data;

	data.buf = NULL;
	data.len = 0;

	return data;
}

SCREWData screwim_ext_buf (char * datap, ULong datalen) {
	int         cryptkey_len;
	SCREWData   sdata;
	int         i;

	short screwim_mycryptkey[] = {
		SCREWIM_ENC_DATA
	};

	cryptkey_len = sizeof (screwim_mycryptkey) / sizeof (short);

	for( i=0; i<datalen; i++ ) {
		datap[i] = (char) screwim_mycryptkey[(datalen - i) % cryptkey_len] ^ (~(datap[i]));
	}

	sdata = screwdata_init ();
	sdata.buf = zdecode (datap, datalen, &sdata.len);

	return sdata;
}

SCREWData screwim_ext_fopen (FILE * fp)
{
	struct      stat stat_buf;
	SCREWData   sdata;
	char      * datap = NULL;
	ULong       datalen;

	fstat (fileno (fp), &stat_buf);
	datalen = stat_buf.st_size - SCREWIM_LEN;
	datap = (char *) emalloc (datalen);
	memset (datap, 0, datalen);
	fread (datap, datalen, 1, fp);
	fclose (fp);

	sdata = screwim_ext_buf (datap, datalen);
	efree (datap);

	return sdata;
}

SCREWData screwim_ext_mmap (zend_file_handle * file_handle) {
	SCREWData   sdata;
	char      * nbuf;
	char      * datap = NULL;
	ULong       datalen;

	nbuf = file_handle->handle.stream.mmap.buf + SCREWIM_LEN;
	datalen = file_handle->handle.stream.mmap.len - SCREWIM_LEN; 

	datap = (char *) emalloc (datalen);
	memset (datap, 0, datalen);
	memcpy (datap, nbuf, datalen);

	sdata = screwim_ext_buf (datap, datalen);
	efree (datap);

	return sdata;
}

ZEND_API zend_op_array *(*org_compile_file)(zend_file_handle * file_handle, int type TSRMLS_DC);

ZEND_API zend_op_array * screwim_compile_file (zend_file_handle * file_handle, int type TSRMLS_DC)
{
	FILE      * fp;
	char        buf[SCREWIM_LEN + 1] = { 0, };
	char        fname[32] = { 0, };
	SCREWData   sdata, tmp;

	if ( ! SCREWIM_G (enabled) )
		return org_compile_file (file_handle, type TSRMLS_CC);

	// If file_handle->type is ZEND_HANDLE_MAPPED, handle.stream.mmap.buf has already
	// contents data. This case is include or require. when file is directly opened,
	// handle.stream.mmap.buf has NULL.
	if ( file_handle->type == ZEND_HANDLE_MAPPED ) {
		if ( file_handle->handle.stream.mmap.len > 0 ) {
			memcpy (buf, file_handle->handle.stream.mmap.buf, SCREWIM_LEN);
			if ( memcmp (buf, SCREWIM, SCREWIM_LEN) != 0 )
				return org_compile_file (file_handle, type TSRMLS_CC);
		} else
			return org_compile_file (file_handle, type TSRMLS_CC);
	}

	if ( zend_is_executing (TSRMLS_C) ) {
		if ( get_active_function_name (TSRMLS_C) ) {
			strncpy (fname, get_active_function_name (TSRMLS_C), sizeof (fname - 2));
		}
	}
	if ( fname[0] ) {
		if ( strcasecmp (fname, "show_source") == 0 || strcasecmp (fname, "highlight_file") == 0) {
			return NULL;
		}
	}

	if ( file_handle->type == ZEND_HANDLE_MAPPED ) {
		sdata = screwim_ext_mmap (file_handle);
	} else {
		// When file is opened directly (type is ZEND_HANDLE_FP), check here.

		fp = fopen (file_handle->filename, "rb");
		if ( ! fp ) {
			return org_compile_file (file_handle, type TSRMLS_CC);
		}

		fread (buf, SCREWIM_LEN, 1, fp);

		if ( memcmp (buf, SCREWIM, SCREWIM_LEN) != 0 ) {
			fclose (fp);
			return org_compile_file (file_handle, type TSRMLS_CC);
		}

		sdata = screwim_ext_fopen (fp);
	}

	tmp = screwdata_init ();

	if ( sdata.buf == NULL )
		return NULL;

	if ( zend_stream_fixup (file_handle, &tmp.buf, &tmp.len TSRMLS_CC) == FAILURE )
		return NULL;

	file_handle->handle.stream.mmap.buf = sdata.buf;
	file_handle->handle.stream.mmap.len = sdata.len;
	file_handle->handle.stream.closer = NULL;

	return org_compile_file (file_handle, type TSRMLS_CC);
}

zend_module_entry screwim_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"screwim",
	NULL,
	PHP_MINIT(screwim),
	PHP_MSHUTDOWN(screwim),
	NULL,
	NULL,
	PHP_MINFO(screwim),
#if ZEND_MODULE_API_NO >= 20010901
	SCREWIM_VERSION, /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SCREWIM
ZEND_GET_MODULE (screwim);
#endif

PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("screwim.enable",  "0", PHP_INI_ALL, OnUpdateBool, enabled, zend_screwim_globals, screwim_globals)
PHP_INI_END()

PHP_MINFO_FUNCTION (screwim)
{
	php_info_print_table_start ();
	php_info_print_table_colspan_header (2, "PHP SCREW Imporved support");
	php_info_print_table_row (2, "Summary", "PHP script encryption tool");
	php_info_print_table_row (2, "URL", "http://github.com/OOPS-ORG-PHP/mod_screwim");
	php_info_print_table_row (2, "Build version", SCREWIM_VERSION);
	php_info_print_table_end ();
}

PHP_MINIT_FUNCTION (screwim)
{
	CG(compiler_options) |= ZEND_COMPILE_EXTENDED_INFO;

	ZEND_INIT_MODULE_GLOBALS(screwim, php_screwim_init_globals, NULL);
	REGISTER_INI_ENTRIES();

	org_compile_file  = zend_compile_file;
	zend_compile_file = screwim_compile_file;
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(screwim)
{
	CG(compiler_options) |= ZEND_COMPILE_EXTENDED_INFO;

	zend_compile_file = org_compile_file;
	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
