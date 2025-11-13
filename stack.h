#ifndef STACK_H
#define STACK_H

#include <stdbool.h>

typedef struct stack_node__ {
  void *item;
  struct stack_node__ *next;
} stack_node;

typedef struct stack__ {
  stack_node *node;
} stack;

stack *stack_init();

void stack_push(stack *s, void *item);
void *stack_pop(stack *s);
void *stack_peak(stack *s);

int stack_count(stack *s);
bool stack_empty(stack *s);

#endif
