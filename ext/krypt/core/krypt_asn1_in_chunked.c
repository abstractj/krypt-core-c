/*
 * krypt-core API - C implementation
 *
 * Copyright (c) 2011-2013
 * Hiroshi Nakamura <nahi@ruby-lang.org>
 * Martin Bosslet <martin.bosslet@gmail.com>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "krypt-core.h"
#include "krypt_asn1-internal.h"

enum krypt_chunked_state {
    NEW_HEADER = 0,
    PROCESS_TAG,
    PROCESS_LENGTH,
    PROCESS_VALUE,
    DONE
};

typedef struct krypt_instream_chunked {
    binyo_instream_interface *methods;
    binyo_instream *inner;
    int values_only;
    enum krypt_chunked_state state;
    krypt_asn1_header *cur_header;
    binyo_instream *cur_value_stream;
    size_t header_offset;
} krypt_instream_chunked;

#define int_safe_cast(out, in)		binyo_safe_cast_instream((out), (in), KRYPT_INSTREAM_TYPE_CHUNKED, krypt_instream_chunked)

static krypt_instream_chunked* int_chunked_alloc(void);
static ssize_t int_chunked_read(binyo_instream *in, uint8_t *buf, size_t len);
static int int_chunked_seek(binyo_instream *in, off_t offset, int whence);
static void int_chunked_mark(binyo_instream *in);
static void int_chunked_free(binyo_instream *in);

static binyo_instream_interface krypt_interface_chunked = {
    KRYPT_INSTREAM_TYPE_CHUNKED,
    int_chunked_read,
    NULL,
    NULL,
    int_chunked_seek,
    int_chunked_mark,
    int_chunked_free
};

binyo_instream *
krypt_instream_new_chunked(binyo_instream *original, int values_only)
{
    krypt_instream_chunked *in;

    in = int_chunked_alloc();
    in->inner = original;
    in->values_only = values_only;
    in->state = NEW_HEADER;
    return (binyo_instream *) in;
}

static krypt_instream_chunked*
int_chunked_alloc(void)
{
    krypt_instream_chunked *ret;
    ret = ALLOC(krypt_instream_chunked);
    memset(ret, 0, sizeof(krypt_instream_chunked));
    ret->methods = &krypt_interface_chunked;
    return ret;
}

static int
int_read_new_header(krypt_instream_chunked *in)
{
    int ret;
    krypt_asn1_header *next;

    ret = krypt_asn1_next_header(in->inner, &next);
    if (ret == KRYPT_ASN1_EOF) {
	krypt_error_add("Premature end of value detected");
	return BINYO_ERR;
    }
    if (ret == KRYPT_ERR) {
        krypt_error_add("An error occured while reading the ASN.1 header");
        return BINYO_ERR;
    }

    if (in->cur_header)
	krypt_asn1_header_free(in->cur_header);
    in->cur_header = next;
    in->state = PROCESS_TAG;
    in->header_offset = 0;
    return BINYO_OK;
}

static size_t
int_read_header_bytes(krypt_instream_chunked *in,
		      uint8_t * bytes, 
		      size_t bytes_len, 
		      enum krypt_chunked_state next_state,
		      uint8_t *buf,
		      size_t len)
{
    size_t to_read;
    size_t available = bytes_len - in->header_offset;
        
    if (len < available) {
	in->header_offset += len;
	to_read = len;
    }
    else {
	in->state = next_state;
	in->header_offset = 0;
	to_read = available;
    }
    
    memcpy(buf, bytes, to_read);
    return to_read;
}

static ssize_t
int_read_value(krypt_instream_chunked *in, uint8_t *buf, size_t len)
{
    ssize_t read;

    if (!in->cur_value_stream)
	in->cur_value_stream = krypt_asn1_get_value_stream(in->inner, in->cur_header, in->values_only);

    read = binyo_instream_read(in->cur_value_stream, buf, len);
    if (read == BINYO_ERR) return BINYO_ERR;

    if (read == BINYO_IO_EOF) {
	if (in->state != DONE)
	    in->state = NEW_HEADER;
	binyo_instream_free(in->cur_value_stream);
	in->cur_value_stream = NULL;
	read = 0;
    }

    return read;
}

/* If state is PROCESS_VALUE, this means that the tag bytes
 * have been consumed. As an EOC contains no value, we are
 * done.
 */
#define int_check_done(in)					\
do {								\
    if ((in)->cur_header->tag == TAGS_END_OF_CONTENTS &&	\
	(in)->cur_header->tag_class == TAG_CLASS_UNIVERSAL &&   \
	(in)->state == PROCESS_VALUE) {				\
	(in)->state = DONE;					\
    }								\
} while (0)

/* TODO: check overflow */
static ssize_t
int_read_single_element(krypt_instream_chunked *in, uint8_t *buf, size_t len)
{
    ssize_t read = 0;
    size_t total = 0;

#define add_header_bytes()			\
do {						\
   if (!in->values_only) {			\
      total += read;				\
      if (total == len || in->state == DONE)	\
          return (ssize_t) total;		\
      if (total > len) return BINYO_ERR;	\
      buf += read;				\
   }						\
} while (0)

    switch (in->state) {
	case NEW_HEADER:
	    if (!int_read_new_header(in))
	       return BINYO_ERR;
    	    /* fallthrough */
	case PROCESS_TAG: 
	    read = int_read_header_bytes(in,
		    			 in->cur_header->tag_bytes,
					 in->cur_header->tag_len,
					 PROCESS_LENGTH, 
					 buf,
					 len);
	    add_header_bytes();
	    /* fallthrough */
	case PROCESS_LENGTH:
	    read = int_read_header_bytes(in,
		    			 in->cur_header->length_bytes,
					 in->cur_header->length_len,
					 PROCESS_VALUE,
					 buf,
					 len);
	    int_check_done(in);
	    add_header_bytes();
	    /* fallthrough */
	case PROCESS_VALUE:
	    read = int_read_value(in, buf, len);
	    if (read == BINYO_ERR) return BINYO_ERR;
	    total += read;
	    buf += read;
	    return (ssize_t) total;
	default:
	    krypt_error_add("Internal error");
	    return BINYO_ERR; /* dummy */
    }
}

static ssize_t
int_read(krypt_instream_chunked *in, uint8_t *buf, size_t len)
{
    ssize_t read = 0;
    size_t total = 0;

    while (total != len && in->state != DONE) {
	read = int_read_single_element(in, buf, len);
	if (read == BINYO_ERR) return BINYO_ERR;
	if (total > (size_t) (SSIZE_MAX - read)) {
	    krypt_error_add("Stream too large");
	    return BINYO_ERR;
	}
	total += read;
	buf += read;
    }
    return total;
}

static ssize_t
int_chunked_read(binyo_instream *instream, uint8_t *buf, size_t len)
{
    krypt_instream_chunked *in;
    
    int_safe_cast(in, instream);
    
    if (!buf) return BINYO_ERR;
    if (in->state == DONE)
	return BINYO_IO_EOF;

    return int_read(in, buf, len);
}

static int
int_chunked_seek(binyo_instream *instream, off_t offset, int whence)
{
    /* int_instream_chunked *in;

    int_safe_cast(in, instream); */

    return BINYO_OK;
    /* TODO */
}

static void
int_chunked_mark(binyo_instream *instream)
{
    krypt_instream_chunked *in;

    if (!instream) return;
    int_safe_cast(in, instream);
    binyo_instream_mark(in->inner);
}

static void
int_chunked_free(binyo_instream *instream)
{
    krypt_instream_chunked *in;

    if (!instream) return;
    int_safe_cast(in, instream);
    if (in->cur_header)
	krypt_asn1_header_free(in->cur_header);
}

