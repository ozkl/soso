#ifndef LIST_H
#define LIST_H

#include "common.h"

#define list_foreach(list_node, list) for (ListNode* list_node = list->head; NULL != list_node ; list_node = list_node->next)

typedef struct ListNode
{
    struct ListNode* previous;
    struct ListNode* next;
    void* data;
} ListNode;

typedef struct List
{
    struct ListNode* head;
    struct ListNode* tail;
} List;

List* list_create();
void list_clear(List* list);
void list_destroy(List* list);
List* list_create_clone(List* list);
BOOL list_is_empty(List* list);
void list_append(List* list, void* data);
void list_prepend(List* list, void* data);
ListNode* list_get_first_node(List* list);
ListNode* list_get_last_node(List* list);
ListNode* list_find_first_occurrence(List* list, void* data);
int list_find_first_occurrence_index(List* list, void* data);
int list_get_count(List* list);
void list_remove_node(List* list, ListNode* node);
void list_remove_first_node(List* list);
void list_remove_last_node(List* list);
void list_remove_first_occurrence(List* list, void* data);

typedef struct Stack
{
    List* list;
} Stack;

Stack* stack_create();
void stack_clear(Stack* stack);
void stack_destroy(Stack* stack);
BOOL stack_is_empty(Stack* stack);
void stack_push(Stack* stack, void* data);
void* stack_pop(Stack* stack);

typedef struct Queue
{
    List* list;
} Queue;

Queue* queue_create();
void queue_clear(Queue* queue);
void queue_destroy(Queue* queue);
BOOL queue_is_empty(Queue* queue);
void queue_enqueue(Queue* queue, void* data);
void* queue_dequeue(Queue* queue);

#endif // LIST_H
