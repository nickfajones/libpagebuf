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


/** libpagebuf
 *
 * Developers of software that involves IO, for example networking IO, face
 * the challenge of dealing with large amounts of data.  Whether the data is
 * passing quickly through the system or not, this data will at the least need
 * to be stored after it is received from the input side, then that same data
 * may need to be arranged for writing back to the output side.
 * Additionally, the data may require processing as it moves in then out of
 * the system, processing such as parsing and even modification and
 * manipulation.
 *
 * When that software system is non-blocking and event driven, additional
 * challenges exist, such as the need to capture data that arrives in a
 * piecewise manner, in uncertain size and delay patterns, then the need to
 * gain access to that data in a linear form to allow proper parsing and
 * manipulation.
 *
 * And all of this needs to be done as resource and time efficiently as
 * possible.
 *
 * libpagebuf is designed to provide a solution to the data storage challenges
 * faced in particular by developers of IO oriented, non-blocking event driven
 * systems.  At its core, it implements a set of data structures and
 * algorithms for the management of regions of system memory.  On the surface,
 * through the use of the central pb_buffer class, it provides a means of
 * reading and manipulating the data that is stored in those memory regions,
 * that is abstracted away from the underlying arrangement of those memory
 * regions in the system memory space.
 *
 * An author may use pb_buffer to receive data from input sources as
 * fragments, then perform read actions such as searching and copying in
 * addition to some more intrusive actions such as insertion or truncation on
 * that data, without any regard for the underlying fragmentation of the
 * memory regions that the data is stored in.
 *
 * libpagebuf is also designed with concerns of non-blocking event driven
 * and multithreaded systems in mind, through the inclusion of interfaces for
 * the use of custom memory allocators, debugging to check internal structure
 * integrity and thread exclusivity, interfaces in both C and C++,
 * C++ interfaces complying to the ASIO ConstBufferSequence,
 * MutableBufferSequence and DynamicBufferSequence concepts, as well as the
 * BidirectionanIterator concept used in particular by boost::regex.
 *
 * libpagebuf is designed for efficiency, using reference counting and
 * zero-copy semantics at its core, as well as providing a class like
 * interface in C that provides a path for subclassing and modifying
 * implementation details.
 */

/** libpagebuf classes
 *
 * Although it is written in C, libpagebuf implements several of its most
 * important data structures in an object like fashion.
 *
 * Each of these objects possesses a '_operations' structure, which is a group
 * of function pointers implementing key internal functions on an instance of
 * said object.  Concrete implementations of important objects will define
 * a const pointer to a _operations structure, populated with functions that
 * are either pre-existing, or borrowed from a 'base' class that implement
 * the same functionality as the base class, or with functions that are
 * unique to the relevant implementation, and override the default behaviour
 * of the base class.  Of course an _operations structure may contain a
 * combination of both types of functions.  These _operations structures are
 * akin to vtables in C++ objects.
 *
 * Concrete implementations will also define 'factory' functions, which are
 * akin to specific constructors of buffers of the relevant implementation.
 * These factory functions may accept specific arguments to configure the
 * specific behaviour and functionality of the implementations but one thing
 * must remain consistent amongst all factory functions, and that is they
 * must return a pointer to the lowest base type of the particular object
 * tree, so that those objects may be treated polymorphicly, either by the
 * user directly, or by other objects internally.  For example, any derivative
 * implementations of a pb_allocator must be created by a factory method,
 * that may take arguments specific to the particular implementation, however
 * that function must return a pointer to a pb_allocator, which can then
 * be called polymorphically by other objects such as pb_data and pb_buffer
 * without any regard to the underlying implementation (other than the
 * expectation that it works properly)
 *
 * Concreate implementations do not need to to make their _operations
 * structures publicly available, the binding of an object to its _operations
 * can be done opaquely inside the factory function.
 *
 * In order to subclass an object, use the typical C object embedding method
 * where an instance of the base class object is the first member of the
 * subclass object.  The factory function will return the address of the
 * embedded base instance which will have the same address as the subclass.
 * Similarly, when a subclass specific over-ridden function receives the
 * pointer to the base class object as a parameter, it can then 'upcast' the
 * base back up to the subclass.
 *
 * libpagebuf allows subclassing of the following classes:
 * pb_allocator,
 * pb_data,
 * pb_buffer
 *
 * libpagebuf provides a base implementation of allocator, data and buffer
 * functionality named 'trivial', which uses a straight forward list based
 * arrangement of pages, backed by heap sourced memory regions.
 *
 * In addition, libpagebuf provides a mmap buffer, which uses arranges data in
 * a sparse list, backed by memory regions mapping to a file on disk.
 */






/* Pre-declare operations. */
struct pb_allocator_operations;



/** Indicates the intended use of an allocated memory block. */
enum pb_allocator_type {
  pb_alloc_type_struct,
  pb_alloc_type_region,
};



/** Wrapper for allocation and freeing of blocks of memory
 *
 * Allocate and free memory blocks through these interfaces.  The allocated
 * blocks will be used to hold data structures (pb_alloc_type_struct) or
 * for use as memory regions (pb_alloc_type_region), as indicated by the
 * caller.
 *
 * Memory to be used for structs must be zero'd after allocation and before
 * returning to the caller, and zero'd again after being handed back to the
 * allocator and before being freed.
 *
 * Memory to be used as a data region, storage areas for user data, need not be
 * treated in any special way by an allocator.
 */
struct pb_allocator {
  const struct pb_allocator_operations *operations;
};



/** The structure that holds the operations that implement pb_allocator
 *  functionality.
 */
struct pb_allocator_operations {
  /** Allocate a memory block.
   *
   * type indicates what the allocated memory block will be used for
   *
   * size is the size of the memory block to allocate.
   */
  void *(*alloc)(const struct pb_allocator *allocator,
                 enum pb_allocator_type type, size_t size);
  /** Free a memory block.
   *
   * type indicates how the memory block was used.
   *
   * obj is the address of beginning of the memory region.
   *
   * size indicates the size of the memory region that was allocated and now
   *      freed.
   */
  void  (*free)(const struct pb_allocator *allocator,
                enum pb_allocator_type type, void *obj, size_t size);
};



/** Functional interfaces for the pb_allocator class.
 *
 * These interfaces should be used to invoke the allocator operations.
 *
 * However, given that allocators are protected members of buffers, these
 * operations are not intended to be called by end users.
 */
void *pb_allocator_alloc(
                       const struct pb_allocator *allocator,
                       enum pb_allocator_type type, size_t size);
void pb_allocator_free(const struct pb_allocator *allocator,
                       enum pb_allocator_type type, void *obj, size_t size);



/** Get a built in, trivial set of pb_allocator operations.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_allocator_operations *pb_get_trivial_allocator_operations(void);



/** Get a built in, trivial heap based allocator.
 *
 * This function is public and available to end users.
 */
const struct pb_allocator *pb_get_trivial_allocator(void);






/** A structure used to describe a data region. */
struct pb_data_vec {
  /** The starting address of the region. */
  uint8_t *base;
  /** The length of the region. */
  size_t len;
};






/* Pre-declare operations. */
struct pb_data_operations;



/** Indicates the responsibility of a pb_data instance has over its data region.
 *
 * Either owned exclusively by the pb_data instance (pb_data_owned), thus freed
 * when pb_data use count reaches zero, or merely referenced by the pb_data
 * instance (pb_data_referenced), whereby the data region is not freed when the
 * pb_data use_count reaches zero.
 */
enum pb_data_responsibility {
  pb_data_owned,
  pb_data_referenced,
};



/** Reference counted structure that represents a memory region.
 *
 * Each pb_data instance has a one-to-one relationship to its memory region,
 * whether that region is owned or referenced, and the description of the
 * bounds of the data region, the data_vec member, is immutable and will not
 * change during the lifetime of the pb_data instance.
 *
 * Where a pb_data instance owns its memory region, the instance and the
 * region should be created at the same time, with the same allocator,
 * ideally in a factory function (such as pb_trivial_data_create)
 * Where a pb_data instance merely referenced a pre-allocated memory region,
 * an association between the two should be created as soon as possible so
 * that the lifecycle and relevance of the memory region can be tracked by
 * the pb_data instance.
 *
 * Instances of pb_data must be created using the create routines below, but
 * they should never be explicitly destroyed.  Instead the get and put
 * functions should be used to maintain the use_count of pb_data instances:
 * pb_data_get is to be called by new owners of a pb_data_instance,
 * pb_data_put is to be called when an owner no longer needs a pb_data
 * instance.  When a call to pb_data_put finds a zero use_count it will
 * destroy.
 *
 * Seeing as the pb_data class has such a close relationship with the
 * memory region it references, pb_buffer subclasses that enact particular
 * internal behaviour will find it neccessary to also subclass pb_data.
 * pb_data is indeed fit for this purpose.
 */
struct pb_data {
  /** The bounds of the region: base memory address and size (length). Cannot
   *  be changed after creation.
   */
  struct pb_data_vec data_vec;

  /** Responsibility that the instance has over its memory region. */
  enum pb_data_responsibility responsibility;

  /** Use count, maintained with atomic operations. */
  uint16_t use_count;

  /** Operations for the pb_buffer instance.  Cannot be changed after creation.
   */
  const struct pb_data_operations *operations;

  /** The allocator used to allocate memory blocks for this struct and its
   *  owned memory region, in addition to freeing these same blocks.
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
   * The memory region of the instance will be freed if it is owned by the
   * instance (see pb_data_responsibility below).
   * The memory block associated with the pb_data instance itself will be
   * freed.
   */
  void (*put)(struct pb_data * const data);
};



/** Functional interfaces for the pb_data class.
 *
 * These interfaces should be used to invoke the data operations.
 *
 * However, given that data intances are protected members of buffers, these
 * operations are not intended to be called by end users.
 */
void pb_data_get(struct pb_data * const data);
void pb_data_put(struct pb_data * const data);



/** Get a built in, trivial set of pb_data operations.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_data_operations *pb_get_trivial_data_operations(void);



/** Factory functions for a trivial implementation of pb_data using trivial
 *  operations.
 *
 * These are protected functions and should not be called externally.
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






/** Non-exclusive owner of a pb_data instance, maintaining a modifiable
 *  reference to the pb_data memory region.
 *
 * This class embodies the consumption of the memory region referenced by the
 * member pb_data instance.  A pb_page instance holds a use_count reference
 * against its pb_data instance member, thus influencing its lifecycle.
 * The data_vec of the pb_page is initialised as a (non-proper) subset of the
 * data_vec of the pb_data, depending what the context of the pb_page
 * initialisation and data usage patterns in the running application.
 * The pb_page data_vec will always reside inside the pb_data data vec, in
 * terms of both base position and length (from the base position).  Any
 * deviation from this is an error.
 *
 * There is a many-to-one relationship between pb_page instances and pb_data
 * instances in some types of pb_buffer.  Each pb_page holds a use_count
 * reference on the same pb_data, collectively influencing its lifecyle.
 *
 * There is a one-to-one relationship between pb_buffer instances and
 * pb_page instances.  pb_pages instances will never travel outside a
 * pb_buffer and the pb_buffer will maintain full control of the lifecycle of
 * its own pages, by means of the allocator associated with the pb_buffer.
 * pb_page therefore does not required reference counting, nor is it
 * required to retain its allocator.
 * pb_data refererenced by a pb_buffer through a pb_page may be passed from
 * one pb_buffer to another, depending on the behaviour built into the target
 * pb_buffer.
 *
 * The pb_page class is thoroughly lightweight and internal to pb_buffer so
 * it should never be subclassed to provide buffer class specific behaviour.
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
 * This is a protected function and should not be called externally.
 *
 * The pb_data instance has its use count incremented.
 */
struct pb_page *pb_page_create(struct pb_data *data,
                               const struct pb_allocator *allocator);

/** Create a pb_page using properties of another.
 *
 * This is a protected function and should not be called externally.
 *
 * The new pb_page instance will duplicate the base and len values of the
 * source page, and reference pb_data instance of the source page.
 */
struct pb_page *pb_page_transfer(const struct pb_page *src_page,
                                 size_t len, size_t src_off,
                                 const struct pb_allocator *allocator);

/** Destroy a pb_page instance.
 *
 * This is a protected function and should not be called externally.
 *
 * The pb_data instance will be de-referenced once.
 */
void pb_page_destroy(struct pb_page *page,
                     const struct pb_allocator *allocator);






/* Pre-declare strategy and operations. */
struct pb_buffer_strategy;
struct pb_buffer_operations;



/** The base pb_buffer class.
 *
 * The pb_buffer class is the focus of libpagebuf.  It represents the core
 * functionality of the library and is the primary interface for accessing
 * this functionality.
 *
 * The base pb_buffer only references high level structures that are
 * fundamental to the identity and operation a buffer.
 */
struct pb_buffer {
  /** The description of the core behaviour of the buffer.  May or may not be
   *  variable between buffer instances, depending on the specific buffer
   *  class, however in all cases, these behaviours are not expected nor should
   *  be permitted to change after a buffer instance is created.
   */
  const struct pb_buffer_strategy *strategy;

  /** The structure describing the concrete implementation of the buffer
   *  functions.  Immutable and identical for all instances of same buffer
   *  classes.
   */
  const struct pb_buffer_operations *operations;

  /** The allocator used by the buffer instance to perform all structure and
   *  memory region allocations.
   */
  const struct pb_allocator *allocator;
};




/** A structure used to iterate over memory regions in a pb_buffer,
 *  one page at a time.
 *
 * The iterator may point to either pages managed internally by the pb_buffer
 * instance, that represent memory regions and thus data inside the buffer,
 * or it may point to a special 'end' page, that indicates the end pf buffer
 * data has been reached following iteration, or it may point to the special
 * 'end' page after initialisation, indicating there is no data in the buffer.
 */
struct pb_buffer_iterator {
  struct pb_page *page;
};



/** Functional interfaces for accessing a memory region through the
 *  pb_buffer_iterator class.
 *
 * These functions are public and may be called by end users.
 */
uint8_t *pb_buffer_iterator_get_base(
                             const struct pb_buffer_iterator *buffer_iterator);
size_t pb_buffer_iterator_get_len(
                             const struct pb_buffer_iterator *buffer_iterator);



/** A structure used to iterate over memory regions in a pb_buffer,
 *  one byte at a time. */
struct pb_buffer_byte_iterator {
  struct pb_buffer_iterator buffer_iterator;

  size_t page_offset;

  const char *current_byte;
};



/** Functional interfaces for accessing a byte in a memory region through the
 *  pb_buffer_byte_iterator class.
 *
 * This function is public and may be called by end users.
 */
const char *pb_buffer_byte_iterator_get_current_byte(
                   const struct pb_buffer_byte_iterator *buffer_byte_iterator);



/** The default size of pb_buffer memory regions. */
#define PB_BUFFER_DEFAULT_PAGE_SIZE                       4096



/** A structure that describes the internal strategy of a pb_buffer.
 *
 * A buffer strategy describes properties, such as page_size, and also
 * describes ways that a buffer will carry out actions such as writing of data
 * and management of memory regions.
 *
 * Some buffer classes may accept strategies as parameters to their factory
 * methods or constructors, in which case behaviour may be tuned on an instance
 * by instance basis.  Some buffer classes may not allow modification of
 * strategy due to their specific internal implementation or intended use.
 *
 * Once a strategy is implanted in a pb_buffer instance (its values are copied
 * into the pb_buffer instance), then these strategy values will not be changed
 * internally and certainly shouldnt be changed externally.
 */
struct pb_buffer_strategy {
  /** The size of memory regions that the pb_buffer will internally dynamically
   *  allocate.
   *
   * If this value is zero, there will be no limit on fragment size,
   * therefore write operations will cause allocations of memory regions equal
   * in size to the length of the source data.
   */
  size_t page_size;

  /** Indicates whether data written into the buffer (from another buffer) is
   *  to be referenced or copied.
   *
   * Available behaviours:
   * not_cloned (false):reference to the pb_data instance is incremented.
   *
   * cloned     (true): new pb_data instance created and memory regions copied.
   */
  bool clone_on_write;

  /** Indicates how data written to the pb_buffer will be fragmented.
   *
   * Available behaviours:
   * as source (false): source data fragmentation takes precedence.
   *
   *                    clone_on_write (false):
   *                    When clone_on_write is false, source pages are
   *                    moved to the target as is and pb_data references are
   *                    incremented.  Source pages are only fragmented due to
   *                    truncation to keep the final page within the write
   *                    operation length value.
   *                    
   *                    clone_on_write (true):
   *                    When clone_on_write is true, source pages are
   *                    fragmented according to the lower of the source
   *                    page size and the target page_size, which may be
   *                    zero, in which case the source fragment size is used.
   *
   * as target  (true): target pb_buffer page_size takes precedence.
   *
   *                    clone_on_write (false):
   *                    When clone_on_write is false, source pages that
   *                    are larger than the target page_size limit are split
   *                    into multiple pages in the target.
   *
   *                    clone_on_write (true):
   *                    When clone_on_write is true, source pages will be
   *                    coppied and packed into target fragments up to the
   *                    target page_size in size.
   */
  bool fragment_as_target;

  /** Indicates whether a pb_buffer rejects (fails to support) insert operations.
   *  That is: operations that write to places in the buffer other than the end.
   *
   * Available behaviours:
   * no reject (false): insert operations can be performed wuth expected results.
   *
   * reject     (true): insert operations will immediately return 0.
   */
  bool rejects_insert;
};



/** The structure that holds the operations that implement pb_buffer
 *  functionality.
 */
struct pb_buffer_operations {
  /** Return a revision stamp of the data.
   *
   * The data revision is a counter that is increased every time that data
   * already inside the buffer is modified.
   *
   * Operations that cause a change in data revision include:
   * seeking, rewinding, trimming, inserting, overwriting
   *
   * Operations that don't cause a change in data revision include:
   * expanding, writing to the end of the buffer, reading
   *
   * External readers such as line readers can use changes in this value to
   * determine whether their view on the buffer data is still relevant, thus
   * allowing them to keep reading, or whether their data view is invalidated
   * thus requiring them to restart.
   */
  uint64_t (*get_data_revision)(struct pb_buffer * const buffer);
  /** Increment the data revision.
   *
   * This is a protected function and should not be called externally.
   */
  void (*increment_data_revision)(
                                struct pb_buffer * const buffer);

  /** Return the amount of data in the buffer, in bytes. */
  uint64_t (*get_data_size)(struct pb_buffer * const buffer);


  /** Initialise an iterator to point to the first data in the buffer, or
   *  to the 'end' page if the buffer is empty.
   *
   * The iterator parameter is best to be a pointer to a stack object.
   */
  void (*get_iterator)(struct pb_buffer * const buffer,
                       struct pb_buffer_iterator * const buffer_iterator);
  /** Initialise an iterator to point to the 'end' of the buffer.
   *
   * The iterator parameter is best to be a pointer to a stack object.
   */
  void (*get_iterator_end)(struct pb_buffer * const buffer,
                           struct pb_buffer_iterator * const buffer_iterator);

  /** Indicates whether an iterator is currently pointing to the 'end' of
   *  a buffer or not.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_iterator* functions above.
   */
  bool (*iterator_is_end)(struct pb_buffer * const buffer,
                          const struct pb_buffer_iterator *buffer_iterator);
  /** Compare two iterators and indicate whether they are equal where equal is
   *  defined as pointing to the same page in the same buffer.
   *
   * The iterators passed to this function must be initialised using one of the
   * get_iterator* functions above.
   */
  bool (*iterator_cmp)(struct pb_buffer * const buffer,
                       const struct pb_buffer_iterator *lvalue,
                       const struct pb_buffer_iterator *rvalue);

  /** Moves an iterator to the next page in the data sequence.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_iterator* functions above.
   */
  void (*iterator_next)(struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator);
  /** Moves an iterator to the previous page in the data sequence.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_iterator* functions above.
   */
  void (*iterator_prev)(struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator);


  /** Initialise a byte iterator to the first byte of the first page in the
   *  buffer, or to the 'end' byte if the buffer is empty.
   *
   * The iterator parameter is best to be a pointer to a stack object.
   */
  void (*get_byte_iterator)(struct pb_buffer * const buffer,
                            struct pb_buffer_byte_iterator * const
                              buffer_byte_iterator);
  /** Initialise a byte iterator to the 'end' of the pb_buffer instance data.
   *
   * The iterator parameter is best to be a pointer to a stack object.
   */
  void (*get_byte_iterator_end)(struct pb_buffer * const buffer,
                                struct pb_buffer_byte_iterator * const
                                  buffer_byte_iterator);

  /** Indicates whether a byte iterator is currently pointing to the 'end' of
   *  a buffer or not.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_byte_iterator* functions above.
   */
  bool (*byte_iterator_is_end)(struct pb_buffer * const buffer,
                               struct pb_buffer_byte_iterator * const
                                 buffer_byte_iterator);
  /** Compare two byte iterators and indicate whether they are equal where
   *  equal is defined as pointing to the same byte in the same page in the
   *  same buffer.
   *
   * The iterators passed to this function must be initialised using one of the
   * get_byte_iterator* functions above.
   */
  bool (*byte_iterator_cmp)(struct pb_buffer * const buffer,
                            const struct pb_buffer_byte_iterator *lvalue,
                            const struct pb_buffer_byte_iterator *rvalue);

  /** Moves a byte iterator to the next byte in the data sequence.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_byte_iterator* functions above.
   */
  void (*byte_iterator_next)(struct pb_buffer * const buffer,
                             struct pb_buffer_byte_iterator * const
                               buffer_byte_iterator);
  /** Moves a byte iterator to the previous byte in the data sequence.
   *
   * The iterator passed to this function must be initialised using one of the
   * get_byte_iterator* functions above.
   */
  void (*byte_iterator_prev)(struct pb_buffer * const buffer,
                             struct pb_buffer_byte_iterator * const
                               buffer_byte_iterator);


  /** Create a pb_page instance with an attached pb_data.
   *
   * This is a protected function and should not be called externally.
   *
   * len: the size of the memory region to allocate.
   */
  struct pb_page *(*page_create)(
                             struct pb_buffer * const buffer,
                             size_t len);
  /** Create a pb_page instance with an attached pb_data that references the
   *  provided memory region.
   *
   * This is a protected function and should not be called externally.
   *
   * buf: the memory region to reference.
   * len: the size of the memory region to referenced.
   */
  struct pb_page *(*page_create_ref)(
                             struct pb_buffer * const buffer,
                             const uint8_t *buf, size_t len);


  /** Insert a pb_page instance into the pb_buffer.
   *
   * This is a protected function and should not be called externally.
   *
   * buffer_iterator: the position in the buffer, before which or into which
   *                  the page will be inserted.
   * offset: the position within the iterator page, before which the page will
   *         be inserted.
   *
   * page: the new page to insert into the buffer.  This page is created
   *       elsewhere within the buffer, using the buffers allocator.
   *
   * If the offset is zero, the page will be inserted in front of the iterator
   * page.  If the offset is non-zero, the iterator page will be split into
   * two sub-pages at the point of the offset, and the page will be inserted
   * between them.
   *
   * The return value is the amount of data successfully inserted into the
   * buffer.
   */
  uint64_t (*insert)(
                   struct pb_buffer * const buffer,
                   const struct pb_buffer_iterator *buffer_iterator,
                   size_t offset,
                   struct pb_page * const page);
  /** Increase the size of the buffer by adding data to the end.
   *
   * This is a protected function and should not be called externally.
   *
   * len: the amount of data to add in bytes.
   *
   * Depending on the buffer implementation details, the extended data may be
   * comprised of multiple pages.
   *
   * The return value is the amount of data successfully added to the buffer.
   */
  uint64_t (*extend)(
                   struct pb_buffer * const buffer,
                   uint64_t len);
  /** Increase the size of the buffer by adding data to the head.
   *
   * This is a protected function and should not be called externally.
   *
   * len: the amount of data to add in bytes.
   *
   * Depending on the buffer implementation details, the rewound data may be
   * comprised of multiple pages.
   *
   * The return value is the amount of data successfully added to the buffer.
   */
  uint64_t (*rewind)(
                   struct pb_buffer * const buffer,
                   uint64_t len);
  /** Seek the starting point of the buffer data.
   *
   * This is a protected function and should not be called externally.
   *
   * len: the amount of data to seek in bytes.
   *
   * Depending on the buffer implementation details, the seek operation may
   * zero one or more pages in the buffer, causing those pages to be freed,
   * and their corresponding data pages to have their use count decremented.
   *
   * The return value is the amount of data successfully seeked in the buffer.
   */
  uint64_t (*seek)(struct pb_buffer * const buffer,
                   uint64_t len);
  /** Trim the end of the buffer data.
   *
   * This is a protected function and should not be called externally.
   *
   * len: the amount of data to seek in bytes.
   *
   * Depending on the buffer implementation details, the trim operation may
   * zero one or more pages in the buffer, causing those pages to be freed,
   * and their corresponding data pages to have their use count decremented.
   *
   * The return value is the amount of data successfully trimmed from the
   * buffer.
   */
  uint64_t (*trim)(struct pb_buffer * const buffer,
                   uint64_t len);


  /** Write data from a memory region to the buffer.
   *
   * buf: the start of the source memory region.
   * len: the amount of data to write in bytes.
   *
   * Data will be appended to the end of the buffer.
   *
   * The return value is the amount of data successfully written to the
   * buffer.
   */
  uint64_t (*write_data)(struct pb_buffer * const buffer,
                         const uint8_t *buf,
                         uint64_t len);
  /** Write data from memory region to the buffer, referencing only.
   *
   * buf: the start of the source memory region.
   * len: the amount of data to write in bytes.
   *
   * Data will be appended to the end of the buffer.
   *
   * The return value is the amount of data successfully written to the
   * buffer.
   */
  uint64_t (*write_data_ref)(struct pb_buffer * const buffer,
                             const uint8_t *buf,
                             uint64_t len);
  /** Write data from a source buffer to the buffer.
   *
   * src_buffer: the buffer to write from.  This pb_buffer instance will not
   *             have its data modified.
   * len: the amount of data to insert in bytes.
   *
   * Data will be appended to the end of the buffer.
   *
   * The return value is the amount of data successfully inserted to the
   * buffer.
   */
  uint64_t (*write_buffer)(struct pb_buffer * const buffer,
                           struct pb_buffer * const src_buffer,
                           uint64_t len);


  /** Write data from a memory region to the buffer.
   *
   * buffer_iterator: the position in the buffer, before which or into which
   *                  the data will be inserted.
   * offset: the position within the iterator page, before which the data will
   *         be inserted.
   * buf: the start of the source memory region.
   * len: the amount of data to insert in bytes.
   *
   * If the offset is zero, the data will be inserted in front of the iterator
   * page.  If the offset is non-zero, the iterator page will be split into
   * two sub-pages at the point of the offset, and the data will be inserted
   * between them.
   *
   * The return value is the amount of data successfully inserted to the
   * buffer.
   */
  uint64_t (*insert_data)(struct pb_buffer * const buffer,
                          const struct pb_buffer_iterator * buffer_iterator,
                          size_t offset,
                          const void *buf,
                          uint64_t len);
  /** Write data from memory region to the buffer, referencing only.
   *
   * buffer_iterator: the position in the buffer, before which or into which
   *                  the data will be inserted.
   * offset: the position within the iterator page, before which the data will
   *         be inserted.
   * buf: the start of the source memory region.
   * len: the amount of data to write in bytes.
   *
   * If the offset is zero, the data will be inserted in front of the iterator
   * page.  If the offset is non-zero, the iterator page will be split into
   * two sub-pages at the point of the offset, and the data will be inserted
   * between them.
   *
   * The return value is the amount of data successfully inserted to the
   * buffer.
   */
  uint64_t (*insert_data_ref)(struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
                              size_t offset,
                              const uint8_t *buf,
                              uint64_t len);
  /** Write data from a source buffer to the buffer.
   *
   * buffer_iterator: the position in the buffer, before which or into which
   *                  the data will be inserted.
   * offset: the position within the iterator page, before which the data will
   *         be inserted.
   * src_buffer: the buffer to write from.  This pb_buffer instance will not
   *             have its data modified.
   * len: the amount of data to write in bytes.
   *
   * If the offset is zero, the data will be inserted in front of the iterator
   * page.  If the offset is non-zero, the iterator page will be split into
   * two sub-pages at the point of the offset, and the data will be inserted
   * between them.
   *
   * The return value is the amount of data successfully inserted to the
   * buffer.
   */
  uint64_t (*insert_buffer)(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *buffer_iterator,
                            size_t offset,
                            struct pb_buffer * const src_buffer,
                            uint64_t len);


  /** Overwrite the head of a buffer with data from a memory region.
   *
   * buf: the start of the source memory region.
   * len: the amount of data to write in bytes.
   *
   * Data is written to the head of the buffer, overwriting existing data.
   * No new storage will be allocated if len is greater than the size of the
   * buffer.
   *
   * The return value is the amount of data successfully written to the
   * buffer.
   */
  uint64_t (*overwrite_data)(struct pb_buffer * const buffer,
                             const uint8_t *buf,
                             uint64_t len);


  /** Read data from the head of a buffer to a memory region.
   *
   * buf: the start of the target memory region.
   * len: the amount of data to read in bytes.
   *
   * Data is read from the head of the buffer.  The amount of data read is the
   * lower of the size of the buffer and the value of len.
   *
   * The return value is the amount of data successfully read from the buffer.
   */
  uint64_t (*read_data)(struct pb_buffer * const buffer,
                        uint8_t * const buf,
                        uint64_t len);


  /** Clear all data in a buffer.
   *
   * Following this operation, the data size of the buffer will be zero.
   */
  void (*clear)(struct pb_buffer * const buffer);


  /** Destroy a buffer.
   *
   * Clear all data in the buffer, dismantle and clear all internal data
   * structures and free memory blocks associated with the buffer itself.
   */
  void (*destroy)(struct pb_buffer * const buffer);
};



/** Functional interfaces for the generic pb_buffer class.
 *
 * These functions are public and may be called by end users.
 */
uint64_t pb_buffer_get_data_revision(struct pb_buffer * const buffer);

uint64_t pb_buffer_get_data_size(struct pb_buffer * const buffer);


void pb_buffer_get_iterator(struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_buffer_get_iterator_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_buffer_iterator_is_end(
                            struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *buffer_iterator);
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
                        const struct pb_buffer_iterator *buffer_iterator,
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
                               const struct pb_buffer_iterator *buffer_iterator,
                               size_t offset,
                               const void *buf,
                               uint64_t len);
uint64_t pb_buffer_insert_data_ref(
                               struct pb_buffer * const buffer,
                               const struct pb_buffer_iterator *buffer_iterator,
                               size_t offset,
                               const uint8_t *buf,
                               uint64_t len);
uint64_t pb_buffer_insert_buffer(
                               struct pb_buffer * const buffer,
                               const struct pb_buffer_iterator *buffer_iterator,
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
 * The trivial buffer is a reference implementation of pb_buffer.
 *
 * It defines a classic strategy and operations structure and uses this when
 * the user does not specifiy their own.  However, all permutations of
 * strategy settings are supported internally by the trivial buffer.
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
 * clone_on_write is false: zero copy transfer of data from other buffers
 *
 * fragment_as_source is false: fragments written from other buffers or memory
 *                              regions are not additionally fragmented within
 *                              the 4k page limit
 *
 * rejects_insert is false: inserts into the middle of the buffer are allowed.
 */
const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void);



/** Get a trivial buffer operations structure.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_buffer_operations *pb_get_trivial_buffer_operations(void);



/** Factory functions producing trivial pb_buffer instances.
 *
 * Users may use either default, trivial strategy and allocator, or supply
 * their own.
 */
struct pb_buffer *pb_trivial_buffer_create(void);
struct pb_buffer *pb_trivial_buffer_create_with_strategy(
                                    const struct pb_buffer_strategy *strategy);
struct pb_buffer *pb_trivial_buffer_create_with_alloc(
                                    const struct pb_allocator *allocator);
struct pb_buffer *pb_trivial_buffer_create_with_strategy_with_alloc(
                                    const struct pb_buffer_strategy *strategy,
                                    const struct pb_allocator *allocator);



/** Specific implementations of pb_trivial_buffer operations.
 *
 * These are protected functions and should not be called externally.
 */
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
                            const struct pb_buffer_iterator *buffer_iterator);
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
                              size_t len);
struct pb_page *pb_trivial_buffer_page_create_ref(
                              struct pb_buffer * const buffer,
                              const uint8_t *buf, size_t len);


uint64_t pb_trivial_buffer_insert(
                              struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
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
                              const struct pb_buffer_iterator *buffer_iterator,
                              size_t offset,
                              const void *buf,
                              uint64_t len);
uint64_t pb_trivial_buffer_insert_data_ref(
                              struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
                              size_t offset,
                              const uint8_t *buf,
                              uint64_t len);
uint64_t pb_trivial_buffer_insert_buffer(
                              struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
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






/* Pre-declare operations. */
struct pb_data_reader_operations;



/** An interface for reading data from a pb_buffer.
 *
 * The data reader attaches to a pb_buffer instance and provides an interface
 * for performing continuous reads from that buffer.  The reader keeps track
 * of its last read position in the buffer as it completes one read and
 * allows the user to continue reads from that same point in the next read.
 */
struct pb_data_reader {
  const struct pb_data_reader_operations *operations;

  struct pb_buffer *buffer;
};



/** The structure that holds the operations that implement pb_data_reader
 *  functionality.
 */
struct pb_data_reader_operations {
  /** Read data from the pb_buffer instance, into a memory region.
   *
   * buf: the start of the target memory region.
   * len: the amount of data to read in bytes.
   *
   * Data is read from the head of the buffer.  The amount of data read is the
   * lower of the size of the buffer and the value of len.
   *
   * Following a data read, the data reader will retain the position of the
   * end of the read, thus, a subsequent call to read will continue from
   * where the last read finished.
   *
   * However, if the buffer undergoes an operation that alters its data
   * revision, a subsequent call to read on the data reader will read from the
   * beginning of the buffer.
   *
   * The return value is the amount of data successfully read from the buffer.
   */
  uint64_t (*read)(struct pb_data_reader * const data_reader,
                   uint8_t * const buf, uint64_t len);

  /** Reset the data reader so that subsequent reads start at the beginning of
   *  the buffer. */
  void (*reset)(struct pb_data_reader * const data_reader);

  /** Destroy the data reader.
   *
   * Dismantle and clear all internal data structures and free memory blocks
   * associated with the data reader itself.
   */
  void (*destroy)(struct pb_data_reader * const data_reader);
};



/** Functional interfaces for the generic pb_data_reader class.
 *
 * These functions are public and may be called by end users.
 */
uint64_t pb_data_reader_read(struct pb_data_reader * const data_reader,
                             uint8_t * const buf, uint64_t len);

void pb_data_reader_reset(struct pb_data_reader * const data_reader);
void pb_data_reader_destroy(
                          struct pb_data_reader * const data_reader);



/** The trivial data reader implementation and its supporting functions.
 *
 * The trivial data reader is a reference implementation of pb_data_reader.
 *
 * It interacts with its attached pb_buffer instance using only the public,
 * functional interfaces of pb_buffer, so it is independent of the internal
 * implementation details of any buffer subclass.
 */



/** Get a trivial data reader operations structure.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_data_reader_operations
  *pb_get_trivial_data_reader_operations(void);



/** Factory functions producing trivial pb_data_reader instances.
 *
 * buffer: the buffer to attach the data reader to.
 */
struct pb_data_reader *pb_trivial_data_reader_create(
                                              struct pb_buffer * const buffer);



/** Specific implementations of pb_trivial_data_reader operations.
 *
 * These are protected functions and should not be called externally.
 */
uint64_t pb_trivial_data_reader_read(struct pb_data_reader * const data_reader,
                                     uint8_t * const buf, uint64_t len);

void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader);
void pb_trivial_data_reader_destroy(
                                  struct pb_data_reader * const data_reader);






/* Pre-declare the operations. */
struct pb_line_reader_operations;



/** An interface for searching a pb_buffer for lines, delimited by either
 *  '\r\n' or '\n'
 *
 * The line reader will use the subject buffers' data revision to monitor the
 * state of the buffer data, which means that a search that previously failed
 * to find a line end can be continued at the same point whe nnew data is
 * wriitten to the end of the buffer.  However, modifications to the buffer
 * that cause the data revision to be updated will invalidate the line search
 * and require the line reader to re-start at the begining of the buffer.
 */
struct pb_line_reader {
  const struct pb_line_reader_operations *operations;

  struct pb_buffer *buffer;
};



/** The maximum size of lines supported by pb_line_reader will be limited to
 *  this value. Any line discovery that reaches this position value during a
 *  search will set an artificial newline at this point.
 */
#define PB_LINE_READER_DEFAULT_LINE_MAX                   16777216L



/** The structure that holds the operations that implement pb_line_reader
 *  functionality.
 */
struct pb_line_reader_operations {
  /** Indicates whether a line exists at the head of a pb_buffer instance.
   *
   * This function will search the attached pb_buffer for either a '\n' or
   * '\r\n' pattern.
   * If one of these patterns is found, then the search position information
   * is recorded and a true result will be returned.
   * If no such pattern is found, then the position at the end of the search
   * is recorded and a false result will be returned.
   * New data may be added to the buffer and because this will not affect the
   * data revision, subsequent calls to this function will also continue from
   * the last search end position, allowing the user to run an active line end
   * search while writing to the buffer.  However, if the buffer is modified
   * in such a way that alters the data revision, line end searches will need
   * to start at the head of the buffer again.
   */
  bool (*has_line)(struct pb_line_reader * const line_reader);

  /** Returns the length of the line discovered by has_line.
   *
   * If the has_line function has been previously called and retuned true,
   * then this function will return the length of the line that was
   * discovered.
   * If no line was discovered or has_line was never called, zero will be
   * returned.
   */
  size_t (*get_line_len)(struct pb_line_reader * const line_reader);
  /** Read data from the discovered line into a memory region.
   *
   * buf: the start of the target memory region.
   * len: the amount of data to read in bytes.
   *
   * Data is read from the head of the buffer.  The amount of data read is the
   * lower of the length of the line and the value of len.
   */
  size_t (*get_line_data)(struct pb_line_reader * const line_reader,
                          uint8_t * const buf, uint64_t len);

  /** Seek the buffer data to the position after the line.
   *
   * This function will take into account the nature of the line end (whether
   * it includes a '\r' or not).
   */
  size_t (*seek_line)(struct pb_line_reader * const line_reader);

  /** Marks the present position of line search as a line end.
   *
   * Even if no line end has yet been found, this function will allow the
   * present search position to be treated as line end, allowing extraction of
   * line data.
   * If the character proceeding the termination point is a '\r', it will be
   * ignored in the line length calculation and included in the line data.
   */
  void (*terminate_line)(struct pb_line_reader * const line_reader);
  /** Marks the present position of line search as a line end.
   *
   * Even if no line end has yet been found, this function will allow the
   * present search position to be treated as line end, allowing extraction of
   * line data.
   * If the character proceeding the termination point is a '\r', it will be
   * included in the line length calculation and excluded from the line data.
   */
  void (*terminate_line_check_cr)(struct pb_line_reader * const line_reader);

  /** Indicates whether the line discovered by has_line is terminated by a
   *  '\r\n' (true) or
   *  '\n' (false)
   */
  bool (*is_crlf)(struct pb_line_reader * const line_reader);
  /** Indicates whether line search has reached the end of the buffer. */
  bool (*is_end)(struct pb_line_reader * const line_reader);

  /** Clone the state of the line reader into a new instance. */
  struct pb_line_reader *(*clone)(struct pb_line_reader * const line_reader);

  /** Reset the current line discovery progress information, including
   *  positions of discovered lines. */
  void (*reset)(struct pb_line_reader * const line_reader);

  /** Destroy the line reader.
   *
   * Dismantle and clear all internal data structures and free memory blocks
   * associated with the line reader itself.
   */
  void (*destroy)(struct pb_line_reader * const line_reader);
};



/** Functional infterface for the generic pb_buffer class.
 *
 * These functions are public and may be called by end users.
 */
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



/** The trivial line reader implementation and its supporting functions.
 *
 * The trivial line reader is a reference implementation of pb_line_reader.
 *
 * It interacts with its attached pb_buffer instance using only the public,
 * functional interfaces of pb_buffer, so it is independent of the internal
 * implementation details of any buffer subclass.
 */



/** Get a trivial line reader operations structure
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_line_reader_operations
  *pb_get_trivial_line_reader_operations(void);



/** Factory functions producing trivial pb_line_reader instances.
 *
 * buffer: the buffer to attach the data reader to.
 */
struct pb_line_reader *pb_trivial_line_reader_create(
                                          struct pb_buffer * const buffer);



/** Specific implementations of pb_trivial_line_reader operations.
 *
 * These are protected functions and should not be called externally.
 */
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
