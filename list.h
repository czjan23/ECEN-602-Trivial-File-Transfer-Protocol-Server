#ifndef _LIST_H
#define _LIST_H

// list head
struct list_head {
   struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

#define prefetch(x) NULL

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

// look up member address
#define container_of(ptr, type, member) ({  \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); })

static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next) {
   next->prev = new;
   new->next = next;
   new->prev = prev;
   prev->next = new;
}

// insert to list head
static inline void list_add(struct list_head *new, struct list_head *head) {
   __list_add(new, head, head->next);
}

// insert to list tail
static inline void list_add_tail(struct list_head *new, struct list_head *head) {
   __list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next) {
   next->prev = prev;
   prev->next = next;
}

// delete
static inline void list_del(struct list_head *entry) {
   __list_del(entry->prev, entry->next);
}

static inline void list_del_init(struct list_head *entry) {
   __list_del(entry->prev, entry->next);
   INIT_LIST_HEAD(entry);
}

// check empty
static inline int list_empty(const struct list_head *head) {
   return head->next == head;
}

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

// traverse list
#define list_for_each(pos, head) \
	for (pos = (head)->next, prefetch(pos->next); pos != (head); \
        	pos = pos->next, prefetch(pos->next))

// list traversal safely
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

#endif
