
#define MAX_IDENT_SLOTS	1024

enum
{
	IDENT_WALL_TOP,
	IDENT_WALL_BOT,
	IDENT_WALL_MID,
	IDENT_PLANE_TOP,
	IDENT_PLANE_BOT,
	IDENT_THING,
	//
	IDENT_NONE,
};

typedef struct
{
	uint8_t type;
	kge_sector_t *sector;
	union
	{
		kge_line_t *line;
		kge_thing_t *thing;
	};
} ident_slot_t;

//

extern matrix3d_t r_projection_3d;
extern matrix3d_t r_projection_2d;
extern matrix3d_t r_projection_ui;
extern matrix3d_t r_work_matrix[3];

extern gl_vertex_t *gl_vertex_buf;

extern float viewangle[2];
extern float view_sprite_vec[2];
extern kge_xyz_vec_t viewpos;

extern gl_plane_t plane_left;
extern gl_plane_t plane_right;
extern gl_plane_t plane_bot;
extern gl_plane_t plane_top;
extern gl_plane_t plane_far;

extern uint32_t vc_render;

extern uint32_t ident_idx;
extern ident_slot_t ident_slot[MAX_IDENT_SLOTS];

//

uint32_t render_init();
void render_view(kge_thing_t *camera);

void render_raw_view(kge_thing_pos_t *pos, float z);
void render_scan_sector(kge_sector_t *sec, uint32_t recursion);

void r_draw_plane(kge_sector_t *sector, uint32_t plane);
