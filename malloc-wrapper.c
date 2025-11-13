#include "malloc-wrapper.h"

void *xmalloc(size_t bytes) {
  void *ptr;
  if ((ptr = malloc(bytes)) == NULL) {
    fprintf(stderr, "Failed to allocate, aborting...\n");
    exit(EXIT_FAILURE);
  }
  return (ptr);
}
