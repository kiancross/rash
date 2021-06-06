/*
 * Copyright (C) 2021 Kian Cross
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "rash.h"

/*
 * Initial exponent of 2 used for the size.
*/
#define HASH_TABLE_INITIAL_SHIFT 4

/*
 * The increment that should be added/subtracted from the
 * power of two when resizing the table.
*/
#define HASH_TABLE_RESIZE_INCREMENT 1

/*
 * Load factor used as one metric to increase the size of
 * the underlying array.
*/
#define HASH_TABLE_LOAD_FACTOR_INCREASE 0.75f

/*
 * Load factor used as the metric to decrease the size of the
 * underlying array.
*/
#define HASH_TABLE_LOAD_FACTOR_DECREASE 0.10f

/*
 * Determine the size of the multiplier depending on size of
 * size_t on the system.
*/
#if ((SIZE_MAX >> 63) == 0)
// If less than 64bit (assuming here that it is 32bit).
// 2^32 / golden ratio
#define FIBONACCI_MULTIPLIER 2654435769lu
#else
// If 64bit or greater.
// 2^64 / golden ratio
#define FIBONACCI_MULTIPLIER 11400714819323198485llu
#endif

/*
 * Macro used to iterate to the next available position in the hash table.
 * This is either an empty slot or a slot with an item that has the same
 * key.
 *
 *   table - The hash table.
 *
 *   key_value - Value of the key for which we are trying to find
 *               the next available position.
 *
 *   position - Initial value of the position for which this key corresponds.
 *              This value will be changed as we iterate. This must be a valid
 *              position in the array otherwise we could read beyond the bounds
 *              of the array.
 *
 *   probe_count - The value of the probe count (number of iterations taken
 *                 to find a free space.
*/
#define HASH_TABLE_ITERATE_TO_NEXT(table, key_value, position, probe_count) \
  for (                                                                     \
    (probe_count) = 0;                                                      \
                                                                            \
    /* Must do the probe count check first to ensure we don't               \
     * access memory that doesn't belong to us. Note that not               \
     * all the statements are evaluated - after the first false             \
     * statement, evaluation stops.                                         \
    */                                                                      \
    (probe_count) < (table)->max_size_shift &&                              \
    (table)->buckets[position] &&                                           \
    strcmp(table->buckets[position]->key, key_value) != 0;                  \
                                                                            \
    (probe_count)++, (position)++                                           \
  )

/*
 * Macro used to generate a for loop that will iterate through every bucket.
*/
#define HASH_TABLE_ITERATE_BUCKETS_TO_END(i, max_size, max_size_shift) \
  for (; (i) < (max_size) + (max_size_shift); (i)++)

/*
 * Same as as above macro but decomposes the arguments from a
 * table struct.
*/
#define HASH_TABLE_ITERATE_TO_END(table, i) \
  HASH_TABLE_ITERATE_BUCKETS_TO_END(i, table->max_size, table->max_size_shift)

/*
 * An entry (stored in a bucket), within the hash table.
*/
struct hash_table_entry {
  char *key;
  size_t hash;
  void *data;

  // How many positions from its desired position the element
  // is.
  uint8_t dist_from_des;
};

/*
 * A hash table which stores all entries.
*/
struct hash_table {

  // Store both the shift and max size (even though either could
  // be derived from the other as (max_size) = 2^(max_size_shift).
  // Means we don't have to constantly do the above calculation.
  uint8_t max_size_shift;
  size_t max_size;

  // Current number of elements in the hash table.
  size_t curr_size;
  struct hash_table_entry **buckets;
};

/*
 * Takes a hash (generated from some other hash function) and
 * ensures that all hashes past are equally distributed over a
 * range 0 to 2^(bits).
*/
static size_t fibonacci_hash(size_t hash, int bits);

/*
 * Hashing algorithm for a string.
*/
static size_t djb2_hash(const char *str);

/* Takes the values modified by HASH_TABLE_ITERATE_TO_NEXT
 * and checks if position is pointing to a valid position.
*/
static bool hash_table_is_next_found(
  const struct hash_table *table,
  uint8_t probe_count  
);

/*
 * Applies fibonacci_hash and djb2_hash at the
 * same time.
*/
static size_t hash_table_get_position(
  const struct hash_table *table,
  const char *key
);

/*
 * Compares the dist_from_des property to determine
 * if entry1 should replace entry2.
*/
static bool hash_table_should_replace_entry(
  const struct hash_table_entry *entry1,
  const struct hash_table_entry *entry2
);

/*
 * Remove a value from the hash table at the given position.
*/
static bool hash_table_remove_from_position(
  struct hash_table *table,
  size_t position
);

/*
 * Function to unwind any changes done by a part-way complete
 * insertion. This is used if node count is exceeded and we
 * need to undo the swaps we've made before we increase the
 * number of buckets.
*/
static void hash_table_unwind_insertion_changes(
  struct hash_table *table,
  struct hash_table_entry *inserted,
  struct hash_table_entry *original,
  size_t unwind_start_position
);

/*
 * Insert a given entry into the table.
*/
static bool hash_table_insert(
  struct hash_table *table,
  struct hash_table_entry *entry
);

/*
 * Rehash all the elements in the hash table.
*/
static void hash_table_rehash(
  struct hash_table *table,
  struct hash_table_entry **old_buckets,
  size_t old_max_size,
  uint8_t old_max_size_shift
);

/*
 * Resize the hash table by a given shift amount (increases by a given
 * power of 2).
*/
static bool hash_table_resize(struct hash_table *table, int shift_amount);

/*
 * Free a hash table entry.
*/
static void hash_table_entry_free(struct hash_table_entry *entry);

/*
 * Allocate the memory used for buckets.
*/
static struct hash_table_entry **hash_table_buckets_alloc(
  struct hash_table *table
);

/*
 * Create a hash table entry.
*/
static struct hash_table_entry *hash_table_entry_create(
  const char *key,
  void *data
);

/*
 * Determines if the hash table should be resized up (based
 * on the resize factor).
*/
static bool hash_table_should_resize_up_factor(
  const struct hash_table *table
);

/*
 * Determines if the hash table should be resized down (based
 * on the resize factor).
*/
static bool hash_table_should_resize_down_factor(
  const struct hash_table *table
);

// Taken from:
// https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-
// that-the-world-forgot-or-a-better-alternative-to-integer-modulo/
static size_t fibonacci_hash(size_t hash, int bits) {
  int shift_amount = (sizeof(size_t) * 8) - bits;
  return (hash * FIBONACCI_MULTIPLIER) >> shift_amount;
}

// Taken from:
// http://www.cse.yorku.ca/~oz/hash.html
// Magic numbers are part of the algorithm and represent nothing
// specifically.
static size_t djb2_hash(const char *str) {

  size_t hash = 5381;
  int c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

static bool hash_table_is_next_found(
  const struct hash_table *table,
  uint8_t probe_count  
) {

  // The max probe count is equal to the max_size_shift (log base
  // 2 of the table size). If probe count has exceeded this then
  // the next value was not found.
  return probe_count < table->max_size_shift;
}

static size_t hash_table_get_position(
  const struct hash_table *table,
  const char *key
) {
  return fibonacci_hash(djb2_hash(key), table->max_size_shift);
}

static bool hash_table_should_replace_entry(
  const struct hash_table_entry *entry1,
  const struct hash_table_entry *entry2
) {
  return entry1->dist_from_des > entry2->dist_from_des;
}

static bool hash_table_remove_from_position(
  struct hash_table *table,
  size_t position
) {

  // Set the current position to NULL (note that the caller
  // must have dealt with freeing memory).
  table->buckets[position] = NULL;

  position++;

  HASH_TABLE_ITERATE_TO_END(table, position) {
  
    struct hash_table_entry *entry = table->buckets[position];

    // If we reach an empty slot, or the current entry is in its
    // intended place then we can stop iterating.
    if (entry == NULL || entry->dist_from_des == 0) {
      break;
    }

    // Otherwise we can move all entries down by one slot to
    // make them closer to their desired position.
    entry->dist_from_des--;

    table->buckets[position - 1] = entry;
    table->buckets[position] = NULL;
  }

  table->curr_size--;

  return true;
}

static void hash_table_unwind_insertion_changes(
  struct hash_table *table,
  struct hash_table_entry *inserted,
  struct hash_table_entry *original,
  size_t unwind_start_position
) {

  struct hash_table_entry *poor = original;

  // Stopping condition is inside the the for loop.
  // i is unsigned therefore no point checking if
  // it is greater than or equal to 0, because it 
  // will never be a negative number.
  for (size_t i = unwind_start_position; ; i--) {

    struct hash_table_entry *current = table->buckets[i];
    
    // Each iteration backwards, the poor element gets a
    // bit richer.
    poor->dist_from_des--;

    // Check if the current element is richer than the poor
    // element. If it is then swap them.
    if (hash_table_should_replace_entry(current, poor)) {
      table->buckets[i] = poor;
      poor = current;
    }
    
    // Stop once the poor element is the same as the element
    // that was inserted (meaning we have removed the
    // element that was inserted).
    //
    // Also stop once at the beginning of the array. However this
    // should never happen without poor also being equal to
    // inserted.
    if (poor == inserted || i == 0) {
      break; 
    }
  }
}

static bool hash_table_insert(
  struct hash_table *table,
  struct hash_table_entry *entry
) {

  uint8_t probe_count = 0;
  struct hash_table_entry *rich = entry;
  size_t position = fibonacci_hash(entry->hash, table->max_size_shift);

  // Iterate through the buckets until we (hopefully) find an available
  // one. An available bucket is either:
  //
  //   - An empty slot.
  //
  //   - A slot that has the same key (meaning the element in the bucket
  //     is being replaced).
  HASH_TABLE_ITERATE_TO_NEXT(table, entry->key, position, probe_count) {

    struct hash_table_entry *current = table->buckets[position];

    // Check if we should swap the elements.
    if (hash_table_should_replace_entry(rich, current)) {
      table->buckets[position] = rich;
      rich = current;
    }
    
    rich->dist_from_des++;
  }

  // Check if we found an available slot.
  if (hash_table_is_next_found(table, probe_count)) {
    
    struct hash_table_entry *current = table->buckets[position];

    // If the slot has something in it (above if statement checks if it has
    // the same key) then free it.
    if (current) {
      hash_table_entry_free(current);

    } else {
    
      // We only want to increase the size if we are not replacing
      // an element.
      table->curr_size++;
    }

    table->buckets[position] = rich;

    return true;
 
  // This means we couldn't find an appropriate empty slot. 
  } else {

    // Undo all the changes we made in the above iteration. This is
    // important in case the reallocation fails - we don't want to
    // leave the underlying array in the state of a partial insertion.
    // So we undo what we've done.
    hash_table_unwind_insertion_changes(table, entry, rich, position - 1);

    if (!hash_table_resize(table, HASH_TABLE_RESIZE_INCREMENT)) {
      return false;
    }

    // Reset
    entry->dist_from_des = 0;

    // Try to insert again.
    return hash_table_insert(table, entry);
  }
}

static void hash_table_rehash(
  struct hash_table *table,
  struct hash_table_entry **old_buckets,
  size_t old_max_size,
  uint8_t old_max_size_shift
) {

  size_t i = 0;

  // Iterate through every bucket.
  HASH_TABLE_ITERATE_BUCKETS_TO_END(i, old_max_size, old_max_size_shift) {

    struct hash_table_entry *entry = old_buckets[i];

    // If the bucket contains something, reset its distant and then
    // insert it into the new buckets.
    if (entry) {
      entry->dist_from_des = 0;
      hash_table_insert(table, entry);
    }
  }
}

static bool hash_table_resize(struct hash_table *table, int shift_amount) {

  // Store the old sizes so that we can restore them if the reallocation
  // fails.
  size_t old_max_size = table->max_size;
  uint8_t old_max_size_shift = table->max_size_shift;

  table->max_size_shift += shift_amount;

  // It is undefined behaviour to have a negative shift
  // (little bit annoying because it would have made the
  // code much cleaner).
  if (shift_amount >= 0) {
    table->max_size <<= shift_amount;
  } else {
    table->max_size >>= -1 * shift_amount;
  }

  // Create the new buckets.
  struct hash_table_entry **new_buckets = hash_table_buckets_alloc(table);

  if (!new_buckets) {
    table->max_size = old_max_size;
    table->max_size_shift = old_max_size_shift;
    return false;
  }

  struct hash_table_entry **old_buckets = table->buckets;
  table->buckets = new_buckets;
  table->curr_size = 0;

  hash_table_rehash(table, old_buckets, old_max_size, old_max_size_shift);

  // We don't need the old buckets any more.
  free(old_buckets);

  return true;
}

static void hash_table_entry_free(struct hash_table_entry *entry) {
  free(entry->key);
  free(entry);
}

static struct hash_table_entry **hash_table_buckets_alloc(
  struct hash_table *table
) {
  return calloc(
    sizeof(*table->buckets),

    /*
     * Adding on table->max_size_shift (which is also the maximum
     * probe count) means that we never have to worry about wrapping
     * from the end of the array back to the beginning during probing.
    */
    table->max_size + table->max_size_shift
  );
}

static struct hash_table_entry *hash_table_entry_create(
  const char *key,
  void *data
) {

  struct hash_table_entry *entry = malloc(sizeof(*entry));
  if (!entry) {
    return NULL;
  }
  memset(entry, 0, sizeof(*entry));

  entry->hash = djb2_hash(key);
  entry->data = data;

  size_t key_length = strlen(key) + 1;

  // Make a copy of the key.
  entry->key = malloc(key_length);
  if (!entry->key) {
    free(entry);
    return NULL;
  }
  strncpy(entry->key, key, key_length);

  return entry;
}

static bool hash_table_should_resize_up_factor(
  const struct hash_table *table
) {

  // Add one to check if we have room for the item we might be
  // adding.
  return table->curr_size + 1 >
    table->max_size * HASH_TABLE_LOAD_FACTOR_INCREASE;
}

static bool hash_table_should_resize_down_factor(
  const struct hash_table *table
) {

  // Never go lower than the minimum shift.
  if (table->max_size_shift <= HASH_TABLE_INITIAL_SHIFT) {
    return false;
  }

  return table->curr_size - 1 <
    table->max_size * HASH_TABLE_LOAD_FACTOR_DECREASE;
}

struct hash_table *hash_table_create() {

  struct hash_table *table = malloc(sizeof(*table));
  if (!table) {
    return NULL;   
  }
  memset(table, 0, sizeof(*table));
  
  table->max_size_shift = HASH_TABLE_INITIAL_SHIFT;
  table->max_size = 1 << HASH_TABLE_INITIAL_SHIFT;

  table->buckets = hash_table_buckets_alloc(table);

  if (!table->buckets) {
    free(table);
    return NULL;
  }
  
  return table;
}

void hash_table_free_callback(struct hash_table *table, void (*cb)(void *)) {

  size_t i = 0;

  HASH_TABLE_ITERATE_TO_END(table, i) {
  
    struct hash_table_entry *entry = table->buckets[i];

    if (entry) {

      if (cb) {
        cb(entry->data);
      }

      hash_table_entry_free(entry);
    }
  }

  free(table->buckets);
  free(table);
}

void hash_table_free(struct hash_table *table) {
  hash_table_free_callback(table, NULL);
}

bool hash_table_add(struct hash_table *table, const char *key, void *data) {

  struct hash_table_entry *new_entry = hash_table_entry_create(key, data);
  if (!new_entry) {
    goto error_create;
  }

  if (hash_table_should_resize_up_factor(table)) {
    if (!hash_table_resize(table, HASH_TABLE_RESIZE_INCREMENT)) {
      goto error_resize;
    }
  }

  if (!hash_table_insert(table, new_entry)) {
    goto error_resize;
  }

  return true;

// Don't bother undoing a resize if we failed to add an
// entry - it will probably just cause more problems!
error_resize:
  hash_table_entry_free(new_entry);
error_create:
  return false;
}

bool hash_table_remove(struct hash_table *table, const char *key) {

  if (hash_table_should_resize_down_factor(table)) {
    if (!hash_table_resize(table, -HASH_TABLE_RESIZE_INCREMENT)) {
      return false;
    }
  }
  
  uint8_t probe_count = 0;
  size_t position = hash_table_get_position(table, key);
  
  // Find the position of the element (might not actually be
  // at the desired position if we've done linear probing).
  HASH_TABLE_ITERATE_TO_NEXT(table, key, position, probe_count);
 
  // Have we found the element? 
  if (
    !hash_table_is_next_found(table, probe_count) ||
    !table->buckets[position]
  ) {
    return false;
  }
  
  // Free the memory then remove it.
  hash_table_entry_free(table->buckets[position]);

  return hash_table_remove_from_position(table, position);
}

void *hash_table_get(const struct hash_table *table, const char *key) {

  size_t position = hash_table_get_position(table, key);
  uint8_t probe_count = 0;

  HASH_TABLE_ITERATE_TO_NEXT(table, key, position, probe_count);

  if (!hash_table_is_next_found(table, probe_count)) {
    return NULL;
  }

  struct hash_table_entry *entry = table->buckets[position];

  return entry ? entry->data : NULL;
}

size_t hash_table_get_size(const struct hash_table *table) {
  return table->curr_size;
}
