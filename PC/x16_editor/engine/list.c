#include "inc.h"
#include "list.h"

//
// API

void *list_add_entry(linked_list_t *list, uint32_t size)
{
	link_entry_t *ret;

	ret = malloc(sizeof(link_entry_t) + size);
	if(!ret)
		return NULL;

	list->count++;

	ret->next = NULL;
	ret->prev = list->cur;

	if(list->cur)
		list->cur->next = ret;

	list->cur = ret;

	if(!list->top)
		list->top = ret;

	return ret + 1;
}

void list_clear(linked_list_t *list)
{
	link_entry_t *ent = list->top;

	while(ent)
	{
		link_entry_t *del = ent;
		ent = ent->next;
		free(del);
	}

	list->top = NULL;
	list->cur = NULL;
	list->count = 0;
}

void list_del_entry(linked_list_t *list, link_entry_t *ent)
{
	if(ent->prev)
		ent->prev->next = ent->next;
	if(ent->next)
		ent->next->prev = ent->prev;

	if(list->top == ent)
		list->top = ent->next;
	if(list->cur == ent)
		list->cur = ent->prev;

	list->count--;

	free(ent);
}

uint32_t list_get_idx(linked_list_t *list, link_entry_t *ent)
{
	uint32_t idx = 0;
	while(ent != list->top)
	{
		idx++;
		ent = ent->prev;
	}
	return idx;
}

void *list_find_idx(linked_list_t *list, uint32_t idx)
{
	link_entry_t *ent = list->top;

	if(idx >= list->count)
		return NULL;

	while(ent)
	{
		if(!idx)
			return ent + 1;
		idx--;
		ent = ent->next;
	}

	return NULL;
}

