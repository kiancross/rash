/*
 * Copyright (C) 2021 Kian Cross
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../src/rash.h"

/*
 * Ensure creating a hash table works.
*/
static void hash_table_tests_create() {

  struct hash_table *table = hash_table_create();

  assert(table);
  assert(hash_table_get_size(table) == 0);

  hash_table_free(table);
}

/*
 * Try adding a couple of elements to the hash table.
*/
static void hash_table_tests_add() {

  struct hash_table *table = hash_table_create();

  assert(table);

  assert(hash_table_add(table, "key1", NULL));
  assert(hash_table_get_size(table) == 1);
  
  assert(hash_table_add(table, "key2", NULL));
  assert(hash_table_get_size(table) == 2);

  hash_table_free(table);
}

/*
 * Check if replacing an element works.
*/
static void hash_table_tests_add_duplicate() {

  struct hash_table *table = hash_table_create();

  assert(table);

  int a = 20;
  int b = 30;

  assert(hash_table_add(table, "key1", &a));

  int *result = hash_table_get(table, "key1");
  assert(result == &a);
  
  assert(hash_table_add(table, "key1", &b));
  result = hash_table_get(table, "key1");
  assert(result == &b);
  
  hash_table_free(table);
}

/*
 * Ensure removing elements works.
*/
static void hash_table_tests_remove() {
  
  struct hash_table *table = hash_table_create();

  assert(table);

  int a = 20;
  int b = 30;

  assert(hash_table_add(table, "key1", &a));
  assert(hash_table_add(table, "key2", &b));

  assert(hash_table_get_size(table) == 2);

  assert(hash_table_get(table, "key1") == &a);
  assert(hash_table_get(table, "key2") == &b);

  assert(hash_table_remove(table, "key1"));
  assert(!hash_table_get(table, "key1"));
  assert(hash_table_get_size(table) == 1);
  
  assert(hash_table_remove(table, "key2"));
  assert(!hash_table_get(table, "key2"));
  assert(hash_table_get_size(table) == 0);

  hash_table_free(table);
}

/*
 * Try removing something from an empty table.
*/
static void hash_table_tests_remove_empty() {
  
  struct hash_table *table = hash_table_create();

  assert(table);
  assert(!hash_table_remove(table, "does_not_exist"));

  hash_table_free(table);
}

/*
 * Try and get an element from an empty table.
*/
static void hash_table_tests_get_empty() {

  struct hash_table *table = hash_table_create();
  assert(table);

  assert(!hash_table_get(table, "does_not_exists"));

  hash_table_free(table);
}

/*
 * Add a large number of elements to the table. Ensure that each
 * element still has the correct value after each addition. Using
 * a large size ensures that the resize of the underlying array
 * is tested.
*/
static void hash_table_tests_add_many() {

  struct hash_table *table = hash_table_create();
  assert(table);

  srand(13);

  int numbers[5000];

  const size_t N = sizeof(numbers) / sizeof(numbers[0]);

  // Add elements.
  for (size_t i = 0; i < N; i++) {
  
    numbers[i] = rand();

    char key[16];
    snprintf(key, sizeof(key), "key_%zu", i);

    assert(hash_table_add(table, key, numbers + i));
    assert(hash_table_get_size(table) == i + 1);

    // Check all elements have the correct value.
    for (size_t j = 0; j <= i; j++) {
    
      snprintf(key, sizeof(key), "key_%zu", j);
    
      int *value = hash_table_get(table, key);

      assert(value == (numbers + j));
    }
  }

  hash_table_free(table);
}

/*
 * Remove a large number of elements from the table. After
 * each removal check the remaining elements have the correct
 * value.
*/
static void hash_table_tests_remove_many() {

  struct hash_table *table = hash_table_create();
  assert(table);

  srand(13);

  int numbers[5000];

  const size_t N = sizeof(numbers) / sizeof(numbers[0]);

  // Add elements.
  for (size_t i = 0; i < N; i++) {
  
    numbers[i] = rand();

    char key[16];
    snprintf(key, sizeof(key), "key_%zu", i);

    assert(hash_table_add(table, key, numbers + i));
  }

  // Start removing elements.
  for (size_t i = 0; i < N; i++) {
    
    char key[16];
    snprintf(key, sizeof(key), "key_%zu", i);

    assert(hash_table_remove(table, key));
    assert(hash_table_get_size(table) == N - i - 1);

    // Check all remaining elements have the correct value.
    for (size_t j = i + 1; j < N; j++) {
    
      snprintf(key, sizeof(key), "key_%zu", j);
    
      int *value = hash_table_get(table, key);

      assert(value == (numbers + j));
    }
  }

  hash_table_free(table);
}

int main(void) {
  hash_table_tests_create();
  hash_table_tests_add();
  hash_table_tests_add_duplicate();
  hash_table_tests_remove();
  hash_table_tests_remove_empty();
  hash_table_tests_get_empty();
  hash_table_tests_add_many();
  hash_table_tests_remove_many();

  return 0;
}
