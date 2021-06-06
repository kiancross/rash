/*
 * Copyright (C) 2021 Kian Cross
 */

#ifndef HASH_TABLE_H_
#define HASH_TABLE_H_

#include <stdbool.h>

/*
 * Structure which represents a hash table.
*/
struct hash_table;

/*
 * Create a hash table.
*/
struct hash_table *hash_table_create();

/*
 * Free a hash table, calling a callback function for each element
 * in the table.
*/
void hash_table_free_callback(struct hash_table *table, void (*cb)(void *));

/*
 * Free a hash table (be aware that the caller must
 * free any memory passed to be stored).
*/
void hash_table_free(struct hash_table *table);

/*
 * Add data, associated with a given key to the hash table.
*/
bool hash_table_add(struct hash_table *table, const char *key, void *data);

/*
 * Remove data, associated with a given key from the hash table.
*/
bool hash_table_remove(struct hash_table *table, const char *key);

/*
 * Get data, associated with a given key from the hash table.
*/
void *hash_table_get(const struct hash_table *table, const char *key);

/*
 * Get the current number of elements currently stored in the hash table.
*/
size_t hash_table_get_size(const struct hash_table *table);

#endif
