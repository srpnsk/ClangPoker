#include "stack.h"
#include "malloc-wrapper.h"

stack *stack_init() {
  stack *s = (stack *)xmalloc(sizeof(stack));

  s->node = NULL;
  return s;
}

void stack_push(stack *s, void *item) {
  stack_node *node = (stack_node *)xmalloc(sizeof(stack_node));

  node->item = item;
  node->next = s->node;

  s->node = node;
}

void *stack_pop(stack *s) {
  if (s->node == NULL)
    return NULL;

  stack_node *node = s->node;
  void *item = node->item;
  s->node = node->item;

  free(node);

  return item;
}

void *stack_peak(stack *s) {
  if (s->node == NULL)
    return NULL;

  stack_node *node = s->node;
  void *item = node->item;

  return item;
}

int stack_count(stack *s) {
  int i = 0;
  stack_node *node = s->node;

  while (node != NULL) {
    node = node->next;
    i++;
  }
  return i;
}

bool stack_empty(stack *s) {
  if (s->node == NULL) {
    return true;
  } else {
    return false;
  }
}
