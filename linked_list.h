#ifndef LINKED_LIST_H
#define LINKED_LIST_H

/* this file defines structures and access methods for
 * generic linear linked lists
 */

#ifdef __KERNEL__
  #include <linux/vmalloc.h>
  #define malloc vmalloc
  #define free vfree
#endif

typedef struct
{
	void *next;
	void *data;
} linked_list_t;

static inline linked_list_t *linked_list_get_next(linked_list_t *l)
{
	return (linked_list_t *)l->next;
}

static inline void *linked_list_get_data(linked_list_t *l)
{
	return l->data;
}

static inline void linked_list_set_data(linked_list_t *l, void *newdata)
{
	l->data=newdata;
}

static inline void linked_list_init(linked_list_t **l)
{
	*l=NULL;
}

static inline void _linked_list_insert(linked_list_t **l, linked_list_t *new)
{
	new->next=*l;
	*l=new;
}

/** insert a new element at start of list
*/
static inline void linked_list_insert(linked_list_t **l, void *newdata)
{
	linked_list_t *temp=(linked_list_t *)malloc(sizeof(linked_list_t));
	linked_list_set_data(temp, newdata);
	_linked_list_insert(l, temp);
}

/** remove one element from list
*/
static inline void linked_list_remove(linked_list_t **l)
{
	linked_list_t *temp=*l;
	*l=linked_list_get_next(temp);
	free(temp);
}

/** free whole linked list and set pointer to NULL
*/
static inline void linked_list_finish(linked_list_t **l)
{
  while(*l)
    {
      linked_list_remove(l);
    }
}

#define linked_list_foreach(el,list) for(el=list; el!=NULL; el=linked_list_get_next(el))

#endif
