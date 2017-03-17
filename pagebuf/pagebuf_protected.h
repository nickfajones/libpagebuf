/*******************************************************************************
 *  Copyright 2017 Nick Jones <nick.fa.jones@gmail.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ******************************************************************************/

#ifndef PAGEBUF_PROTECTED_H
#define PAGEBUF_PROTECTED_H


#include "pagebuf.h"


#ifdef __cplusplus
extern "C" {
#endif


/** pagebuf_protected
 *
 * This header defines functions and objects that are used internally by
 * pb_buffer, and in particular by pb_trivial_buffer.
 *
 * Although these interfaces shouldn't be used directly by authors, they
 * provide base functionality that may be of use to implementors of pb_buffer
 * subclasses, such as pb_mmap_buffer.
 *
 * These functions and objects are defined here, clearly but separately, so
 * that they do not clutter the definitions of the public interfaces.
 */






/** Get a built in, trivial set of pb_allocator operations.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_allocator_operations *pb_get_trivial_allocator_operations(void);



/** Implementations of allocator operations for the trivial allocator. */
void *pb_trivial_allocator_alloc(
                               const struct pb_allocator *allocator,
                               enum pb_allocator_alloc_type type, size_t size);
void pb_trivial_allocator_free(const struct pb_allocator *allocator,
                               enum pb_allocator_alloc_type type,
                               void *obj, size_t size);




/* Pre-declare data operations. */
struct pb_data_operations;



/** Indicates the responsibility a pb_data instance has over its memory region.
 *
 * owned: the memory region is owned by the pb_data instance, usually because
 *        the memory region was created when the pb_data instance was created,
 *        but this may not always be the case.
 *        When the pb_data instance is destroyed, it is responsible for also
 *        freeing the memory region.
 *
 * referenced: the memory region is merely referenced by the pb_data instance
 *             as the pb_data instance has no control or even awareness of the
 *             origins of that memory region.
 *             When the pb_data instance is detroyed, it will simply NULLify
 *             the base address pointer.
 */
enum pb_data_responsibility {
  pb_data_responsibility_owned,
  pb_data_responsibility_referenced,
};



/** Reference counted structure that directly represents a memory region.
 *
 * Authors:
 * The pb_data object is used internally by pb_buffer classes and occurrences
 * of pb_data leaking out to the public API should be considered a bug.
 *
 * Implementors:
 * Each pb_data instance has a one-to-one relationship to its memory region,
 * whether that region is owned or referenced.  The object used as the
 * descriptor of the bounds of the data region, the data_vec member, is
 * immutable and will not change during the lifetime of the pb_data instance.
 *
 * Where a pb_data instance owns its memory region, the instance and the
 * region are usually created at the same time, with the same allocator,
 * ideally in a factory function (such as pb_trivial_data_create)
 * Where a pb_data instance merely references an externally allocated memory
 * region, the memory region will exist both before the pb_data instance is
 * created and after it is destroyed.  The memory region is simply described by
 * the pb_data instance.
 *
 * Instances of pb_data must be created using the create routines below, but
 * they should never be explicitly destroyed.  Instead the get and put
 * functions should be used to maintain the use_count of pb_data instances:
 * pb_data_get is to be called by new owners of a pb_data_instance,
 * pb_data_put is to be called when an owner no longer needs a pb_data
 * instance.  When a call to pb_data_put finds a zero use_count it must
 * destroy the instance.
 *
 * Seeing as the pb_data class has such a close relationship with the
 * memory region it references, pb_buffer subclasses that enact particular
 * internal behaviour will find it neccessary to also subclass pb_data.
 */
struct pb_data {
  /** The description of the region: base memory address and size (length).
   *  Cannot be changed after creation of the parent pb_data instance.
   */
  struct pb_data_vec data_vec;

  /** Responsibility that the instance has over its memory region. */
  enum pb_data_responsibility responsibility;

  /** Use count.  How many pb_page instances reference this data (see later) */
  uint16_t use_count;

  /** Operations for the pb_data instance. */
  const struct pb_data_operations *operations;

  /** The allocator used to allocate memory blocks for this struct and the
   *  referenced memory region if th responsibility is 'owned'.  The purpose
   *  of encouraging this consistency is so that not only is the pb_data
   *  instance destroyed by the allocator that created it, but so is the memory
   *  region.  Unless a pb_data subclass can find another way to ensure similar
   *  consistency.
   */
  const struct pb_allocator *allocator;
};



/** The structure that holds the operations that implement pb_data
 *  functionality.
 */
struct pb_data_operations {
  /** Increment the use count of the pb_data instance. */
  void (*get)(struct pb_data * const data);

  /** Decrement the use count of the pb_data instance.
   *
   * Will destroy the instance if the use count becomes zero.
   * The memory region of the instance will be freed if it is 'owned' by the
   * instance.  The memory block associated with the pb_data instance itself
   * will be freed.
   */
  void (*put)(struct pb_data * const data);
};



/** Functional interfaces for the pb_data class.
 *
 * These interfaces should be used to invoke the data operations.
 *
 * Given pb_data intances are protected members of buffers, these functions
 * are not intended to be called by authors.
 */
void pb_data_get(struct pb_data * const data);
void pb_data_put(struct pb_data * const data);



/** Functional interfaces for accessing a memory region through the
 *  pb_data class.
 *
 * Given pb_data intances are protected members of buffers, these functions
 * are not intended to be called by authors.
 */
void *pb_data_get_base(const struct pb_data *data);
void *pb_data_get_base_at(
                       const struct pb_data *data, size_t offset);
size_t pb_data_get_len(const struct pb_data *data);






/** The trivial data implementation and its supporting functions.
 *
 * The trivial data doesn't need to add anything to the pb_data base structure.
 *
 * Below is a built in, trivial set of pb_data operations.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_data_operations *pb_get_trivial_data_operations(void);



/** Factory functions for trivial data, using trivial operations.
 *
 * These are protected functions and should not be called externally.
 *
 * The 'create' function creates an instances and an 'owned' memory region of
 * the given size.
 *
 * The 'create_ref' function receives a pointer to a memory region and its size
 * and creates a pb_data instance that is 'referenced'.
 */
struct pb_data *pb_trivial_data_create(size_t len,
                                       const struct pb_allocator *allocator);

struct pb_data *pb_trivial_data_create_ref(
                                       const uint8_t *buf, size_t len,
                                       const struct pb_allocator *allocator);

/** Trivial implementations of pb_data reference count management functions.
 *  pb_trivial_data_put also performs object cleanup amd memory block frees
 *  through the associated allocator.
 *
 * These are protected functions and should not be called externally.
 */
void pb_trivial_data_get(struct pb_data * const data);
void pb_trivial_data_put(struct pb_data * const data);






/** Functional interfaces for accessing a memory region through pb_page.
 *
 * These functions are public and may be called by authors.
 */
void *pb_page_get_base(const struct pb_page *page);
void *pb_page_get_base_at(
                       const struct pb_page *page, size_t offset);
size_t pb_page_get_len(const struct pb_page *page);



/** Create a pb_page instance.
 *
 * The pb_data instance has its use count incremented.
 *
 * This is a protected function and should not be called externally.
 */
struct pb_page *pb_page_create(struct pb_data *data,
                               const struct pb_allocator *allocator);

/** Create a pb_page using properties of another.
 *
 * The new pb_page instance will duplicate the base and len values of the
 * source page, and reference pb_data instance of the source page.
 *
 * This is a protected function and should not be called externally.
 */
struct pb_page *pb_page_transfer(const struct pb_page *src_page,
                                 size_t len, size_t src_off,
                                 const struct pb_allocator *allocator);

/** Destroy a pb_page instance.
 *
 * The pb_data instance will be de-referenced once.
 *
 * This is a protected function and should not be called externally.
 */
void pb_page_destroy(struct pb_page *page,
                     const struct pb_allocator *allocator);



/** Utility function to set the data object of a page.
 *
 * This is a protected function and should not be called externally.
 */
void pb_page_set_data(struct pb_page * const page,
                      struct pb_data * const data);






/** The trivial buffer implementation and its supporting functions.
 *
 * The trivial buffer is a reference implementation of pb_buffer.
 *
 * Because the trivial buffer uses heap based memory allocations (by default),
 * and defines operations that support all strategy options, it is maximally
 * flexible, meaning authors can tweak any of the strategy parameters when
 * creating a trivial buffer instance.
 */
struct pb_trivial_buffer {
  struct pb_buffer buffer;

  struct pb_page page_end;

  uint64_t data_revision;
  uint64_t data_size;
};



/** Get a trivial buffer strategy.
 *
 * This default, immutable, buffer strategy for trivial buffer is flexible and
 * efficient: allowing zero copy transfers, minimal fragmentation of transfers,
 * and allowing insertion operations.  The trivial buffer was designed with
 * TCP networked systems, using non-blocking IO, in mind, where data read from
 * sockets can be highly fragmented, and parsing and splitting of network
 * data can be intensive.
 *
 * page_size: 4096
 *
 * clone_on_write: false: zero copy transfer of data from other buffers
 *
 * fragment_as_source: false: fragments written from other buffers or memory
 *                            regions are not additionally fragmented within
 *                            the 4k page limit
 *
 * rejects_insert: false: inserts into the middle of the buffer are allowed.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void);



/** Get a trivial buffer operations structure.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_buffer_operations *pb_get_trivial_buffer_operations(void);



/** Specific implementations of pb_trivial_buffer operations.
 *
 * These are protected functions and should not be called externally.
 */
uint64_t pb_trivial_buffer_get_data_revision(
                                         struct pb_buffer * const buffer);
uint64_t pb_trivial_buffer_get_data_size(struct pb_buffer * const buffer);


void pb_trivial_buffer_get_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_trivial_buffer_get_end_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_trivial_buffer_is_end_iterator(
                            struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *buffer_iterator);
bool pb_trivial_buffer_cmp_iterator(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *lvalue,
                            const struct pb_buffer_iterator *rvalue);
void pb_trivial_buffer_next_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_trivial_buffer_prev_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);


void pb_trivial_buffer_get_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_trivial_buffer_get_end_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_trivial_buffer_is_end_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_trivial_buffer_cmp_byte_iterator(struct pb_buffer * const buffer,
                         const struct pb_buffer_byte_iterator *lvalue,
                         const struct pb_buffer_byte_iterator *rvalue);
void pb_trivial_buffer_next_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_trivial_buffer_prev_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);


uint64_t pb_trivial_buffer_insert(
                              struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
                              size_t offset,
                              struct pb_page * const page);
uint64_t pb_trivial_buffer_extend(
                              struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_reserve(
                              struct pb_buffer * const buffer,
                              uint64_t size);
uint64_t pb_trivial_buffer_rewind(
                              struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_seek(struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_trim(struct pb_buffer * const buffer,
                              uint64_t len);


uint64_t pb_trivial_buffer_insert_data(
                              struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
                              size_t offset,
                              const void *buf,
                              uint64_t len);
uint64_t pb_trivial_buffer_insert_data_ref(
                              struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
                              size_t offset,
                              const void *buf,
                              uint64_t len);
uint64_t pb_trivial_buffer_insert_buffer(
                              struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
                              size_t offset,
                              struct pb_buffer * const src_buffer,
                              uint64_t len);


uint64_t pb_trivial_buffer_write_data(struct pb_buffer * const buffer,
                                      const void *buf,
                                      uint64_t len);
uint64_t pb_trivial_buffer_write_data_ref(
                                      struct pb_buffer * const buffer,
                                      const void *buf,
                                      uint64_t len);
uint64_t pb_trivial_buffer_write_buffer(
                                      struct pb_buffer * const buffer,
                                      struct pb_buffer * const src_buffer,
                                      uint64_t len);


uint64_t pb_trivial_buffer_overwrite_data(struct pb_buffer * const buffer,
                                          const void *buf,
                                          uint64_t len);
uint64_t pb_trivial_buffer_overwrite_buffer(
                                          struct pb_buffer * const buffer,
                                          struct pb_buffer * const src_buffer,
                                          uint64_t len);


uint64_t pb_trivial_buffer_read_data(struct pb_buffer * const buffer,
                                     void * const buf,
                                     uint64_t len);


void pb_trivial_buffer_clear(struct pb_buffer * const buffer);
void pb_trivial_pure_buffer_clear(
                             struct pb_buffer * const buffer);

void pb_trivial_buffer_destroy(
                             struct pb_buffer * const buffer);


/** Utility functions for the trivial buffer implementation.
 *
 * These are protected functions and should not be called externally.
 */
void pb_trivial_buffer_increment_data_revision(
                                         struct pb_buffer * const buffer);

void pb_trivial_buffer_increment_data_size(
                                         struct pb_buffer * const buffer,
                                         uint64_t size);
void pb_trivial_buffer_decrement_data_size(
                                         struct pb_buffer * const buffer,
                                         uint64_t size);

struct pb_page *pb_trivial_buffer_page_create(
                              struct pb_buffer * const buffer,
                              size_t len);
struct pb_page *pb_trivial_buffer_page_create_ref(
                              struct pb_buffer * const buffer,
                              const uint8_t *buf, size_t len);

/** Duplicate the data of a page and set the duplicate into the page.
 *
 * The existing data in a page has its data memory region duplicated into a
 * new pb_data object.  The new pb_data object replaces the old data object in
 * the page.
 *
 * This is primarily done when an overwrite operation is performed on a buffer
 * that has a strategy setting of clone_on_write = false.  In this case, the
 * data will be duplicated to prevent the overwrite operation from interfering
 * with data objects that are shared with other buffers.
 * It will also be done if the target data objet references a memory region.
 */
bool pb_trivial_buffer_dup_page_data(struct pb_buffer * const buffer,
                                     struct pb_page * const page);



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAGEBUF_PROTECTED_H */
