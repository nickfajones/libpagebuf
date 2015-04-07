/*******************************************************************************
 *  Copyright 2013-2015 Nick Jones <nick.fa.jones@gmail.com>
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
  void *(*alloc)(enum pb_allocator_type type, size_t size,
                 const struct pb_allocator *allocator);
  /** Free a memory region.
   *
   * type indicates whether the memory allocated was used for a data
   * structure, or as a memory region.
   *
   * obj is the address of beginning of the memory region.
   *
   * size indicates the size of the memory region that was allocated.
   */
  void  (*free)(enum pb_allocator_type type, void *obj, size_t size,
                const struct pb_allocator *allocator);
};

/** Get a built in, trivial heap based allocator. */
const struct pb_allocator *pb_get_trivial_allocator(void);



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

  /** Use count, maintained with atomic operations. */
  uint16_t use_count;

  /** Responsibility that this structure has over the memory region. */
  enum pb_data_responsibility responsibility;

  /** Allocator used to allocate storage for this struct and its owned
   * memory region */
  const struct pb_allocator *allocator;
};



/** Create a pb_data instance.
 *
 * Memory region buf is now owned by the pb_data instance and will be freed
 * when the instance is destroyed.
 *
 * Use count is initialised to one, therefore returned data instance must
 * be treated with the pb_data_put function when the creator is finished with
 * it, whether it is passed to another owner or not.
 */
struct pb_data *pb_data_create(uint8_t * const buf, size_t len,
                               const struct pb_allocator *allocator);

/** Create a pb_data instance.
 *
 * Memory region buf is not owned by the pb_data instance.
 *
 * Use count is initialised to one, therefore returned data instance must
 * be treated with the pb_data_put function when the creator is finished with
 * it, whether it is passed to another owner or not.
 */
struct pb_data *pb_data_create_ref(const uint8_t *buf, size_t len,
                                   const struct pb_allocator * allocator);

/** Clone a pb_data instance.
 *
 * A new memory region is allocated and the source memory region is copied
 * byte by byte.
 */
struct pb_data *pb_data_clone(uint8_t * const buf, size_t len, size_t src_off,
                              const struct pb_data *src_data,
                              const struct pb_allocator *allocator);

/** Destroy a pb_data instance.
 *
 * Not to be called directly unless the instance was never 'get'd by a
 * container.
 *
 * The memory region will be freed if it is owned by the instance.
 * The pb_data instance will be freed.
 */
void pb_data_destroy(struct pb_data *data);

/** Increment the use count of the pb_data instance.
 */
void pb_data_get(struct pb_data *data);

/** Decrement the use count of the pb_data instance.
 *
 *  Will destroy the instance if the use count becomes zero.
 */
void pb_data_put(struct pb_data *data);



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

#if 0
/** Clone an existing pb_page instance
 *
 * Duplicate the base and len values as well as a full clone of the referenced
 * pb_data instance.
 */
struct pb_page *pb_page_clone(const struct pb_page *src_page,
                              uint8_t * const buf, size_t len, size_t src_off,
                              const struct pb_allocator *allocator);
#endif

/** Destroy the pb_page instance.
 *
 * pb_data instance will be released.
 * pb_page instance will be cleared and freed.
 */
void pb_page_destroy(struct pb_page *page,
                     const struct pb_allocator *allocator);



/** A structure used to iterate over buffer data.
 */
struct pb_buffer_iterator {
  struct pb_page *page;
};



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
   * not_cloned (false): reference to the pb_data instance is incremented.
   *
   * cloned (true): new pb_data instance created and memory regions copied.
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
   * as target (true):  target pb_buffer page_size takes precedence.
   *                    When clone_on_write is false, source fragments that
   *                    are greater than the target page_size limit are split.
   *                    When clone_on_write is true, source fragments will be
   *                    packed into target fragments up to the target page_size
   *                    in size.
   */
  bool fragment_as_target;
};



/** The page size of the default pb_buffer strategy.
 */
#define PB_TRIVIAL_LIST_DEFAULT_PAGE_SIZE                 4096



/** Get a built in, trivial buffer strategy.
 *
 * page_size is 4096
 *
 * clone_on_write is false
 *
 * fragment_as_source is false
 *
 * no_insertion is false;
 */
const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void);



/** Represents a buffer of pages, and operations for the manipulation of the
 * pages and the buffers of data therein.
 *
 * The buffer also represents the strategy for allocation and freeing of
 * data buffers.
 */
struct pb_buffer {
  /** The strategy used by the pb_buffer instance. */
  struct pb_buffer_strategy strategy;

  /** Return the amount of data in the buffer, in bytes. */
  uint64_t (*get_data_size)(struct pb_buffer * const buffer);

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

  /** Append a pb_page instance to the pb_buffer.
   *
   * buffer_iterator is the position in the buffer to update the pb_buffer.  The
   *               pb_page will be inserted in front of the iterator position.
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
  uint64_t (*insert)(struct pb_buffer * const buffer,
                     struct pb_page * const page,
                     struct pb_buffer_iterator * const buffer_iterator,
                     size_t offset);

  /** Increase the size of the buffer by adding len bytes of data to the end.
   *
   * len indicates the amount of data to add in bytes.
   *
   * Depending on the buffer implementation details, the total len may be split
   * across multiple pages.
   */
  uint64_t (*reserve)(struct pb_buffer * const buffer,
                      uint64_t len);
  /** Increases the size of the buffer by adding len bytes of data to the head.
   *
   * len indicates the amount of data to ad in bytes.
   *
   * Depending on the buffer implementation details, the total len may be split
   * across multiple pages.
   */
  uint64_t (*rewind)(struct pb_buffer * const buffer,
                     uint64_t len);
  /** Seek the buffer by len bytes.
     *
     * len indicates the amount of data to seek, in bytes.
     *
     * The seek operation may cause internal pages to be consumed depending on
     * the buffer implementation details.  These pb_pages will be destroyed during
     * the seek operation and their respective data 'put'd.
     */
  uint64_t (*seek)(struct pb_buffer * const buffer,
                   uint64_t len);

  /** Initialise an iterator to the start of the pb_buffer instance data.
   *
   * The iterator should be a pointer to an object on the stack that should
   * be manipulated only by the iterator methods of the same buffer instance.
   */
  void (*get_iterator)(struct pb_buffer * const buffer,
                       struct pb_buffer_iterator * const buffer_iterator);
  /** Initialise an iterator to the end of the pb_buffer instance data.
     *
     * The iterator should be a pointer to an object on the stack that should
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
  bool (*is_iterator_end)(struct pb_buffer * const buffer,
                          struct pb_buffer_iterator * const buffer_iterator);

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
  /** Write data from a source pb_buffer instance to the pb_buffer instance.
   *
   * src_buffer is the buffer to write from.  This pb_buffer instance will not have
   *          it's data modified, but it is not const because iterator
   *          operations of this pb_buffer may cause internal state changes.
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

  const struct pb_allocator *allocator;
};

/** A trivial buffer implementation using builtin or supplied allocator.
 *
 * Default page size: 4096 bytes
 */
struct pb_buffer *pb_trivial_buffer_create(void);
struct pb_buffer *pb_trivial_buffer_create_with_strategy(
                                     const struct pb_buffer_strategy *strategy);
struct pb_buffer *pb_trivial_buffer_create_with_alloc(
                                     const struct pb_allocator *allocator);
struct pb_buffer *pb_trivial_buffer_create_with_strategy_with_alloc(
                                     const struct pb_buffer_strategy *strategy,
                                     const struct pb_allocator *allocator);

uint64_t pb_trivial_buffer_get_data_size(struct pb_buffer * const buffer);

uint64_t pb_trivial_buffer_get_data_revision(struct pb_buffer * const buffer);

uint64_t pb_trivial_buffer_insert(struct pb_buffer * const buffer,
                                  struct pb_page * const page,
                                  struct pb_buffer_iterator * const iterator,
                                  size_t offset);

uint64_t pb_trivial_buffer_append(struct pb_buffer * const buffer,
                                  struct pb_page * const page);
uint64_t pb_trivial_buffer_reserve(struct pb_buffer * const buffer,
                                   uint64_t len);
uint64_t pb_trivial_buffer_seek(struct pb_buffer * const buffer,
                                uint64_t len);
uint64_t pb_trivial_buffer_rewind(struct pb_buffer * const buffer,
                                  uint64_t len);

void pb_trivial_buffer_get_iterator(struct pb_buffer * const buffer,
                                    struct pb_buffer_iterator * const iterator);
void pb_trivial_buffer_get_iterator_end(
                                    struct pb_buffer * const buffer,
                                    struct pb_buffer_iterator * const iterator);
bool pb_trivial_buffer_is_iterator_end(struct pb_buffer * const buffer,
                                    struct pb_buffer_iterator * const iterator);

void pb_trivial_buffer_iterator_next(
                                    struct pb_buffer * const buffer,
                                    struct pb_buffer_iterator * const iterator);
void pb_trivial_buffer_iterator_prev(
                                    struct pb_buffer * const buffer,
                                    struct pb_buffer_iterator * const iterator);

uint64_t pb_trivial_buffer_write_data(struct pb_buffer * const buffer,
                                      const uint8_t *buf,
                                      uint64_t len);
uint64_t pb_trivial_buffer_write_buffer(struct pb_buffer * const buffer,
                                        struct pb_buffer * const src_buffer,
                                        uint64_t len);
uint64_t pb_trivial_buffer_overwrite_data(struct pb_buffer * const buffer,
                                          const uint8_t *buf,
                                          uint64_t len);

uint64_t pb_trivial_buffer_read_data(struct pb_buffer * const buffer,
                                     uint8_t * const buf,
                                     uint64_t len);

void pb_trivial_buffer_clear(struct pb_buffer * const buffer);
void pb_trivial_buffer_destroy(struct pb_buffer * const buffer);



/** An interface for reading data from a pb_buffer.
 *
 * Through this interface, a
 */
struct pb_data_reader {
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

  struct pb_buffer *buffer;

  const struct pb_allocator *allocator;
};

/** A trivial data reader implementation that reads data via iterators
 */
struct pb_data_reader *pb_trivial_data_reader_create(
                                          struct pb_buffer * const buffer);
struct pb_data_reader *pb_trivial_data_reader_create_with_alloc(
                                          struct pb_buffer * const buffer,
                                          const struct pb_allocator *allocator);

uint64_t pb_trivial_data_reader_read(struct pb_data_reader * const data_reader,
                                     uint8_t * const buf, uint64_t len);

void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader);

void pb_trivial_data_reader_destroy(struct pb_data_reader * const data_reader);



/** An interface for discovering, reading and consuming LF or CRLF terminated
 *  lines in pb_buffer instances.
 */
struct pb_line_reader {
  /** Indicates whether a line exists at the head of a pb_buffer instance. */
  bool (*has_line)(struct pb_line_reader * const line_reader);

  /** Returns the length of the line discovered by has_line. */
  uint64_t (*get_line_len)(struct pb_line_reader * const line_reader);
  /** Extracts the data of the line discovered by has_line.
   *
   * buf indicates the start of the destination memory region
   * len indicates the shorter of the number of bytes to read or the length of
   *     the memory region.
   */
  uint64_t (*get_line_data)(struct pb_line_reader * const line_reader,
                            uint8_t * const base, uint64_t len);

  uint64_t (*seek_line)(struct pb_line_reader * const line_reader);

  /** Marks the present position of line discovery as a line end.
   *
   * If the character proceeding the termination point is a cr, it will be
   * ignored in the line length calculation and included in the line. */
  void (*terminate_line)(struct pb_line_reader * const line_reader);
  /** Marks the present position of line discovery as a line end.
   *
   * If the character proceeding the termination point is a cr, it will be
   * considered in the line length calculation. */
  void (*terminate_line_and_cr)(struct pb_line_reader * const line_reader);

  /** Indicates whether the line discovered in has_line is:
   *    LF (false) or CRLF (true) */
  bool (*is_crlf)(struct pb_line_reader * const line_reader);
  /** Indicates whether the line discovery has reached the end of the buffer.
   *
   * Buffer end may be used as line end if terminate_line is called. */
  bool (*is_end)(struct pb_line_reader * const line_reader);

  /** Reset the current line discovery back to an initial state. */
  void (*reset)(struct pb_line_reader * const line_reader);

  /** Destroy a line reader instance. */
  void (*destroy)(struct pb_line_reader * const line_reader);

  struct pb_buffer *buffer;

  const struct pb_allocator *allocator;
};

/** A trivial line reader implementation that searches for lines via iterators.
 */
struct pb_line_reader *pb_trivial_line_reader_create(
                                          struct pb_buffer *buffer);
struct pb_line_reader *pb_trivial_line_reader_create_with_alloc(
                                          struct pb_buffer *buffer,
                                          const struct pb_allocator *allocator);

bool pb_trivial_line_reader_has_line(struct pb_line_reader * const line_reader);

uint64_t pb_trivial_line_reader_get_line_len(
                                     struct pb_line_reader * const line_reader);
uint64_t pb_trivial_line_reader_get_line_data(
                                     struct pb_line_reader * const line_reader,
                                     uint8_t * const buf, uint64_t len);

uint64_t pb_trivial_line_reader_seek_line(
                                     struct pb_line_reader * const line_reader);

bool pb_trivial_line_reader_is_crlf(struct pb_line_reader * const line_reader);
bool pb_trivial_line_reader_is_end(struct pb_line_reader * const line_reader);

void pb_trivial_line_reader_terminate_line(
                                     struct pb_line_reader * const line_reader);

void pb_trivial_line_reader_reset(struct pb_line_reader * const line_reader);
void pb_trivial_line_reader_destroy(struct pb_line_reader * const line_reader);



#ifdef __cplusplus
}; // extern "C"
#endif

#endif /* PAGEBUF_H */
