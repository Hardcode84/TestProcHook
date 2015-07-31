#ifndef CODETOINJECT_H
#define CODETOINJECT_H

#include <stddef.h>
#include <stdint.h>

size_t sizeToInject();

ptrdiff_t thrdFuncOffset();

void writeData(void* ptr, float scale);




#endif // CODETOINJECT_H
