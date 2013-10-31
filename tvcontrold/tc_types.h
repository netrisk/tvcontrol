#ifndef TC_TYPES_H_INCLUDED
#define TC_TYPES_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

/** Get the pointer to the container type given a pointer
 *  to one of its members */
#define tc_containerof(ptr,type,member) \
	((type *) (char *)(ptr)-offsetof(type,member))

#endif // TC_TYPES_H_INCLUDED
