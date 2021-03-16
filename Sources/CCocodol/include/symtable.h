#ifndef COCODOL_SYMBOL_H
#define COCODOL_SYMBOL_H

#include "common.h"

struct SymTableEntry;

/// A symbol table, mapping identifiers to arbitrary data.
typedef struct SymTable {

  /// The buckets of the table.
  struct SymTableEntry* buckets;

  /// The number of used entries in `buckets`, including tombstones.
  size_t count;

  /// The capacity of `buckets`.
  size_t capacity;

} SymTable;

/// Initializes a symbol table.
void symtable_init(SymTable*);

/// Deinitializes a symbol table.
void symtable_deinit(SymTable*, bool delete_keys);

/// Inserts the given entry in the symbol table.
///
/// The function returns `NULL` if a new entry was inserted, or the value of the existing entry if
/// `key` was already in the table.
void* symtable_insert(SymTable*, const char* key, void* value);

/// Inserts or updates the given entry in the symbol table.
///
/// The function returns `NULL` if a new entry was inserted, or the value that was overridden if
/// `key` was already in the table.
void* symtable_update(SymTable*, const char* key, void* value);

/// Removes the entry indexed by the given key from the symbol table.
///
/// The function returns the value of the existing entry for `key`,  or `NULL` if `key` is not in
/// the table.
void* symtable_remove(SymTable*, const char* key);

/// Retrieves the value for the given key in the symbol table.
void* symtable_get(SymTable*, const char* key);

/// Returns the number of entries in the table.
size_t symtable_entry_count(SymTable*);

/// Executes the given function on each entry of the table.
///
/// The parameter `results` is a reference to an array of arbitrary pointers, in which the result
/// of each call to `transform` will be stored. `results` should be at least as large as the number
/// of entries in the table. The function returns the number of elements stored in `results`. If it
/// is passed as `NULL`, then no result is stored.
size_t symtable_map(SymTable*, void** results, void*(*transform)(const char*, void*));

/// Exevutes the given function on each entry of the table.
///
/// The second parameter is a pointer to arbitrary data that is passed to the given function.
void symtable_foreach(SymTable* self, void* user, void(*action)(const char*, void*, void*));

#endif
