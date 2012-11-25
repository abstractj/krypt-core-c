/*
* krypt-core API - C version
*
* Copyright (C) 2011
* Hiroshi Nakamura <nahi@ruby-lang.org>
* Martin Bosslet <martin.bosslet@googlemail.com>
* All rights reserved.
*
* This software is distributed under the same license as Ruby.
* See the file 'LICENSE' for further details.
*/

#include "krypt-core.h"

typedef struct krypt_instream_definite_st {
    krypt_instream_interface *methods;
    krypt_instream *inner;
    size_t max_read;
    size_t num_read;
} krypt_instream_definite;

#define int_safe_cast(out, in)		krypt_safe_cast_instream((out), (in), KRYPT_INSTREAM_TYPE_DEFINITE, krypt_instream_definite)

static krypt_instream_definite* int_definite_alloc(void);
static ssize_t int_definite_read(krypt_instream *in, uint8_t *buf, size_t len);
static int int_definite_seek(krypt_instream *in, off_t offset, int whence);
static void int_definite_mark(krypt_instream *in);
static void int_definite_free(krypt_instream *in);

static krypt_instream_interface krypt_interface_definite = {
    KRYPT_INSTREAM_TYPE_DEFINITE,
    int_definite_read,
    NULL,
    NULL,
    int_definite_seek,
    int_definite_mark,
    int_definite_free
};

krypt_instream *
krypt_instream_new_definite(krypt_instream *original, size_t len)
{
    krypt_instream_definite *in;

    in = int_definite_alloc();
    in->inner = original;
    in->max_read = len;
    return (krypt_instream *) in;
}

static krypt_instream_definite*
int_definite_alloc(void)
{
    krypt_instream_definite *ret;
    ret = ALLOC(krypt_instream_definite);
    memset(ret, 0, sizeof(krypt_instream_definite));
    ret->methods = &krypt_interface_definite;
    return ret;
}

static ssize_t
int_definite_read(krypt_instream *instream, uint8_t *buf, size_t len)
{
    krypt_instream_definite *in;
    size_t to_read;
    ssize_t r;
    
    int_safe_cast(in, instream);

    if (!buf) return 0;

    if (in->num_read == in->max_read)
	return -1;

    if (in->max_read - in->num_read < len)
	to_read = in->max_read - in->num_read;
    else
	to_read = len;

    r = krypt_instream_read(in->inner, buf, to_read);
    if (r <= -1) return -2;
    if (in->num_read >= SIZE_MAX - r) {
	krypt_error_add("Stream too large");
	return -2;
    }

    in->num_read += r;
    return r;
}

static int
int_definite_seek(krypt_instream *instream, off_t offset, int whence)
{
    off_t real_off;
    long numread;
    krypt_instream_definite *in;

    int_safe_cast(in, instream);

    switch (whence) {
	case SEEK_CUR:
	    real_off = offset;
	    break;
	case SEEK_SET:
	    real_off =  offset - in->num_read;
	    break;
	case SEEK_END:
	    real_off = offset + in->max_read - in->num_read;
	    break;
	default:
	    krypt_error_add("Unknown whence: %d", whence);
	    return 0;
    }
    
    numread = in->num_read;
    if (numread + real_off < 0 || numread + real_off >= (long)in->max_read) {
	krypt_error_add("Invalid seek position: %ld", numread + real_off);
	return 0;
    }

    return krypt_instream_seek(in->inner, offset, whence);
}

static void
int_definite_mark(krypt_instream *instream)
{
    krypt_instream_definite *in;

    if (!instream) return;
    int_safe_cast(in, instream);
    krypt_instream_mark(in->inner);
}


static void
int_definite_free(krypt_instream *instream)
{
    /* do not free the inner stream */
}

