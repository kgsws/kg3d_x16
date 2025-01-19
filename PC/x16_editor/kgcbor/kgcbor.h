//
// kgsws' simple CBOR parser

//#define KGCBOR_ENABLE_PRASE_FILE	// enable 'kgcbor_parse_file'
#define KGCBOR_ENABLE_GENERATOR
//#define KGCBOR_ENABLE_64BIT	// enable 'uint64_t' and 'double'
#define KGCBOR_COMBINED_VALUE	// do not distinguish between negative and positive types; use singend integer

enum
{
	KGCBOR_RET_OK,
	KGCBOR_ERROR,
	KGCBOR_UNEXPECTED_TYPE,
	KGCBOR_EXPECTED_KEY,
	KGCBOR_RECURSION_DEPTH,
	KGCBOR_OPEN_ERROR,
	KGCBOR_MAPPING_ERROR,
};

enum
{
	KGCBOR_TYPE_OBJECT,
	KGCBOR_TYPE_ARRAY,
#ifdef KGCBOR_COMBINED_VALUE
	KGCBOR_TYPE_VALUE,
#else
	KGCBOR_TYPE_VALUE_POS,
	KGCBOR_TYPE_VALUE_NEG,
#endif
	KGCBOR_TYPE_FLOAT,
#ifdef KGCBOR_ENABLE_64BIT
	KGCBOR_TYPE_DOUBLE,
#endif
	KGCBOR_TYPE_STRING,
	KGCBOR_TYPE_BINARY,
	KGCBOR_TYPE_BOOLEAN,
	KGCBOR_TYPE_NULL,
	KGCBOR_TYPE_UNDEFINED,
	KGCBOR_TYPE_TERMINATOR,
	KGCBOR_TYPE_TERMINATOR_CB,
};

#ifdef KGCBOR_ENABLE_64BIT
typedef uint64_t cbor_uint_t;
typedef int64_t cbor_sint_t;
#else
typedef uint32_t cbor_uint_t;
typedef int32_t cbor_sint_t;
#endif

typedef union kgcbor_value_u
{
	void *ptr;
	cbor_uint_t uint;
	cbor_sint_t sint;
	uint8_t raw[4];
	//
	uint8_t u8;
	int8_t s8;
	uint16_t u16;
	int16_t s16;
	uint32_t u32;
	int32_t s32;
	float f32;
#ifdef KGCBOR_ENABLE_64BIT
	uint64_t u64;
	int64_t s64;
	double f64;
#endif
} kgcbor_value_t;

struct kgcbor_ctx_s;
typedef int32_t (*cbor_entry_cb_t)(struct kgcbor_ctx_s*,uint8_t*,uint8_t,kgcbor_value_t*);

typedef struct kgcbor_ctx_s
{
	cbor_entry_cb_t entry_cb;
	uint8_t *end;
	void *ptr_obj;
	void *ptr_val;
	void *end_val;
	cbor_uint_t key_len;
	cbor_uint_t val_len;
	cbor_uint_t count;
	cbor_uint_t index;
	uint32_t max_recursion;
} kgcbor_ctx_t;

#ifdef KGCBOR_ENABLE_GENERATOR
typedef struct kgcbor_gen_s
{
	uint8_t *ptr;
	uint8_t *end;
} kgcbor_gen_t;
#endif

//

int32_t kgcbor_parse_object(kgcbor_ctx_t *ctx, uint8_t *data, int32_t size);

#ifdef KGCBOR_ENABLE_PRASE_FILE
int32_t kgcbor_parse_file(kgcbor_ctx_t *ctx, const uint8_t *fn);
#endif

uint8_t *kgcbor_get(uint8_t *ptr, uint8_t *end, uint32_t *type, kgcbor_value_t *value, cbor_uint_t *size);

#ifdef KGCBOR_ENABLE_GENERATOR
uint32_t kgcbor_put_object(kgcbor_gen_t *ctx, uint32_t count);
uint32_t kgcbor_put_array(kgcbor_gen_t *ctx, uint32_t count);
uint32_t kgcbor_put_string(kgcbor_gen_t *ctx, const uint8_t *data, int32_t len);
uint32_t kgcbor_put_binary(kgcbor_gen_t *ctx, const uint8_t *data, uint32_t len);
uint32_t kgcbor_put_s32(kgcbor_gen_t *ctx, int32_t value);
uint32_t kgcbor_put_u32(kgcbor_gen_t *ctx, uint32_t value);
uint32_t kgcbor_put_bool(kgcbor_gen_t *ctx, uint32_t value);
uint32_t kgcbor_put_null(kgcbor_gen_t *ctx);

uint32_t kgcbor_put_objextra(kgcbor_gen_t *ctx, uint8_t *data, uint32_t len, uint32_t extra);
#endif

