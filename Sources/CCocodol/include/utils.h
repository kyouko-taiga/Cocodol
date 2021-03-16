#ifndef COCODOL_UTILS_H
#define COCODOL_UTILS_H

#include <stddef.h>
#include <stdint.h>

/// Hashes the given buffer.
uint64_t fnv1_hash_buffer(const char* bytes, size_t count);

/// Hashes the given string.
uint64_t fnv1_hash_string(const char* str);

#endif
