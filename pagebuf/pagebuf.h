/*******************************************************************************
 *  Copyright 2015, 2016 Nick Jones <nick.fa.jones@gmail.com>
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

#ifndef PAGEBUF_H
#define PAGEBUF_H


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif



/** Indicates how the allocated memory region will be used. */
enum pb_allocator_type {
  pb_alloc_type_struct =                                  1,
  pb_alloc_type_region =                                  2,
};

/** Wrapper for allocation and freeing of data regions
 *
 * Allocate data memory regions through these interfaces.
 *
 * Allocators must zero memory regions designated for use in structs.
 *
 * These functions receive a reference to their parent as a parameter so that
 * implementations may use the pointer to hold additional information.
 */
struct pb_allocator {
  /** Allocate a memory region.
   *
   * type indicates whether the memory allocated will be used for a data
   * structure, or as a memory region.
   *
   * size indicates the size of the memory region to allocate.
   */
  void *(*alloc)(const struct pb_allocator *allocator,
                 enum pb_allocator_type type, size_t size);
  /** Free a memory region.
   *
   * type indicates whether the memory allocated was used for a data
   * structure, or as a memory region.
   *
   * obj is the address of beginning of the memory region.
   *
   * size indicates the size of the memory region that was allocated.
   */
  void  (*free)(const struct pb_allocator *allocator,
                enum pb_allocator_type type, void *obj, size_t size);
};

/** Functional interfaces for the pb_allocator class. */
void *pb_allocator_alloc(
                       const struct pb_allocator *allocator,
                       enum pb_allocator_type type, size_t size);
void pb_allocator_free(const struct pb_allocator *allocator,
                       enum pb_allocator_type type, void *obj, size_t size);


/** Get a built in, trivial heap based allocator. */
const struct pb_allocator *pb_get_trivial_allocator(void);



/** The pb_data and its supporting classes and functions. */
struct pb_data;



/** A structure used to represent a data region.
 */
struct pb_data_vec {
  /** The starting address of the region. */
  uint8_t *base;
  /** The length of the region. */
  size_t len;
};



/** Responsibility that the pb_data instance has over the data region.
 *
 * Either owned, thus freed when use count reaches zero,
 * or merely referenced.
 */
enum pb_data_responsibility {
  pb_data_none =                                          0,
  pb_data_owned,
  pb_data_referenced,
};



/** The structure that holds the operations that implement the pb_data
 *  functionality.
 *
 * A pb_buffer should tie data operations to each data instance so that
 * data instances can be destroyed in the correct way even if they are passed
 * outside of their creating pb_buffer.
 */
struct pb_data_operations {
  /** Increment the use count of the pb_data instance. */
  void (*get)(struct pb_data * const data);

  /** Decrement the use count of the pb_data instance.
   *
   * Will destroy the instance if the use count becomes zero.
   * The memory region of the instance will be freed if it is owned by the
   * instance.  The pb_data instance itself will be freed.
   */
  void (*put)(struct pb_data * const data);
};



/** Reference counted structure that represents a memory region.
 *
 * Instances of this object are created using the create routines below, but
 * it should rarely be explicitly destroyed.  Instead the get and put
 * functions should be used when accepting or releasing an instance.
 * When a call to put finds a zero use count it will internally call destroy.
 */
struct pb_data {
  /** The bounds of the region. */
  struct pb_data_vec data_vec;

  /** Responsibility that this structure has over the memory region. */
  enum pb_data_responsibility responsibility;

  /** Use count, maintained with atomic operations. */
  uint16_t use_count;

  /** Operations for the pb_buffer instance.  Cannot be changed after creation. */
  const struct pb_data_operations *operations;

  /** Allocator used to allocate storage for this struct and its owned
   *  memory region. */
  const struct pb_allocator *allocator;
};



/** Functional interfaces for the generic pb_data class. */
void pb_data_get(struct pb_data * const data);
void pb_data_put(struct pb_data * const data);



/** The trivial data implementation and its supporting functions. */
const struct pb_data_operations *pb_get_trivial_data_operations(void);



/** Trivial implementation of the pb_data operations. */
struct pb_data *pb_trivial_data_create(size_t len,
                                       const struct pb_allocator *allocator);

struct pb_data *pb_trivial_data_create_ref(
                                       const uint8_t *buf, size_t len,
                                       const struct pb_allocator *allocator);


void pb_trivial_data_get(struct pb_data * const data);
void pb_trivial_data_put(struct pb_data * const data);



/** Reference to a pb_data instance and a bounded subset of its memory region.
 *
 * Embodies the consumption of the memory region referenced by the
 * pb_data instance.
 * Multiple pb_page instances may refer to one pb_data instance, each
 * increasing the usage count.
 *
 * Because a page stays always within the boundary of its owner buffer, it
 * needn't carry an allocator with it.
 */
struct pb_page {
  /** The offset into the referenced region, and the length of the reference */
  struct pb_data_vec data_vec;

  /** The reference to the pb_data instance. */
  struct pb_data *data;

  /** Previous page in a buffer structure. */
  struct pb_page *prev;
  /** Next page in a buffer structure */
  struct pb_page *next;
};

/** Create a pb_page instance.
 *
 * pb_data instance has its use count incremented.
 */
struct pb_page *pb_page_create(struct pb_data *data,
                               const struct pb_allocator *allocator);

/** Transfer pb_page data.
 *
 * Duplicate the base and len values of the source page, and reference its
 * pb_data instance.
 */
struct pb_page *pb_page_transfer(const struct pb_page *src_page,
                                 size_t len, size_t src_off,
                                 const struct pb_allocator *allocator);

/** Destroy the pb_page instance.
 *
 * pb_data instance will be released.
 * pb_page instance will be cleared and freed.
 */
void pb_page_destroy(struct pb_page *page,
                     const struct pb_allocator *allocator);






/** The pb_buffer and its supporting classes and functions. */
struct pb_buffer;



/** A structure used to iterate over buffer pages. */
struct pb_buffer_iterator {
  struct pb_page *page;
};



/** Functional interfaces for the pb_buffer_iterator class. */
uint8_t * pb_buffer_iterator_get_base(
                             const struct pb_buffer_iterator *buffer_iterator);
size_t pb_buffer_iterator_get_len(
                             const struct pb_buffer_iterator *buffer_iterator);



/** A structure used to iterate over buffer bytes. */
struct pb_buffer_byte_iterator {
  struct pb_buffer_iterator buffer_iterator;

  size_t page_offset;

  const char *current_byte;
};



/** Functional interfaces for the pb_buffer_byte_iterator class. */
const char *pb_buffer_byte_iterator_get_current_byte(
                   const struct pb_buffer_byte_iterator *buffer_byte_iterator);



/** The default page size of pb_buffer pages. */
#define PB_BUFFER_DEFAULT_PAGE_SIZE                       4096



/** A structure that describes the internal strategy of a pb_buffer.
 *
 * A buffer strategy controls the way that a buffer will carry out actions such
 * as writing of data, or management of memory regions.
 */
struct pb_buffer_strategy {
  /** The size of data regions that the pb_buffer will internally dynamically
   *  allocate.
   *
   * If this value is zero, there will be no limit on fragment size,
   * so write operations will cause allocations of data regions equal in size
   * to the length of the source data. */
  size_t page_size;

  /** Indicates whether data written to the buffer from another buffer is:
   *
   * not_cloned (false):reference to the pb_data instance is incremented.
   *
   * cloned     (true): new pb_data instance created and memory regions copied.
   */
  bool clone_on_write;

  /** Indicates how data written to the pb_buffer will be fragmented.
   *
   * as source (false): source data fragmentation takes precedence.
   *                    When clone_on_write is false, source pages are
   *                    moved to the target as is and pb_data references are
   *                    incremented.  Source pages are only fragmented by
   *                    truncation to fit the operation length value.
   *                    When clone_on_write is true, source pages are
   *                    fragmented according to the lower of the source
   *                    fragment size, and the target page_size, which may be
   *                    zero, in which case the source fragment size is used.
   * as target  (true): target pb_buffer page_size takes precedence.
   *                    When clone_on_write is false, source fragments that
   *                    are greater than the target page_size limit are split.
   *                    When clone_on_write is true, source fragments will be
   *                    packed into target fragments up to the target page_size
   *                    in size.
   */
  bool fragment_as_target;

  /** Indicates whether a pb_buffer rejects (fails to support) insert operations.
   *
   * no reject (false): insert operations can be performed and the buffer
   *                     update its data view appropriately.
   * reject     (true): insert operations will immediately return 0.
   */
  bool rejects_insert;
};



/** The structure that holds the operations that implement pb_buffer
 *  functionality.
 *
 * An author should also define a group of factory functions to opaquely
 * create their pb_buffer instance and bind it to their customised
 * pb_buffer_operations structure.  See the pb_buffer_create* group of factory
 * functions as an example of standard pb_buffer factory functionality.
 */
struct pb_buffer_operations {
  /** Return a revision stamp of the data.
   *
   * This value is incremented by pb_buffer implementations whenever the
   * pb_buffer is:
   * - seek'd
   * - rewind'd
   * - overwritten
   * - cleared
   *
   * External users such as line readers can retain and compare this value
   * to determine whether the pb_buffer instance has been invalidated since
   * last use, and therefore needs to be re-processed.
   */
  uint64_t (*get_data_revision)(struct pb_buffer * const buffer);
  /** Increment data revision, indicating that iterators and readers are no
   * longer valid.
   *
   * This is a private function and should not be called externally.
   */
  void (*increment_data_revision)(
                                struct pb_buffer * const buffer);

  /** Return the amount of data in the buffer, in bytes. */
  uint64_t (*get_data_size)(struct pb_buffer * const buffer);


  /** Initialise an iterator to the start of the pb_buffer instance data.
   *
   * The iterator should be a pointer to an object on the stack that must
   * be manipulated only by the iterator methods of the same buffer instance.
   */
  void (*get_iterator)(struct pb_buffer * const buffer,
                       struct pb_buffer_iterator * const buffer_iterator);
  /** Initialise an iterator to the end of the pb_buffer instance data.
     *
     * The iterator should be a pointer to an object on the stack that must
     * be manipulated only by the iterator methods of the same buffer instance.
     */
  void (*get_iterator_end)(struct pb_buffer * const buffer,
                           struct pb_buffer_iterator * const buffer_iterator);
  /** Indicates whether an iterator has traversed to the end of a buffers
   *  internal chain of pages.
   *
   * This function must always be called, and return false, before the data
   * vector of the pb_page of the iterator can be used.  The value of the
   * pb_page pointer is undefined when the iterator end function returns true.
   */
  bool (*iterator_is_end)(struct pb_buffer * const buffer,
                          struct pb_buffer_iterator * const buffer_iterator);

  bool (*iterator_cmp)(struct pb_buffer * const buffer,
                       const struct pb_buffer_iterator *lvalue,
                       const struct pb_buffer_iterator *rvalue);

  /** Increments an iterator to the next pb_page in a buffer's internal chain. */
  void (*iterator_next)(struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator);
  /** Decrements an iterator to the previous pb_page in a buffer's internal
   *  chain.
   *
   * It is valid to call this function on an iterator that is the end
   * iterator, according to is_iterator_end.  If this function is called on such
   * an iterator, the buffer implementation must correctly decrement back to the
   * position before end in this case. */
  void (*iterator_prev)(struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator);

  /** Initialise a byte iterator to the start of the pb_buffer instance data.
   *
   * The byte iterator should be a pointer to an object on the stack that must
   * be manipulated only by the iterator methods of the same buffer instance.
   */
  void (*get_byte_iterator)(struct pb_buffer * const buffer,
                            struct pb_buffer_byte_iterator * const
                              buffer_byte_iterator);
  /** Initialise an iterator to the end of the pb_buffer instance data.
   *
   * The iterator should be a pointer to an object on the stack that must
   * be manipulated only by the iterator methods of the same buffer instance.
   */
  void (*get_byte_iterator_end)(struct pb_buffer * const buffer,
                                struct pb_buffer_byte_iterator * const
                                  buffer_byte_iterator);
  /** Indicates whether an iterator has traversed to the end of a buffers
   *  internal chain of pages.
   *
   * This function must always be called, and return false, before the data
   * vector of the pb_page of the iterator can be used.  The value of the
   * pb_page pointer is undefined when the iterator end function returns true.
   */
  bool (*byte_iterator_is_end)(struct pb_buffer * const buffer,
                               struct pb_buffer_byte_iterator * const
                                 buffer_byte_iterator);

  bool (*byte_iterator_cmp)(struct pb_buffer * const buffer,
                            const struct pb_buffer_byte_iterator *lvalue,
                            const struct pb_buffer_byte_iterator *rvalue);

  /** Increments an iterator to the next pb_page in a buffer's internal chain. */
  void (*byte_iterator_next)(struct pb_buffer * const buffer,
                             struct pb_buffer_byte_iterator * const
                               buffer_byte_iterator);
  /** Decrements an iterator to the previous pb_page in a buffer's internal
   *  chain.
   *
   * It is valid to call this function on an iterator that is the end
   * iterator, according to is_iterator_end.  If this function is called on such
   * an iterator, the buffer implementation must correctly decrement back to the
   * position before end in this case. */
  void (*byte_iterator_prev)(struct pb_buffer * const buffer,
                             struct pb_buffer_byte_iterator * const
                               buffer_byte_iterator);


  /** Create a pb_page instance with accompanying pb_data.
   *
   * This is a private function and should not be called externally.
   *
   * len is the size of the memory region to allocate.
   * is_rewind indicates whether the region is to be placed at the beginning
   *           of the buffer in a rewind situation.
   */
  struct pb_page *(*page_create)(
                             struct pb_buffer * const buffer,
                             struct pb_buffer_iterator * const buffer_iterator,
                             size_t len,
                             bool is_rewind);
  /** Create a pb_page instance with accompanying pb_data.
   *
   * This is a private function and should not be called externally.
   *
   * buf is the memory region to reference.
   * len is the size of the memory region to allocate.
   * is_rewind indicates whether the region is to be placed at the beginning
   *           of the buffer in a rewind situation.
   *
   * Memory region buf will not owned by the accompanying pb_data instance.
   */
  struct pb_page *(*page_create_ref)(
                             struct pb_buffer * const buffer,
                             struct pb_buffer_iterator * const buffer_iterator,
                             const uint8_t *buf, size_t len,
                             bool is_rewind);


  /** Append a pb_page instance to the pb_buffer.
   *
   * This is a private function and should not be called externally.
   *
   * buffer_iterator is the position in the buffer to update the pb_buffer.  The
   *                 pb_page will be inserted in front of the iterator position.
   * offset is the offset within the iterator page to insert the new pb_page
   *        into.
   * page is the page to insert.  The data base and len values must be set
   *      before this operation is called.
   *
   * If the offset is zero, the new pb_page will be inserted before the
   * iterator page.  If the offset is non zero, then the iterator page may be
   * split according to the pb_buffer instances internal implementation.
   * If the offset is greater than or equal to the iterator page len, then the
   * new pb_page will be inserted after the iterator page, or at the head of
   * the pb_buffer if the iterator is an end iterator.
   *
   * This operation will affect the data size of the pb_buffer instance.
   *
   * The return value is the amount of data actually inserted to the
   * pb_buffer instance.  Users of the insert operation must reflect the value
   * returned back to their own callers.
   */
  uint64_t (*insert)(
                   struct pb_buffer * const buffer,
                   struct pb_buffer_iterator * const buffer_iterator,
                   size_t offset,
                   struct pb_page * const page);
  /** Increase the size of the buffer by adding len bytes of data to the end.
   *
   * This is a private function and should not be called externally.
   *
   * len indicates the amount of data to add in bytes.
   *
   * Depending on the buffer implementation details, the total len may be split
   * across multiple pages.
   */
  uint64_t (*extend)(
                   struct pb_buffer * const buffer,
                   uint64_t len);
  /** Increases the size of the buffer by adding len bytes of data to the head.
   *
   * This is a private function and should not be called externally.
   *
   * len indicates the amount of data to ad in bytes.
   *
   * Depending on the buffer implementation details, the total len may be split
   * across multiple pages.
   */
  uint64_t (*rewind)(
                   struct pb_buffer * const buffer,
                   uint64_t len);
  /** Seek the buffer by len bytes starting at the beginning of the buffer.
   *
   * This is a private function and should not be called externally.
   *
   * len indicates the amount of data to seek, in bytes.
   *
   * The seek operation may cause internal pages to be consumed depending on
   * the buffer implementation details.  These pb_pages will be destroyed during
   * the seek operation and their respective data 'put'd.
   */
  uint64_t (*seek)(struct pb_buffer * const buffer,
                   uint64_t len);
  /** Trim the buffer by len bytes starting at the end of the buffer.
   *
   * This is a private function and should not be called externally.
   *
   * len indicates the amount of data to trim, in bytes.
   *
   * The trim operation may cause internal pages to be consumed depending on
   * the buffer implementation details.  These pb_pages will be destroyed during
   * the trim operation and their respective data 'put'd.
   */
  uint64_t (*trim)(struct pb_buffer * const buffer,
                   uint64_t len);

  /** Write data from a memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the buffer, allocating new storage if
   * necessary.
   */
  uint64_t (*write_data)(struct pb_buffer * const buffer,
                         const uint8_t *buf,
                         uint64_t len);
  /** Write data from a referenced memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source referenced memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the buffer.  As the memory region is
   * referenced, no storage needs to be allocated unless the buffer strategy
   * indicates clone_on_write or fragment_as_target is true.
   */
  uint64_t (*write_data_ref)(struct pb_buffer * const buffer,
                             const uint8_t *buf,
                             uint64_t len);
  /** Write data from a source pb_buffer instance to the pb_buffer instance.
   *
   * src_buffer is the buffer to write from.  This pb_buffer instance will not
   *            have it's data modified, but it is not const because iterator
   *            operations of this pb_buffer may cause internal state changes.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.  This buffer will not be altered by
   * this operation.
   *
   * Data is appended to the tail of the buffer, allocating new storage if
   * necessary.
   */
  uint64_t (*write_buffer)(struct pb_buffer * const buffer,
                           struct pb_buffer * const src_buffer,
                           uint64_t len);

  /** Write data from a memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the buffer, allocating new storage if
   * necessary.
   */
  uint64_t (*insert_data)(struct pb_buffer * const buffer,
                          struct pb_buffer_iterator * const buffer_iterator,
                          size_t offset,
                          const uint8_t *buf,
                          uint64_t len);
  /** Write data from a referenced memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source referenced memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is appended to the tail of the buffer.  As the memory region is
   * referenced, no storage needs to be allocated unless the buffer strategy
   * indicates clone_on_write or fragment_as_target is true.
   */
  uint64_t (*insert_data_ref)(struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              const uint8_t *buf,
                              uint64_t len);
  /** Write data from a source pb_buffer instance to the pb_buffer instance.
   *
   * src_buffer is the buffer to write from.  This pb_buffer instance will not
   *            have it's data modified, but it is not const because iterator
   *            operations of this pb_buffer may cause internal state changes.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.  This buffer will not be altered by
   * this operation.
   *
   * Data is appended to the tail of the buffer, allocating new storage if
   * necessary.
   */
  uint64_t (*insert_buffer)(struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator,
                            size_t offset,
                            struct pb_buffer * const src_buffer,
                            uint64_t len);

  /** Write data from a memory region to the pb_buffer instance.
   *
   * buf indicates the start of the source memory region.
   * len indicates the amount of data to write, in bytes.
   *
   * returns the amount of data written.
   *
   * Data is written from the head of the source pb_buffer.  No new storage will
   * be allocated if len is greater than the size of the target pb_buffer.
   */
  uint64_t (*overwrite_data)(struct pb_buffer * const buffer,
                             const uint8_t *buf,
                             uint64_t len);

  /** Read data from the start of a pb_buffer instance to a data region.
   *
   * buf indicates the start of the target memory region.
   * len indicates the amount of data to read, in bytes.
   *
   * returns the amount of data read into the target buffer.
   */
  uint64_t (*read_data)(struct pb_buffer * const buffer,
                        uint8_t * const buf,
                        uint64_t len);

  /** Clear all data in the buffer. */
  void (*clear)(struct pb_buffer * const buffer);

  /** Destroy a pb_buffer. */
  void (*destroy)(struct pb_buffer * const buffer);
};



/** Represents a buffer of pages, and operations for the manipulation of the
 * pages and the buffers of data therein.
 *
 * The buffer also represents the strategy for allocation and freeing of
 * data buffers.
 */
struct pb_buffer {
  /** Strategy for the pb_buffer instance.  Cannot be changed after creation. */
  const struct pb_buffer_strategy *strategy;

  /** Operations for the pb_buffer instance.  Cannot be changed after creation. */
  const struct pb_buffer_operations *operations;

  /** Allocator used to allocate storage for this struct and its pages. */
  const struct pb_allocator *allocator;
};



/** Functional interfaces for the generic pb_buffer class. */
uint64_t pb_buffer_get_data_revision(struct pb_buffer * const buffer);

uint64_t pb_buffer_get_data_size(struct pb_buffer * const buffer);


void pb_buffer_get_iterator(struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_buffer_get_iterator_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_buffer_iterator_is_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_buffer_iterator_cmp(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *lvalue,
                            const struct pb_buffer_iterator *rvalue);
void pb_buffer_iterator_next(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_buffer_iterator_prev(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);


void pb_buffer_get_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_buffer_get_byte_iterator_end(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_buffer_byte_iterator_is_end(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_buffer_byte_iterator_cmp(
                         struct pb_buffer * const buffer,
                         const struct pb_buffer_byte_iterator *lvalue,
                         const struct pb_buffer_byte_iterator *rvalue);
void pb_buffer_byte_iterator_next(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_buffer_byte_iterator_prev(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);


uint64_t pb_buffer_insert(
                        struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator,
                        size_t offset,
                        struct pb_page * const page);
uint64_t pb_buffer_extend(
                        struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_rewind(
                        struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_seek(struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_trim(struct pb_buffer * const buffer, uint64_t len);


uint64_t pb_buffer_write_data(struct pb_buffer * const buffer,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_buffer_write_data_ref(
                              struct pb_buffer * const buffer,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_buffer_write_buffer(
                              struct pb_buffer * const buffer,
                              struct pb_buffer * const src_buffer,
                              uint64_t len);

uint64_t pb_buffer_insert_data(struct pb_buffer * const buffer,
                               struct pb_buffer_iterator * const buffer_iterator,
                               size_t offset,
                               const uint8_t *buf,
                               uint64_t len);
uint64_t pb_buffer_insert_data_ref(
                               struct pb_buffer * const buffer,
                               struct pb_buffer_iterator * const buffer_iterator,
                               size_t offset,
                               const uint8_t *buf,
                               uint64_t len);
uint64_t pb_buffer_insert_buffer(
                               struct pb_buffer * const buffer,
                               struct pb_buffer_iterator * const buffer_iterator,
                               size_t offset,
                               struct pb_buffer * const src_buffer,
                               uint64_t len);

uint64_t pb_buffer_overwrite_data(
                              struct pb_buffer * const buffer,
                              const uint8_t *buf,
                              uint64_t len);


uint64_t pb_buffer_read_data(struct pb_buffer * const buffer,
                             uint8_t * const buf,
                             uint64_t len);


void pb_buffer_clear(struct pb_buffer * const buffer);
void pb_buffer_destroy(
                     struct pb_buffer * const buffer);



/** The trivial buffer implementation and its supporting functions.
 *
 * The trivial buffer defines a classic strategy and operations set, and uses
 * the trivial heap based allocator by default.
 */
struct pb_trivial_buffer {
  struct pb_buffer buffer;

  struct pb_page page_end;

  uint64_t data_revision;
  uint64_t data_size;
};



/** Get a trivial buffer strategy.
 *
 * page_size is 4096
 *
 * clone_on_write is false
 *
 * fragment_as_source is false
 *
 * rejects_insert is false
 */
const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void);



/** Get a trivial buffer operations structure. */
const struct pb_buffer_operations *pb_get_trivial_buffer_operations(void);



/** An implementation of pb_buffer using the default or supplied allocator. */
struct pb_buffer *pb_trivial_buffer_create(void);
struct pb_buffer *pb_trivial_buffer_create_with_strategy(
                                    const struct pb_buffer_strategy *strategy);
struct pb_buffer *pb_trivial_buffer_create_with_alloc(
                                    const struct pb_allocator *allocator);
struct pb_buffer *pb_trivial_buffer_create_with_strategy_with_alloc(
                                    const struct pb_buffer_strategy *strategy,
                                    const struct pb_allocator *allocator);



/** Trivial buffer operations. */
uint64_t pb_trivial_buffer_get_data_revision(
                                         struct pb_buffer * const buffer);
void pb_trivial_buffer_increment_data_revision(
                                         struct pb_buffer * const buffer);


uint64_t pb_trivial_buffer_get_data_size(struct pb_buffer * const buffer);

void pb_trivial_buffer_increment_data_size(
                                         struct pb_buffer * const buffer,
                                         uint64_t size);
void pb_trivial_buffer_decrement_data_size(
                                         struct pb_buffer * const buffer,
                                         uint64_t size);


void pb_trivial_buffer_get_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_trivial_buffer_get_iterator_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_trivial_buffer_iterator_is_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_trivial_buffer_iterator_cmp(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *lvalue,
                            const struct pb_buffer_iterator *rvalue);
void pb_trivial_buffer_iterator_next(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_trivial_buffer_iterator_prev(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);


void pb_trivial_buffer_get_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_trivial_buffer_get_byte_iterator_end(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_trivial_buffer_byte_iterator_is_end(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_trivial_buffer_byte_iterator_cmp(struct pb_buffer * const buffer,
                         const struct pb_buffer_byte_iterator *lvalue,
                         const struct pb_buffer_byte_iterator *rvalue);
void pb_trivial_buffer_byte_iterator_next(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_trivial_buffer_byte_iterator_prev(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);


struct pb_page *pb_trivial_buffer_page_create(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t len,
                              bool is_rewind);
struct pb_page *pb_trivial_buffer_page_create_ref(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              const uint8_t *buf, size_t len,
                              bool is_rewind);


uint64_t pb_trivial_buffer_insert(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              struct pb_page * const page);
uint64_t pb_trivial_buffer_extend(
                              struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_rewind(
                              struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_seek(struct pb_buffer * const buffer,
                              uint64_t len);
uint64_t pb_trivial_buffer_trim(struct pb_buffer * const buffer,
                              uint64_t len);


uint64_t pb_trivial_buffer_write_data(struct pb_buffer * const buffer,
                                      const uint8_t *buf,
                                      uint64_t len);
uint64_t pb_trivial_buffer_write_data_ref(
                                      struct pb_buffer * const buffer,
                                      const uint8_t *buf,
                                      uint64_t len);
uint64_t pb_trivial_buffer_write_buffer(
                                      struct pb_buffer * const buffer,
                                      struct pb_buffer * const src_buffer,
                                      uint64_t len);


uint64_t pb_trivial_buffer_insert_data(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_trivial_buffer_insert_data_ref(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_trivial_buffer_insert_buffer(
                              struct pb_buffer * const buffer,
                              struct pb_buffer_iterator * const buffer_iterator,
                              size_t offset,
                              struct pb_buffer * const src_buffer,
                              uint64_t len);


uint64_t pb_trivial_buffer_overwrite_data(struct pb_buffer * const buffer,
                                          const uint8_t *buf,
                                          uint64_t len);


uint64_t pb_trivial_buffer_read_data(struct pb_buffer * const buffer,
                                     uint8_t * const buf,
                                     uint64_t len);


void pb_trivial_buffer_clear(struct pb_buffer * const buffer);
void pb_trivial_pure_buffer_clear(
                             struct pb_buffer * const buffer);
void pb_trivial_buffer_destroy(
                             struct pb_buffer * const buffer);






/** The pb_data_reader and its supporting functions. */
struct pb_data_reader;



/**
 *
 * The operations structure of the data reader.
 */
struct pb_data_reader_operations {
  /** Read data from the pb_buffer instance, into a memory region.
   *
   * buf indicates the start of the target memory region.
   * len indicates the amount of data to read, in bytes.
   *
   * returns the amount of data read.
   *
   * Data is read from the head of the source pb_buffer.  The amount of data read
   * is the lower of the size of the source pb_buffer and the value of len.
   *
   * Following a data read, the data reader will retain the position of the
   * end of the read, thus, a subsequent call to read_buffer will continue from
   * where the last read left off.
   *
   * However, if the pb_buffer undergoes an operation that alters its data
   * revision, a subsequent call to read_buffer will read from the beginning
   */
  uint64_t (*read)(struct pb_data_reader * const data_reader,
                   uint8_t * const buf, uint64_t len);

  /** Reset the data reader back to the beginning of the pb_buffer instance. */
  void (*reset)(struct pb_data_reader * const data_reader);

  /** Destroy the pb_data_reader instance. */
  void (*destroy)(struct pb_data_reader * const data_reader);
};



/** An interface for reading data from a pb_buffer. */
struct pb_data_reader {
  const struct pb_data_reader_operations *operations;

  struct pb_buffer *buffer;
};



/** Functional interfaces for the generic pb_data_reader class. */
uint64_t pb_data_reader_read(struct pb_data_reader * const data_reader,
                             uint8_t * const buf, uint64_t len);

void pb_data_reader_reset(struct pb_data_reader * const data_reader);
void pb_data_reader_destroy(
                          struct pb_data_reader * const data_reader);



/** The trivial data reader implementation and its supporting functions. */



/** Get a trivial data reader operations structure. */
const struct pb_data_reader_operations
  *pb_get_trivial_data_reader_operations(void);



/** A trivial data reader implementation that reads data via iterators
 */
struct pb_data_reader *pb_trivial_data_reader_create(
                                              struct pb_buffer * const buffer);

uint64_t pb_trivial_data_reader_read(struct pb_data_reader * const data_reader,
                                     uint8_t * const buf, uint64_t len);

void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader);
void pb_trivial_data_reader_destroy(
                                  struct pb_data_reader * const data_reader);






/** The pb_line_reader and its supporting functions. */
struct pb_line_reader;



/** An interface for discovering, reading and consuming LF or CRLF terminated
 *  lines in pb_buffer instances.
 *
 * The operations structure of the line reader.
 */
struct pb_line_reader_operations {
  /** Indicates whether a line exists at the head of a pb_buffer instance. */
  bool (*has_line)(struct pb_line_reader * const line_reader);

  /** Returns the length of the line discovered by has_line. */
  size_t (*get_line_len)(struct pb_line_reader * const line_reader);
  /** Extracts the data of the line discovered by has_line.
   *
   * base indicates the start of the destination memory region
   * len indicates the shorter of the number of bytes to read or the length of
   *     the memory region.
   */
  size_t (*get_line_data)(struct pb_line_reader * const line_reader,
                          uint8_t * const base, uint64_t len);

  size_t (*seek_line)(struct pb_line_reader * const line_reader);

  /** Marks the present position of line discovery as a line end.
   *
   * If the character proceeding the termination point is a cr, it will be
   * ignored in the line length calculation and included in the line. */
  void (*terminate_line)(struct pb_line_reader * const line_reader);
  /** Marks the present position of line discovery as a line end.
   *
   * If the character proceeding the termination point is a cr, it will be
   * considered in the line length calculation. */
  void (*terminate_line_check_cr)(struct pb_line_reader * const line_reader);

  /** Indicates whether the line discovered in has_line is:
   *    LF (false) or CRLF (true) */
  bool (*is_crlf)(struct pb_line_reader * const line_reader);
  /** Indicates whether the line discovery has reached the end of the buffer.
   *
   * Buffer end may be used as line end if terminate_line is called. */
  bool (*is_end)(struct pb_line_reader * const line_reader);

  /** Clone the state of the line reader into a new instance. */
  struct pb_line_reader *(*clone)(struct pb_line_reader * const line_reader);

  /** Reset the current line discovery back to an initial state. */
  void (*reset)(struct pb_line_reader * const line_reader);

  /** Destroy a line reader instance. */
  void (*destroy)(struct pb_line_reader * const line_reader);
};



/** Lines in a buffer will be limited by implementations to no longer than
 * PB_TRIVIAL_LINE_READER_DEFAULT_LINE_MAX bytes.  Any lines that are longer
 * will be truncated to that length.
 */
#define PB_LINE_READER_DEFAULT_LINE_MAX                   16777216L

struct pb_line_reader {
  const struct pb_line_reader_operations *operations;

  struct pb_buffer *buffer;
};




/** Functional infterface for the generic pb_buffer class. */
bool pb_line_reader_has_line(struct pb_line_reader * const line_reader);

size_t pb_line_reader_get_line_len(struct pb_line_reader * const line_reader);
size_t pb_line_reader_get_line_data(
                                   struct pb_line_reader * const line_reader,
                                   uint8_t * const buf, uint64_t len);

bool pb_line_reader_is_crlf(
                           struct pb_line_reader * const line_reader);
bool pb_line_reader_is_end(struct pb_line_reader * const line_reader);

void pb_line_reader_terminate_line(struct pb_line_reader * const line_reader);
void pb_line_reader_terminate_line_check_cr(
                                   struct pb_line_reader * const line_reader);

size_t pb_line_reader_seek_line(struct pb_line_reader * const line_reader);

struct pb_line_reader *pb_line_reader_clone(
                                     struct pb_line_reader * const line_reader);

void pb_line_reader_reset(struct pb_line_reader * const line_reader);
void pb_line_reader_destroy(
                          struct pb_line_reader * const line_reader);



/** The trivial line reader implementation and its supporting functions. */



/** Get a trivial line reader operations structure. */
const struct pb_line_reader_operations
  *pb_get_trivial_line_reader_operations(void);



/** A trivial line reader implementation that searches for lines via iterators.
 */
struct pb_line_reader *pb_trivial_line_reader_create(
                                          struct pb_buffer * const buffer);

bool pb_trivial_line_reader_has_line(
                                    struct pb_line_reader * const line_reader);

size_t pb_trivial_line_reader_get_line_len(
                                    struct pb_line_reader * const line_reader);
size_t pb_trivial_line_reader_get_line_data(
                                    struct pb_line_reader * const line_reader,
                                    uint8_t * const buf, uint64_t len);

size_t pb_trivial_line_reader_seek_line(
                                    struct pb_line_reader * const line_reader);

bool pb_trivial_line_reader_is_crlf(struct pb_line_reader * const line_reader);
bool pb_trivial_line_reader_is_end(struct pb_line_reader * const line_reader);

void pb_trivial_line_reader_terminate_line(
                                    struct pb_line_reader * const line_reader);
void pb_trivial_line_reader_terminate_line_check_cr(
                                    struct pb_line_reader * const line_reader);

struct pb_line_reader *pb_trivial_line_reader_clone(
                                    struct pb_line_reader * const line_reader);

void pb_trivial_line_reader_reset(struct pb_line_reader * const line_reader);
void pb_trivial_line_reader_destroy(
                                  struct pb_line_reader * const line_reader);



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAGEBUF_H */
