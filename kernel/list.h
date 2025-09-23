#ifndef _LIST_H_
#define _LIST_H_
#include "types.h"

struct list {
  struct list *next;
  struct list *prev;
};

void lst_init(struct list *lst);
int  lst_empty(struct list *lst);
void lst_remove(struct list *e);
void* lst_pop(struct list *lst);
void lst_push(struct list *lst, void *p);
void lst_print(struct list *lst);

#endif
