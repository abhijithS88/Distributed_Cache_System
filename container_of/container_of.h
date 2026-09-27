#pragma once

#include <cstddef>

#ifndef container_of
#define container_of(ptr, T, member) \
    ((T*)((char*)(ptr) - offsetof(T, member)))
#endif