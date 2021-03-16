#include "utils.h"

#define FNV_BASIS 0xcbf29ce484222325
#define FNV_PRIME 0x100000001b3

uint64_t fnv1_hash_buffer(const char* bytes, size_t count) {
  uint64_t h = FNV_BASIS;
  for (size_t i = 0; i < count; ++i) {
    h = h * FNV_PRIME;
    h = h ^ bytes[i];
  }
  return h;
}

uint64_t fnv1_hash_string(const char* str) {
  uint64_t h = FNV_BASIS;
  size_t i = 0;
  while (str[i] != 0) {
    h = h * FNV_PRIME;
    h = h ^ str[i];
    i++;
  }
  return h;
}

