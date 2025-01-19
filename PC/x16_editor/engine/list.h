
#define LIST_ENTRY(e)	((link_entry_t*)(e)-1)

typedef struct link_entry_s
{
	struct link_entry_s *next;
	struct link_entry_s *prev;
	// data follows
} link_entry_t;

typedef struct linked_list_s
{
	link_entry_t *top;
	link_entry_t *cur;
	uint32_t count;
} linked_list_t;

//

void *list_add_entry(linked_list_t *list, uint32_t size);
void list_clear(linked_list_t *list);
void list_del_entry(linked_list_t *list, link_entry_t *ent);
uint32_t list_get_idx(linked_list_t *list, link_entry_t *ent);
void *list_find_idx(linked_list_t *list, uint32_t idx);

