#include "alloc.h"
#include "common.h"
#include "list.h"

List* list_create()
{
    List* list = (List*)kmalloc(sizeof(List));

    memset((uint8_t*)list, 0, sizeof(List));

    return list;
}

void list_clear(List* list)
{
    ListNode* list_node = list->head;

    while (NULL != list_node)
    {
        ListNode* next = list_node->next;

        kfree(list_node);

        list_node = next;
    }

    list->head = NULL;
    list->tail = NULL;
}

void list_destroy(List* list)
{
    list_clear(list);

    kfree(list);
}

List* list_create_clone(List* list)
{
    List* newList = list_create();

    list_foreach(n, list)
    {
        list_append(newList, n->data);
    }

    return newList;
}

BOOL list_is_empty(List* list)
{
    //At empty state, both head and tail are null!
    return list->head == NULL;
}

void list_append(List* list, void* data)
{
    ListNode* node = (ListNode*)kmalloc(sizeof(ListNode));

    memset((uint8_t*)node, 0, sizeof(ListNode));
    node->data = data;

    //At empty state, both head and tail are null!
    if (NULL == list->tail)
    {
        list->head = node;

        list->tail = node;

        return;
    }

    node->previous = list->tail;
    node->previous->next = node;
    list->tail = node;
}

void list_prepend(List* list, void* data)
{
    ListNode* node = (ListNode*)kmalloc(sizeof(ListNode));

    memset((uint8_t*)node, 0, sizeof(ListNode));
    node->data = data;

    //At empty state, both head and tail are null!
    if (NULL == list->tail)
    {
        list->head = node;

        list->tail = node;

        return;
    }

    node->next = list->head;
    node->next->previous = node;
    list->head = node;
}

ListNode* list_get_first_node(List* list)
{
    return list->head;
}

ListNode* list_get_last_node(List* list)
{
    return list->tail;
}

ListNode* list_find_first_occurrence(List* list, void* data)
{
    list_foreach(n, list)
    {
        if (n->data == data)
        {
            return n;
        }
    }

    return NULL;
}

int list_find_first_occurrence_index(List* list, void* data)
{
    int result = 0;

    list_foreach(n, list)
    {
        if (n->data == data)
        {
            return result;
        }

        ++result;
    }

    return -1;
}

int list_get_count(List* list)
{
    int result = 0;

    list_foreach(n, list)
    {
        ++result;
    }

    return result;
}

void list_remove_node(List* list, ListNode* node)
{
    if (NULL == node)
    {
        return;
    }

    if (NULL != node->previous)
    {
        node->previous->next = node->next;
    }

    if (NULL != node->next)
    {
        node->next->previous = node->previous;
    }

    if (node == list->head)
    {
        list->head = node->next;
    }

    if (node == list->tail)
    {
        list->tail = node->previous;
    }

    kfree(node);
}

void list_remove_first_node(List* list)
{
    if (NULL != list->head)
    {
        list_remove_node(list, list->head);
    }
}

void list_remove_last_node(List* list)
{
    if (NULL != list->tail)
    {
        list_remove_node(list, list->tail);
    }
}

void list_remove_first_occurrence(List* list, void* data)
{
    ListNode* node = list_find_first_occurrence(list, data);

    if (NULL != node)
    {
        list_remove_node(list, node);
    }
}

Stack* stack_create()
{
    Stack* stack = (Stack*)kmalloc(sizeof(Stack));

    memset((uint8_t*)stack, 0, sizeof(Stack));

    stack->list = list_create();

    return stack;
}

void stack_clear(Stack* stack)
{
    list_clear(stack->list);
}

void stack_destroy(Stack* stack)
{
    list_destroy(stack->list);

    kfree(stack);
}

BOOL stack_is_empty(Stack* stack)
{
    return list_is_empty(stack->list);
}

void stack_push(Stack* stack, void* data)
{
    list_prepend(stack->list, data);
}

void* stack_pop(Stack* stack)
{
    void* result = NULL;

    ListNode* node = list_get_first_node(stack->list);

    if (NULL != node)
    {
        result = node->data;

        list_remove_node(stack->list, node);
    }

    return result;
}

Queue* queue_create()
{
    Queue* queue = (Queue*)kmalloc(sizeof(Queue));

    memset((uint8_t*)queue, 0, sizeof(Queue));

    queue->list = list_create();

    return queue;
}

void queue_clear(Queue* queue)
{
    list_clear(queue->list);
}

void queue_destroy(Queue* queue)
{
    list_destroy(queue->list);

    kfree(queue);
}

BOOL queue_is_empty(Queue* queue)
{
    return list_is_empty(queue->list);
}

void queue_enqueue(Queue* queue, void* data)
{
    list_append(queue->list, data);
}

void* queue_dequeue(Queue* queue)
{
    void* result = NULL;

    ListNode* node = list_get_first_node(queue->list);

    if (NULL != node)
    {
        result = node->data;

        list_remove_node(queue->list, node);
    }

    return result;
}
