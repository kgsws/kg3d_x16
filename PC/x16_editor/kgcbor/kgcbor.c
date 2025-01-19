//
// kgsws' simple CBOR parser
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "kgcbor.h"

#ifdef KGCBOR_ENABLE_PRASE_FILE
#ifndef WINDOWS
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#endif

enum
{
	// CBOR types
	CT_INT_POS,
	CT_INT_NEG,
	CT_DATA_BINARY,
	CT_DATA_STRING,
	CT_ARRAY,
	CT_DICT,
	CT_TAG, // unsupported
	CT_EXTRA,
	// CBOR extra types
	CT_FALSE = 20,
	CT_TRUE,
	CT_NULL,
	CT_UNDEF,
	CT__RESERVED,
	CT_F16, // unsupported
	CT_F32,
	CT_F64,
};

//
// functions

#ifdef KGCBOR_ENABLE_GENERATOR
static uint32_t kgcbor_header(kgcbor_gen_t *ctx, uint32_t type, cbor_uint_t value)
{
	uint32_t size;
	uint32_t sc;
	uint8_t *val = (uint8_t*)&value;

#ifdef KGCBOR_ENABLE_64BIT
	if(value >= 0x100000000)
	{
		size = sizeof(uint64_t);
		sc = 27;
	} else
#endif
	if(value >= 0x10000)
	{
		size = sizeof(uint32_t);
		sc = 26;
	} else
	if(value >= 0x100)
	{
		size = sizeof(uint16_t);
		sc = 25;
	} else
	if(value > 23)
	{
		size = sizeof(uint8_t);
		sc = 24;
	} else
	{
		size = 0;
		sc = value;
	}

	if(ctx->end && ctx->ptr + size >= ctx->end)
		return 1;

	*ctx->ptr++ = (type << 5) | sc;

	if(!size)
		return 0;

	do
	{
		size--;
		*ctx->ptr++ = val[size];
	} while(size);

	return 0;
}

static uint32_t kgcbor_str_bin(kgcbor_gen_t *ctx, uint32_t type, const uint8_t *data, uint32_t len)
{
	if(kgcbor_header(ctx, type, len))
		return 1;

	if(ctx->end && ctx->ptr + len > ctx->end)
		return 1;

	memcpy(ctx->ptr, data, len);
	ctx->ptr += len;

	return 0;
}
#endif

//
// recursive parser

static int32_t _cbor_parse_object(kgcbor_ctx_t *ctx, uint8_t **rptr, uint16_t recursion)
{
	int32_t ret;
	uint8_t *ptr;
	uint32_t type, arobj;
	cbor_uint_t count;
	cbor_uint_t size, keysz;
	kgcbor_value_t value, keyval;
	int32_t (*old_cb)(struct kgcbor_ctx_s*,uint8_t*,uint8_t,kgcbor_value_t*);

	if(recursion >= ctx->max_recursion)
		return KGCBOR_RECURSION_DEPTH;

	ptr = kgcbor_get(*rptr, ctx->end, &arobj, &value, &count);
	if(!ptr)
		return KGCBOR_ERROR;

	if(arobj != KGCBOR_TYPE_OBJECT && arobj != KGCBOR_TYPE_ARRAY)
		return KGCBOR_UNEXPECTED_TYPE;

	ctx->count = count;
	size = 0;

	for(uint32_t i = 0; i < count; i++)
	{
		uint8_t *nptr;

		ctx->ptr_val = ptr;

		if(arobj == KGCBOR_TYPE_OBJECT)
		{
			// key
			ptr = kgcbor_get(ptr, ctx->end, &type, &keyval, &keysz);
			if(!ptr)
				return KGCBOR_ERROR;

			if(type != KGCBOR_TYPE_STRING) // only string keys, for now ...
				return KGCBOR_EXPECTED_KEY;

			ctx->key_len = keysz;
		} else
			keyval.ptr = NULL;

		ctx->ptr_obj = ptr;
		ctx->index = i;

		// value
		nptr = kgcbor_get(ptr, ctx->end, &type, &value, &size);
		if(!nptr)
			return KGCBOR_ERROR;

		ctx->val_len = size;

		old_cb = ctx->entry_cb;

		if(	type == KGCBOR_TYPE_OBJECT ||
			type == KGCBOR_TYPE_ARRAY
		){
			uint8_t *valptr = ctx->ptr_val;
			uint8_t *objptr = ctx->ptr_obj;

			// entry callback
			if(ctx->entry_cb)
			{
				ret = ctx->entry_cb(ctx, keyval.ptr, type, NULL);
				if(ret < 0)
					return ret;
				if(ret)
					ctx->entry_cb = NULL;
			}

			// parse
			ret = _cbor_parse_object(ctx, &ptr, recursion + 1);
			if(ret)
				return ret;

			ctx->end_val = ptr;

			// restore stuff
			ctx->ptr_val = valptr;
			ctx->ptr_obj = objptr;
			ctx->index = i;
			ctx->count = count;
			ctx->key_len = keysz;
			ctx->val_len = size;

			// exit callback
			ret = 0;
			if(ctx->entry_cb && ctx->entry_cb != old_cb)
				ret = ctx->entry_cb(ctx, keyval.ptr, KGCBOR_TYPE_TERMINATOR_CB, NULL);
			ctx->entry_cb = old_cb;
			if(ret >= 0 && ctx->entry_cb)
				ret = ctx->entry_cb(ctx, keyval.ptr, KGCBOR_TYPE_TERMINATOR, NULL);

			if(ret < 0)
				return ret;
		} else
		{
			ptr = nptr;
			ctx->end_val = ptr;
			if(ctx->entry_cb)
			{
				ret = ctx->entry_cb(ctx, keyval.ptr, type, &value);
				if(ret < 0)
					return ret;
			}
		}
	}

	*rptr = ptr;

	return KGCBOR_RET_OK;
}

//
// API

int32_t kgcbor_parse_object(kgcbor_ctx_t *ctx, uint8_t *data, int32_t size)
{
	uint32_t ret;

	if(size < 0)
		ctx->end = NULL;
	else
		ctx->end = data + size;

	ret = _cbor_parse_object(ctx, &data, 0);
	if(ret == KGCBOR_RET_OK)
		ctx->end_val = data;

	return ret;
}

#ifdef KGCBOR_ENABLE_PRASE_FILE
int32_t kgcbor_parse_file(kgcbor_ctx_t *ctx, const uint8_t *fn)
{
#ifdef WINDOWS
	int ret;
	int len;
	FILE *f;
	uint8_t *buff;
	uint8_t *text;

	f = fopen(fn, "rb");
	if(!f)
		return KGCBOR_OPEN_ERROR;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	buff = malloc(len);
	if(!buff)
	{
		fclose(f);
		return KGCBOR_MAPPING_ERROR;
	}

	fread(buff, 1, len, f);
	fclose(f);

	ctx->end = data + len;
	ret = _cbor_parse_object(ctx, &data, 0);
	if(ret == KGCBOR_RET_OK)
		ctx->end_val = data;

	free(buff);

	return ret;
#else
	int fd, ret;
	uint8_t *tptr, *data;
	struct stat sb;

	fd = open(fn, O_RDONLY);
	if(fd < 0)
		return KGCBOR_OPEN_ERROR;

	fstat(fd, &sb);

	tptr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if(tptr == MAP_FAILED)
	{
		close(fd);
		return KGCBOR_MAPPING_ERROR;
	}
	data = tptr;

	ctx->end = data + sb.st_size;
	ret = _cbor_parse_object(ctx, &data, 0);
	if(ret == KGCBOR_RET_OK)
		ctx->end_val = data;

	munmap(tptr, sb.st_size);
	close(fd);

	return ret;
#endif
}
#endif

uint8_t *kgcbor_get(uint8_t *ptr, uint8_t *end, uint32_t *type, kgcbor_value_t *value, cbor_uint_t *size)
{
	kgcbor_value_t val;
	uint8_t mt, sc;

	if(end && ptr >= end)
		return NULL;

	mt = *ptr >> 5;
	sc = *ptr & 31;
	ptr++;

	if(sc < 24)
	{
		// value is stored directly
		val.uint = sc;
	} else
#ifdef KGCBOR_ENABLE_64BIT
	if(sc < 28)
	{
		// value is stored as; (24) 8bit; (25) 16bit; (26) 32bit; (27) 64bit
		uint32_t count = 1 << (sc - 24);
		val.u64 = 0;
		do
		{
			if(end && ptr >= end)
				return NULL;
			count--;
			val.raw[count] = *ptr++;
		} while(count);
	} else
#else
	if(sc < 27)
	{
		// value is stored as; (24) 8bit; (25) 16bit; (26) 32bit
		uint32_t count = 1 << (sc - 24);
		val.u32 = 0;
		do
		{
			if(end && ptr >= end)
				return NULL;
			count--;
			val.raw[count] = *ptr++;
		} while(count);
	} else
#endif
		// unsupported: (31) 'indefinite'
		return NULL;

	switch(mt)
	{
		case CT_INT_POS:
#ifdef KGCBOR_COMBINED_VALUE
			*type = KGCBOR_TYPE_VALUE;
#else
			*type = KGCBOR_TYPE_VALUE_POS;
#endif
			*value = val;
		break;
		case CT_INT_NEG:
#ifdef KGCBOR_COMBINED_VALUE
			val.sint++;
			val.sint = -val.sint;
			*type = KGCBOR_TYPE_VALUE;
#else
			*type = KGCBOR_TYPE_VALUE_NEG;
#endif
			*value = val;
		break;
		case CT_DATA_BINARY:
			if(!size)
				return NULL;
			if(end && val.uint > end - ptr)
				return NULL;
			value->ptr = ptr;
			ptr += val.uint;
			*size = val.uint;
			*type = KGCBOR_TYPE_BINARY;
		break;
		case CT_DATA_STRING:
			if(!size)
				return NULL;
			if(end && val.uint > end - ptr)
				return NULL;
			value->ptr = ptr;
			ptr += val.uint;
			*size = val.uint;
			*type = KGCBOR_TYPE_STRING;
		break;
		case CT_ARRAY:
			if(!size)
				return NULL;
			*size = val.uint;
			*type = KGCBOR_TYPE_ARRAY;
		break;
		case CT_DICT:
			if(!size)
				return NULL;
			*size = val.uint;
			*type = KGCBOR_TYPE_OBJECT;
		break;
		case CT_EXTRA:
			switch(sc)
			{
				case CT_FALSE:
					value->uint = 0;
					*type = KGCBOR_TYPE_BOOLEAN;
				break;
				case CT_TRUE:
					value->uint = 1;
					*type = KGCBOR_TYPE_BOOLEAN;
				break;
				case CT_NULL:
					*type = KGCBOR_TYPE_NULL;
				break;
				case CT_UNDEF:
					*type = KGCBOR_TYPE_UNDEFINED;
				break;
				case CT_F32:
					*type = KGCBOR_TYPE_FLOAT;
					for(uint32_t i = 0; i < 4; i++)
					{
						if(end && ptr >= end)
							return NULL;
						value->raw[3-i] = *ptr++;
					}
				break;
#ifdef KGCBOR_ENABLE_64BIT
				case CT_F64:
					*type = KGCBOR_TYPE_DOUBLE;
					for(uint32_t i = 0; i < 8; i++)
					{
						if(end && ptr >= end)
							return NULL;
						value->raw[7-i] = *ptr++;
					}
				break;
#endif
				default:
					return NULL;
			}
		break;
		default:
			return NULL;
	}

	return ptr;
}

//
// generators

#ifdef KGCBOR_ENABLE_GENERATOR
uint32_t kgcbor_put_object(kgcbor_gen_t *ctx, uint32_t count)
{
	return kgcbor_header(ctx, CT_DICT, count);
}

uint32_t kgcbor_put_array(kgcbor_gen_t *ctx, uint32_t count)
{
	return kgcbor_header(ctx, CT_ARRAY, count);
}

uint32_t kgcbor_put_string(kgcbor_gen_t *ctx, const uint8_t *data, int32_t len)
{
	if(len < 0)
		len = strlen(data);
	return kgcbor_str_bin(ctx, CT_DATA_STRING, data, len);
}

uint32_t kgcbor_put_binary(kgcbor_gen_t *ctx, const uint8_t *data, uint32_t len)
{
	return kgcbor_str_bin(ctx, CT_DATA_BINARY, data, len);
}

uint32_t kgcbor_put_s32(kgcbor_gen_t *ctx, int32_t value)
{
	if(value < 0)
		return kgcbor_header(ctx, CT_INT_NEG, -value - 1);
	return kgcbor_header(ctx, CT_INT_POS, value);
}

uint32_t kgcbor_put_u32(kgcbor_gen_t *ctx, uint32_t value)
{
	return kgcbor_header(ctx, CT_INT_POS, value);
}

uint32_t kgcbor_put_bool(kgcbor_gen_t *ctx, uint32_t value)
{
	value = value ? CT_TRUE : CT_FALSE;
	return kgcbor_header(ctx, CT_EXTRA, value);
}

uint32_t kgcbor_put_null(kgcbor_gen_t *ctx)
{
	return kgcbor_header(ctx, CT_EXTRA, CT_NULL);
}

uint32_t kgcbor_put_objextra(kgcbor_gen_t *ctx, uint8_t *data, uint32_t len, uint32_t extra)
{
	uint32_t type;
	kgcbor_value_t dummy;
	cbor_uint_t size;
	uint8_t *end = data + len;

	data = kgcbor_get(data, end, &type, &dummy, &size);
	if(!data)
		return 1;
	if(type != KGCBOR_TYPE_OBJECT)
		return 1;

	extra += size;

	type = kgcbor_header(ctx, CT_DICT, extra);
	if(type)
		return type;

	size = end - data;

	if(ctx->end && ctx->ptr + size > ctx->end)
		return 1;

	memcpy(ctx->ptr, data, size);
	ctx->ptr += size;

	return 0;
}
#endif

