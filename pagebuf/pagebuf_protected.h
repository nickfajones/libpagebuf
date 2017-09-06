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
void *pb_trivial_allocator_malloc(
                               const struct pb_allocator *allocator,
                               size_t size);
void *pb_trivial_allocator_calloc(
                               const struct pb_allocator *allocator,
                               size_t size);
void *pb_trivial_allocator_realloc(
                               const struct pb_allocator *allocator,
                               void *obj, size_t oldsize, size_t newsize);
void pb_trivial_allocator_free(const struct pb_allocator *allocator,
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






/** Non-exclusive owner of a pb_data instance, holding a modifiable reference
 *  to the memory region.
 *
 * The pb_page structure is used by buffers to represent a portion of a memory
 * region.  Buffers internally will maintain an ordered list of these pb_page
 * structures and the entirety if this list represents the data contained in
 * the buffer.
 *
 * There being multiple pages in the buffer represents the notion of the
 * fragmentation of the buffers' view of that data, either because of the
 * configuration of the buffer or because of the pattern that data is written
 * into the buffer (or both).
 *
 * A pb_page instance will always remain inside its parent buffer.  It will be
 * consumed or manipulated in response to requests to consome or manipulate the
 * data in the buffer.  The pb_page is a 'dumb' object, meaning there is no
 * scope to subclass it for the purpose of implementing a new class of buffer.
 * If an author wishes to create a new buffer class, they should explore
 * subclassing the pb_allocator, pb_data and pb_buffer itself, but the role and
 * implementation of pb_page must remain generic.
 *
 * While a pb_data instance represents an unchanging, direct, reference to a
 * memory region and also closely models that memory region's lifecycle, the
 * pb_page object represents the opposite:
 *
 * pb_data, while maintaining a description of the same memory region, may
 * change that description, and the lifecycle of the pb_page is only indirectly
 * tied to that of the pb_data instance and the memory region.
 *
 * Although pb_page will only every reference one pb_data and memory region,
 * and will never adjust the pb_data description of the memory region, it can
 * adjust it's own.  However, the pb_page description of the memory region will
 * always reside within the pb_data's description of the same region.  This
 * means the pb_page description will be either equal too or strictly a
 * subset of the pb_data description, in regard to both value of the base
 * address and the length:
 *
 * data.base <= page.base + page.len <= data.base + data.len
 *
 * A pb_page is either initialised with a new pb_data instance, in the case
 * where a buffer is extended to grow its capacity.  In the case of a write
 * from one buffer to another, depending on the target buffers' internal
 * implementation, the properties of the pb_page of the source buffer will be
 * copied into a new pb_page of the target buffer, but the pb_data from the
 * source pb_page will actually be shared between the two pb_pages and the
 * two buffers.  This is the notion of zero copy, where writes don't cause the
 * data to be copied, instead, a second reference to the same data is created.
 *
 * When a pb_page instance is initialised with either a new pb_data or an
 * existing one, the pb_page will will increment the reference count of the
 * pb_data instance using the pb_data_get function.  This will ensure the
 * pb_data instance says alive for as long as the pb_page instance is alive.
 * When the pb_page instance reaches the end of its lifecycle and is destroyed,
 * the reference count to the pb_data will be decremented using the pb_data_get
 * function.  Only when the pb_data reference count reaches zero, meaning all
 * pb_page instances referencing the pb_data are destroyed, will the pb_data
 * itself be destroyed.
 */
struct pb_page {
  /** The description of the referenced pb_data memory region description. */
  struct pb_data_vec data_vec;

  /** The reference to the pb_data instance. */
  struct pb_data *data;

  /** Previous page in a buffer structure. */
  struct pb_page *prev;
  /** Next page in a buffer structure */
  struct pb_page *next;
};



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






/** The structure that holds the operations that implement specific
 *  pb_trivial_buffer functionality.
 */
struct pb_trivial_buffer_operations {
  /** The base buffer operations. */
  struct pb_buffer_operations buffer_operations;

  struct pb_page *(*page_create)(struct pb_buffer * const buffer,
                                 size_t len);
  struct pb_page *(*page_create_ref)(
                                 struct pb_buffer * const buffer,
                                 const uint8_t *buf, size_t len);

  /** Duplicate the memory region and the data of a page and set the duplicate
   *  into the page.
   *
   * This is primarily done when the target data object references a memory region
   * that is shared with other pages.
   * It will also be done if an overwrite operation is performed on a buffer
   * that has a strategy setting of clone_on_write = false.  In this case, the
   * data will be duplicated to prevent the overwrite operation from interfering
   * with data objects that are shared with other buffers.
   */
  bool (*dup_page_data)(struct pb_buffer * const buffer,
                        struct pb_page * const page);


  /** Convert an iterator into an underlying pb_page structure.
   *
   * The returned pb_page object will still remain attached to the iterator
   * and will not be cleaned up or deleted by the caller.
   */
  struct pb_page *(*resolve_iterator)(
                        struct pb_buffer * const buffer,
                        const struct pb_buffer_iterator *buffer_iterator);
};






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

  /** The anchor node of the pb_page list.
   */
  struct pb_page page_end;

  /** Revision: A monotonic counter describing the 'revision' state of the
   *  buffer.
   */
  uint64_t data_revision;

  /** A running total buffer size counter.
   *
   * Kept up to date after any operation that adds or removes data from the
   * buffer, it is used to describe the buffer size in an optimised way, to
   * avoid iterating and measuring all pages whenever the buffer size is
   * queried.  Debug builds will do an iterate and measure and compare the
   * result to the data_size to assure correctness.
   */
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


/** Insert a pb_page instance into the pb_buffer.
 *
 * buffer_iterator: the position in the buffer: page and page offset, before
 *                  which the new page will be inserted.
 *
 * offset: the position within the iterator page, before which the page will
 *         be inserted.
 *
 * page: the new page to insert into the buffer.  This page is created
 *       elsewhere within the buffer, using the buffers allocator.
 *
 * If the iterator offset is zero, the new page will be inserted in front of
 * the iterator page.  If the offset is non-zero, the iterator page will be
 * split into two sub-pages at the point of the offset, and the new page will
 * be inserted between them.
 *
 * The return value is the amount of data successfully inserted into the
 * buffer.
 *
 * This is a protected function and should not be called externally.
 */
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


/** Implementations of unique Trivial buffer operations. */
struct pb_page *pb_trivial_buffer_page_create(
                            struct pb_buffer * const buffer,
                            size_t len);
struct pb_page *pb_trivial_buffer_page_create_ref(
                            struct pb_buffer * const buffer,
                            const uint8_t *buf, size_t len);

bool pb_trivial_buffer_dup_page_data(
                            struct pb_buffer * const buffer,
                            struct pb_page * const page);

struct pb_page *pb_trivial_buffer_resolve_iterator(
                            struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *buffer_iterator);


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



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAGEBUF_PROTECTED_H */
