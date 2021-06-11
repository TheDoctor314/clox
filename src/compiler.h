#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "object.h"
#include "vm.h"

ObjFunction *compile(const char *src);
void mark_compiler_roots();

#endif
