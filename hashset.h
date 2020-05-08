/*
 *     Copyright 2012 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct hashset_st {
        size_t nbits;
        size_t mask;

        size_t capacity;
        size_t *items;
        size_t nitems;
        size_t n_deleted_items;
    };

    typedef struct hashset_st *hashset_t;

    /* create hashset instance */
    hashset_t hashset_create(void);

    /* destroy hashset instance */
    void hashset_destroy(hashset_t set);

    size_t hashset_num_items(hashset_t set);

    /* add item into the hashset.
     *
     * @note 0 and 1 is special values, meaning nil and deleted items. the
     *       function will return -1 indicating error.
     *
     * returns zero if the item already in the set and non-zero otherwise
     */
    int hashset_add(hashset_t set, void *item);

    /* remove item from the hashset
     *
     * returns non-zero if the item was removed and zero if the item wasn't
     * exist
     */
    int hashset_remove(hashset_t set, void *item);

    /* check if existence of the item
     *
     * returns non-zero if the item exists and zero otherwise
     */
    int hashset_is_member(hashset_t set, void *item);


struct hashset_itr_st {
  hashset_t set;
  size_t index;
};

typedef struct hashset_itr_st *hashset_itr_t;

/* create a hashset iterator, advances to first value is available*/
hashset_itr_t hashset_iterator(hashset_t set);

/* returns the value at the current index */
size_t hashset_iterator_value(hashset_itr_t itr);

/* return 1 is can advance, 0 otherwise */
int hashset_iterator_has_next(hashset_itr_t itr);

/*
 * check if iterator can advance, if so advances
 * returns current index if can advance and -1 otherwise
 */
int hashset_iterator_next(hashset_itr_t itr);


hashset_itr_t hashset_iterator(hashset_t set)
{
  hashset_itr_t itr = calloc(1, sizeof(struct hashset_itr_st));
  if (itr == NULL)
    return NULL;

  itr->set = set;
  itr->index = 0;

  /* advance to the first item if one is present */
  if (set->nitems > 0)
    hashset_iterator_next(itr);

  return itr;
}

int hashset_iterator_has_next(hashset_itr_t itr)
{
  size_t index;

  /* empty or end of the set */
  if (itr->set->nitems == 0 || itr->index == itr->set->capacity)
    return 0;

  /* peek to find another entry */
  index = itr->index;
  while(index < itr->set->capacity)
  {
    size_t value = itr->set->items[index++];
    if(value != 0)
      return 1;
  }

  /* Otherwise */
  return 0;

}

int hashset_iterator_next(hashset_itr_t itr)
{

  if(hashset_iterator_has_next(itr) == 0)
    return -1; /* Can't advance */

  itr->index++;

  while (itr->set->items[(itr->index)] == 0 && itr->index < itr->set->capacity) {
    itr->index++;
  }

  return itr->index;
}

size_t hashset_iterator_value(hashset_itr_t itr) {

  /* Check to verify we're not at a null value, this can happen if an iterator is created
   * before items are added to the set.
   */
  if(itr->set->items[itr->index] == 0)
  {
    hashset_iterator_next(itr);
  }

  return itr->set->items[itr->index];
}
static const unsigned int prime_1 = 73;
static const unsigned int prime_2 = 5009;

hashset_t hashset_create(void)
{
    hashset_t set = calloc(1, sizeof(struct hashset_st));

    if (set == NULL) {
        return NULL;
    }
    set->nbits = 3;
    set->capacity = (size_t)(1 << set->nbits);
    set->mask = set->capacity - 1;
    set->items = calloc(set->capacity, sizeof(size_t));
    if (set->items == NULL) {
        hashset_destroy(set);
        return NULL;
    }
    set->nitems = 0;
    set->n_deleted_items = 0;
    return set;
}

size_t hashset_num_items(hashset_t set)
{
    return set->nitems;
}

void hashset_destroy(hashset_t set)
{
    if (set) {
        free(set->items);
    }
    free(set);
}

static int hashset_add_member(hashset_t set, void *item)
{
    size_t value = (size_t)item;
    size_t ii;

    if (value == 0 || value == 1) {
        return -1;
    }

    ii = set->mask & (prime_1 * value);

    while (set->items[ii] != 0 && set->items[ii] != 1) {
        if (set->items[ii] == value) {
            return 0;
        } else {
            /* search free slot */
            ii = set->mask & (ii + prime_2);
        }
    }
    set->nitems++;
    if (set->items[ii] == 1) {
        set->n_deleted_items--;
    }
    set->items[ii] = value;
    return 1;
}

static void maybe_rehash(hashset_t set)
{
    size_t *old_items;
    size_t old_capacity, ii;


    if (set->nitems + set->n_deleted_items >= (double)set->capacity * 0.85) {
        old_items = set->items;
        old_capacity = set->capacity;
        set->nbits++;
        set->capacity = (size_t)(1 << set->nbits);
        set->mask = set->capacity - 1;
        set->items = calloc(set->capacity, sizeof(size_t));
        set->nitems = 0;
        set->n_deleted_items = 0;
        assert(set->items);
        for (ii = 0; ii < old_capacity; ii++) {
            hashset_add_member(set, (void *)old_items[ii]);
        }
        free(old_items);
    }
}

int hashset_add(hashset_t set, void *item)
{
    int rv = hashset_add_member(set, item);
    maybe_rehash(set);
    return rv;
}

int hashset_remove(hashset_t set, void *item)
{
    size_t value = (size_t)item;
    size_t ii = set->mask & (prime_1 * value);

    while (set->items[ii] != 0) {
        if (set->items[ii] == value) {
            set->items[ii] = 1;
            set->nitems--;
            set->n_deleted_items++;
            return 1;
        } else {
            ii = set->mask & (ii + prime_2);
        }
    }
    return 0;
}

int hashset_is_member(hashset_t set, void *item)
{
    size_t value = (size_t)item;
    size_t ii = set->mask & (prime_1 * value);

    while (set->items[ii] != 0) {
        if (set->items[ii] == value) {
            return 1;
        } else {
            ii = set->mask & (ii + prime_2);
        }
    }
    return 0;
}
