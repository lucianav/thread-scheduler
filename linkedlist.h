/* Viziru Luciana - 332CA */

#ifndef LIST_H_
#define LIST_H_

typedef struct list list;

/* return reference to an empty list */
list *create_list(void);

/* insert node to end of a list */
void insert(list *mylist, void *value);

/* return an array of list values - char* */
void **get_values(list *mylist);

/* list size getter */
/* ret -1 for invalid list pointer */
int get_size(list *mylist);

/* delete all elements in list */
void empty_list(list *mylist);

/* destroy list */
void destruct_list(list *mylist);

#endif
