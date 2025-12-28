
/// depths
#define DEPTH_PLANE_B	16
#define DEPTH_GRID	15
#define DEPTH_PLANE_A	14
#define DEPTH_PLANE_H	13
#define DEPTH_WALL	12
#define DEPTH_VERTEX	11
#define DEPTH_WALL_X	(DEPTH_WALL - 2)
#define DEPTH_VERTEX_X	(DEPTH_VERTEX - 2)
#define DEPTH_SKYBOX	8
#define DEPTH_THING	7
#define DEPTH_CAMERA	6
#define DEPTH_DRAWING	5
#define DEPTH_TEXT	0

// stuff
#define EDIT_SELECTION_ALPHA	0.4f
#define EDIT_HIGHLIGHT_ALPHA_OFFSET	0.2f
#define EDIT_HIGHLIGHT_ALPHA_OFFSET_BAD	0.3f
#define EDIT_HIGHLIGHT_FREQ_MULT	0.09f
#define EDIT_HIGHLIGHT_LINE_WIDTH	5.0f
#define EDIT_VERTEX_SIZE	2.5f
#define EDIT_THING_CIRCLE_VTX	31 // odd numbers only

// TODO: move to config
#define EDIT_EXPORT_BUFFER_SIZE	(16 * 1024 * 1024)
#define EDIT_MAX_SECTOR_LINES	32
#define EDIT_MAX_SOBJ_LINES	8
#define EDIT_MAX_FILE_PATH	256
#define EDIT_MAX_GROUPS	128
#define EDIT_MAX_SUBGROUPS	16
#define EDIT_MAX_GROUP_NAME	20

// group selector flags
#define EDIT_GRPSEL_FLAG_ENABLE_SPECIAL	1
#define EDIT_GRPSEL_FLAG_ENABLE_NONE	2
#define EDIT_GRPSEL_FLAG_ALLOW_MODIFY	4

/// colors
// NOTE: 'bad' thing color must follow 'normal' color
enum
{
	EDITCOLOR_GRID,
	EDITCOLOR_ORIGIN,
	EDITCOLOR_CAMERA,
	EDITCOLOR_CAMERA_BAD,
	EDITCOLOR_THING,
	EDITCOLOR_THING_BAD,
	EDITCOLOR_LINE_SOLID,
	EDITCOLOR_LINE_PORTAL,
	EDITCOLOR_LINE_NEW,
	EDITCOLOR_LINE_OBJ,
	EDITCOLOR_LINE_BAD,
	EDITCOLOR_VERTEX,
	EDITCOLOR_VERTEX_NEW,
	EDITCOLOR_SECTOR_HIGHLIGHT,
	EDITCOLOR_SECTOR_HIGHLIGHT_BAD,
	EDITCOLOR_SECTOR_HIGHLIGHT_LINK,
	EDITCOLOR_CROSSHAIR,
	//
	EDITCOLOR__COUNT
};

enum
{
	EDIT_MODE_2D,
	EDIT_MODE_3D,
	EDIT_MODE_WIREFRAME,
	//
	EDIT_MODE_2D_OR_3D = -1
};

enum
{
	EDIT_XMODE_MAP, // must be zero
	EDIT_XMODE_GFX,
	EDIT_XMODE_THG,
	//
	NUM_X16_EDIT_MODES
};

enum
{
	EDIT_CBOR_TYPE_FLAG8,
	EDIT_CBOR_TYPE_U8,
	EDIT_CBOR_TYPE_S8,
	EDIT_CBOR_TYPE_U16,
	EDIT_CBOR_TYPE_S16,
	EDIT_CBOR_TYPE_U32,
	EDIT_CBOR_TYPE_S32,
	EDIT_CBOR_TYPE_ANGLE,
	EDIT_CBOR_TYPE_FLOAT,
	EDIT_CBOR_TYPE_STRING,
	EDIT_CBOR_TYPE_BINARY,
	EDIT_CBOR_TYPE_OBJECT,
	EDIT_CBOR_TYPE_ARRAY,
};

enum
{
	EDIT_INPUT_ESCAPE,
	EDIT_INPUT_LEFT,
	EDIT_INPUT_RIGHT,
	EDIT_INPUT_UP,
	EDIT_INPUT_DOWN,
	EDIT_INPUT_COPY,
	EDIT_INPUT_PASTE,
};

enum
{
	BLOCKING_PLAYER,
	BLOCKING_ENEMY,
	BLOCKING_SOLID,
	BLOCKING_PROJECTILE,
	BLOCKING_HITSCAN,
	BLOCKING_EXTRA_A,
	BLOCKING_EXTRA_B,
	BLOCKING_SPECIAL,
	//
	NUM_BLOCKBITS
};

#define BLOCK_FLAG(x)	(1 << (x))

//

typedef struct
{
	int32_t inidx;
	int32_t (*func)();
} edit_input_func_t;

typedef struct
{
	union
	{
		struct
		{
			float r, g, b, a;
		};
		float color[4];
	};
} edit_color_t;

typedef struct
{
	kge_line_t *line;
	kge_thing_t *thing;
	kge_vertex_t *vertex;
	kge_sector_t *sector;
	kge_sector_t *extra_sector;
	uint16_t idx, locked;
} edit_hit_t;

typedef struct
{
	const uint8_t *name;
	uint32_t extra;
	uint16_t nlen;
	uint16_t type;
	uint_fast8_t *valid;
	uint32_t *readlen;
	union
	{
		void *handler;
		uint8_t *ptr;
		uint8_t *u8;
		int8_t *s8;
		uint16_t *u16;
		int16_t *s16;
		uint32_t *u32;
		int32_t *s32;
		float *f32;
	};
} edit_cbor_obj_t;

typedef struct
{
	uint8_t name[EDIT_MAX_GROUP_NAME];
} edit_subgroup_t;

typedef struct
{
	uint8_t name[EDIT_MAX_GROUP_NAME];
	uint32_t count;
	edit_subgroup_t sub[EDIT_MAX_SUBGROUPS];
} edit_group_t;

typedef struct edit_sec_obj_s
{
	struct edit_sec_obj_s *next;
	struct edit_sec_obj_s *prev;
	uint32_t count;
	float dist;
	kge_vertex_t origin;
	kge_line_t line[EDIT_MAX_SOBJ_LINES];
	kge_vertex_t vtx[EDIT_MAX_SOBJ_LINES];
} edit_sec_obj_t;

//

struct kgcbor_gen_s;
union kgcbor_value_u;
struct kge_x16_tex_s;
struct thing_st_s;

// common

extern uint32_t vc_editor;

extern edit_color_t editor_color[EDITCOLOR__COUNT];

extern float edit_highlight_alpha;

extern struct linked_list_s edit_list_draw_new;
extern struct linked_list_s edit_list_vertex;
extern struct linked_list_s edit_list_sector;

extern int32_t edit_sky_num;

extern edit_hit_t edit_hit;

extern gl_vertex_t edit_circle_vtx[EDIT_THING_CIRCLE_VTX+4];

extern edit_group_t edit_group[EDIT_MAX_GROUPS];
extern uint32_t edit_num_groups;
extern uint32_t edit_grp_show;
extern uint32_t edit_subgrp_show;
extern uint32_t edit_grp_last;
extern uint32_t edit_subgrp_last;

extern kge_secplane_t edit_ceiling_plane;
extern kge_secplane_t edit_floor_plane;
extern kge_x16_tex_t edit_line_default;
extern const kge_x16_tex_t edit_empty_texture;

extern kge_thing_pos_t edit_wf_pos;

extern uint32_t edit_ui_busy;

extern uint8_t edit_info_box_text[1024];

extern kge_thing_t *edit_camera_thing;

extern void *edit_cbor_buffer;

extern uint32_t edit_x16_mode;

extern const uint8_t *txt_right_left[];
extern const uint8_t *txt_yes_no[];
extern const uint8_t *txt_top_bot[];
extern const uint8_t *txt_back_front[];

//

uint32_t editor_init();

void edit_draw_ui();
void edit_ui_blocking_select(const uint8_t *title, uint8_t*, int32_t);
void edit_ui_thing_select(kge_thing_t*);
void edit_ui_tframe_select(uint8_t*, uint32_t);
void edit_ui_wframe_select(uint8_t*, uint32_t);
void edit_ui_tsprite_select(uint32_t*);
void edit_ui_wsprite_select(uint32_t*);
void edit_ui_light_select(kge_light_t*);
void edit_ui_palette_select(uint8_t*);
void edit_ui_texture_select(const uint8_t *title, kge_x16_tex_t *dest, uint32_t mode);
void edit_ui_file_select(const uint8_t *title, const uint8_t *path, const uint8_t *suffix, void (*cb)(uint8_t *file));
void edit_ui_action_select(struct thing_st_s *st, uint32_t is_wpn);
void edit_ui_group_select(const uint8_t *title, uint32_t flags, void (*cb)(uint32_t magic));
void edit_ui_textentry(const uint8_t *title, uint32_t limit, void (*cb)(uint8_t*));
void edit_ui_question(const uint8_t *title, const uint8_t *text, void (*cb)(uint32_t));
void edit_ui_info_box(const uint8_t *title, uint32_t textcolor);
void edit_busy_window(const uint8_t *tile);

uint8_t *edit_put_blockbits(uint8_t *dst, uint8_t bits);

uint32_t edit_ui_input();
void edit_ui_tick();
void edit_ui_draw();

void edit_place_thing_in_sector(kge_thing_t *th, kge_sector_t *sec);
void edit_mark_sector_things(kge_sector_t *sec);
void edit_fix_thing_z(kge_thing_t *thing, uint32_t);
void edit_fix_marked_things_z(uint32_t);

void edit_update_thing_type(kge_thing_t *th);
uint32_t edit_get_special_thing(const uint8_t *);

void edit_update_object(edit_sec_obj_t*);

uint32_t edit_is_sector_hidden(kge_sector_t*);

void edit_clear_hit(uint32_t);
void edit_highlight_changed();

void edit_spawn_camera();
void edit_enter_thing(kge_thing_t *);

void edit_new_map();
uint32_t edit_check_map();

void edit_status_printf(const uint8_t *text, ...);
uint32_t edit_handle_input(const edit_input_func_t *ifunc);

void editor_x16_mode(uint32_t mode);
void editor_change_mode(int32_t mode);

uint32_t edit_is_point_in_sector(float x, float y, kge_sector_t *sec, uint32_t only_visible);
kge_sector_t *edit_find_sector_by_point(float x, float y, kge_sector_t *ignore, uint32_t only_visible);

uint32_t edit_check_sector_overlap(kge_sector_t*, kge_sector_t*);

void *edit_cbor_branch(const edit_cbor_obj_t *cbor_obj, uint32_t type, const uint8_t *key, uint32_t key_len, union kgcbor_value_u *value, uint32_t val_len);
uint32_t edit_cbor_export(const edit_cbor_obj_t *cbor_obj, uint32_t count, struct kgcbor_gen_s *gen);

uint32_t edit_check_name(const uint8_t *text, uint8_t reject);

int32_t edit_infunc_menu();

// 2D

void e2d_input();
void e2d_draw();
void e2d_tick(uint32_t);
void e2d_mode_set();

kge_line_t *e2d_find_other_line(kge_sector_t *sec, kge_sector_t *os, kge_line_t *ol);

// 3D

extern uint32_t e3d_highlight;

void e3d_input();
void e3d_draw();
void e3d_tick(uint32_t);
void e3d_mode_set();

// wireframe

void ewf_input();
void ewf_draw();
void ewf_tick(uint32_t);
void ewf_mode_set();

// files

void *edit_load_file(const uint8_t *path, uint32_t *size);
uint32_t edit_read_file(const uint8_t *path, void *buff, uint32_t size);
uint32_t edit_save_file(const uint8_t *path, void *data, uint32_t size);
const uint8_t *edit_path_filename(const uint8_t *path);

// map

const uint8_t *edit_map_save(const uint8_t*);
const uint8_t *edit_map_load(const uint8_t*);

