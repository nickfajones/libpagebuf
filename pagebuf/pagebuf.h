/*******************************************************************************
 *  Copyright 2015 - 2017 Nick Jones <nick.fa.jones@gmail.com>
 *  Copyright 2016 Network Box Corporation Limited
 *      Jeff He <jeff.he@network-box.com>
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
 * passing quickly through the system or not, this data will need to be stored
 * after it is received from the input side, then arranged for writing back to
 * the output side.
 * Additionally, the data may require processing as it moves in then out of
 * the system.  Processing includes parsing and analysis, and can even include
 * modification.
 *
 * When that software system is non-blocking and event driven, an additional
 * challenge exists in that data may arrive in a piecewise manner with
 * uncertain size and delay patterns.  Authors of such applications may need
 * to be access the data in a sequential way, or in a way that deals as little
 * as possible with the underlying fragmentation.  libpagebuf is designed to
 * provide a solution to these data storage challenges.
 *
 * On the surface, through the use of the primary pb_buffer class, libpagbuf
 * provides a means of writing or copying blocks of data, then a means of
 * reading and manipulating that data as if it was sequential and unfragmented.
 * An author may use pb_buffer to receive data from input sources as fragments,
 * then perform read actions such as searching and copying in addition to some
 * more intrusive actions such as insertion or truncation on that data, without
 * any regard for the underlying fragmentation, positioning in system memory
 * (or other storage) or even ordering in memory of the the data.
 *
 * libpagebuf is also designed with concerns of non-blocking event driven
 * and multithreaded systems in mind, through the inclusion of interfaces for
 * the integration of custom memory allocators, debugging to check internal
 * structure integrity and thread exclusivity.
 *
 * The core API and implementation is in C but a thin C++ wrapper is provided.
 * There exist C++ objects that comply to the ASIO ConstBufferSequence,
 * MutableBufferSequence and DynamicBufferSequence concepts, as well as the
 * BidirectionanIterator concept used in particular by boost::regex.
 *
 * libpagebuf is designed for efficiency, using reference counting and
 * zero-copy semantics, as well as providing a class like interface in C that
 * provides a path for subclassing and modifying implementation details.
 */

/** libpagebuf classes
 *
 * Although it is written in C, libpagebuf interfaces with its data structures
 * in an object like style.
 *
 * Each core object possesses a '_operations' structure, which is a group of
 * function pointers acting like a vtable, providing an overridable index of
 * object functionality.
 * Concrete implementations of core objects will define a const pointer to an
 * _operations structure, populated with functions that are either pr-defined,
 * most often already in use in the 'base class', or with functions that are
 * unique to the relevant implementation, that override specific behaviour of
 * some of the functions of the base class.
 *
 * Concrete implementations will also define 'factory' functions, which are
 * akin to constructors in C++.  These factory functions may accept specific
 * arguments to configure the behaviour and functionality of the constructed
 * instance implementations.  Factory functions return pointers to fully
 * initialised instances of objects.  They will most often return the pointer
 * in the form of the base type, to allow maximum polymorphism, however in
 * special cases, factory functions may return subclasses to allow specific
 * functions to be executed against them, but in these cases, a convenience
 * conversion function should be provided to convert the subclass to the base
 * class.
 *
 * Concrete implementations do not need to to make their _operations structures
 * publicly available, for example through a public accessor function declared
 * in a public header file.  The binding of an object to its _operations can be
 * done opaquely inside the factory function.
 *
 * In order to subclass an object, use the typical C object embedding method
 * where an instance of the base class object is the first member of the
 * subclass object.  The factory function will return the address and type of
 * the embedded base object, thus 'downcast'ing the subclass. Similarly, when a
 * subclass specific over-ridden function receives a pointer to the base class
 * object as a parameter, it can 'upcast' the base object pointer back up to
 * the subclass.
 */

/** Thread safety
 *
 * The pb_buffer, its supporting classes and API is explicitly NOT thread safe.
 *
 * There is no notion of locking in any of the pb_buffer operations.  Reference
 * counted objects such as pb_data do not have their counts modified in a
 * globally atomic fashion.
 *
 * It is the responsibility of authors to ensure that buffers are not accessed
 * concurrently from across thread boundaries, and even if such a thing is
 * done in a serialised way, authors must ensure that pb_data cleanup
 * operations, consider thread safety.
 */






/* Pre-declare allocator operations. */
struct pb_allocator_operations;



/** Indicates the intended use of an allocated memory block.
 *
 * type_struct: The memory region will be used to store a data structure.
 *              The region will be initialised to all zero bytes before it is
 *              returned to the caller.
 * type_region: The memory block will be used as a memory region for the
 *              storage of data.  It will not be initialised.
 */
enum pb_allocator_type {
  pb_alloc_type_struct,
  pb_alloc_type_region,
};



/** Responsible for allocation and freeing of blocks of memory
 *
 * Allocate and free memory blocks through functions operating on an allocator.
 * The allocator subclass will provide state and logic that define a memory
 * allocation strategy.
 */
struct pb_allocator {
  const struct pb_allocator_operations *operations;
};



/** The structure that holds the operations that implement pb_allocator
 *  functionality.
 *
 * Authors:
 * pb_allocator its subclasses are briefly mentioned in public APIs, but this
 * is usually where they are the return value of public functions.  These
 * references are expected to be passed immediately into object factory
 * functions in order to use the pb_allocator to create that object, and also
 * to bind the pb_allocator to the object but it is not expected that
 * Application Authors will interract directly with pb_allocator instances.
 * 
 * Having said that, it is not strictly illegal to do so, and doing so should
 * not confound an allocator implementation.
 *
 * Implementors:
 * While pb_allocator operations are only invoked internally by pagebuf classes,
 * it can be expected that Authors will use these operations in their own
 * Allocator functions are used by other pagebuf objects to perform memory
 * allocations and frees.  Although it is not encouraged that these interfaces
 * be used directly by Authors, it is not illegal or confounding to libpagebuf,
 * and Implementors who provide custom allocator backends should accomodate
 * such usage.
 */
struct pb_allocator_operations {
  /** Allocate a memory block.
   *
   * type: indicates what the allocated memory block will be used for
   *
   * size: the size of the memory block to allocate.
   */
  void *(*alloc)(const struct pb_allocator *allocator,
                 enum pb_allocator_type type, size_t size);
  /** Free a memory block.
   *
   * type: indicates how the memory block was used.
   *
   * obj: the address of beginning of the memory region.
   *
   * size: indicates the size of the memory region that was allocated and now
   *       freed.
   */
  void  (*free)(const struct pb_allocator *allocator,
                enum pb_allocator_type type, void *obj, size_t size);
};



/** Functional interfaces for the pb_allocator class.
 *
 * These interfaces should be used to invoke the allocator operations.
 *
 * Given that allocators are protected members of buffers, these functions are
 * not intended to be called by authors, however they can be used as a useful
 * interface for a host application to use the allocator consistently along
 * with libpagebuf, especially in conjunction with the trivial built in
 * allocator defined below.
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
 * The trivial allocator simply wraps around malloc and free.  It may be called
 * during the construction of other types, namely pb_buffer instances.
 *
 * This function is public and available to authors.
 */
const struct pb_allocator *pb_get_trivial_allocator(void);






/* Pre-declare data operations. */
struct pb_data_operations;



/** Describes a memory region, as a starting address and length. */
struct pb_data_vec {
  /** The starting address of the region. */
  uint8_t *base;
  /** The length of the region. */
  size_t len;
};



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
  pb_data_owned,
  pb_data_referenced,
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
 * data.base <= page.base + page.len < data.base + data.len
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



/** Utility function to set the data object of a page. */
void pb_page_set_data(struct pb_page * const page,
                      struct pb_data * const data);



/** Functional interfaces for accessing a memory region through pb_page.
 *
 * These functions are public and may be called by authors.
 */
void *pb_page_get_base(const struct pb_page *page);
void *pb_page_get_base_at(
                       const struct pb_page *page, size_t offset);
size_t pb_page_get_len(const struct pb_page *page);






/* Pre-declare strategy and operations. */
struct pb_buffer_strategy;
struct pb_buffer_operations;



/** The base pb_buffer class.
 *
 * The pb_buffer class is the focus of libpagebuf.  It represents the core
 * functionality of the library and is the primary interface for accessing
 * this functionality.
 *
 * The pb_buffer represents data, that the author has written in, or wishes
 * to read out.  The pb_buffer is FIFO in terms of data and will preserve the
 * order of data read out.
 *
 * The pb_buffer itself is a base class that merely provides a framework for
 * concrete implementations.  Implementations will opaquely manage the handling
 * of data written in, the storage of that data, then the retrieval of data as
 * it is read out.
 *
 */
struct pb_buffer {
  /** The description of the core behaviour of the buffer.  These strategy
   *  flags should be constant for the lifetime of a pb_buffer instance.
   */
  const struct pb_buffer_strategy *strategy;

  /** The structure describing the concrete implementation of the buffer
   *  functions.  These should not only be constant for the lifetime of the
   *  buffer instance, they should be immutable and identical for all instances
   *  of same buffer classes.
   */
  const struct pb_buffer_operations *operations;

  /** The allocator used by the buffer instance to perform all structure and
   *  memory region allocations.
   */
  const struct pb_allocator *allocator;
};






/** A structure used to sequentially access data regions in a pb_buffer.
 *
 * Iterators provide an interface to traverse the pages contained within a
 * pb_buffer.
 *
 * Because pb_buffer subtypes may vary in how they split their data into pages,
 * the iterator is used as a token for page traversal and access to data.
 *
 * Iterators are initialised and manipulated using buffer operations, meaning
 * the implementation of their functions are buffer subclass specific, thus
 * allowing authors to tailor iterator operations to suit their pb_buffer
 * subclasses' internal data arrangement.
 *
 * Iterators act similarly to container iterators in C++:
 * - When initialised to the start of a non-empty pb_buffer instance, using the
 *   buffer operation:
 *     pb_buffer_get_iterator,
 *   then the iterator will point to a pb_data_vec refererence to the first
 *   data in that buffer.  The functions:
 *     pb_buffer_iterator_get_base and
 *     pb_buffer_iterator_get_len
 *   can be used to access the vector values.
 *
 * - When initialised to the start of an empty pb_buffer instance, or as the
 *   end of the pb_buffer using the buffer operation:
 *     pb_buffer_get_end_iterator,
 *   the pb_data_vec reference will be to a special 'end' data reference.
 *   This 'end' iterator value will cause the buffer operation:
 *     pb_buffer_is_end_iterator
 *   to return true.
 *
 * - To advance through the pages of a pb_buffer, execute the following buffer
 *   operation on an initialised, non-'end' iterator:
 *     pb_buffer_next_iterator
 *   If the previously referenced buffer page was the last in the pb_buffer
 *   instances sequence, the iterator will now point to the 'end' data
 *   reference and the buffer operation:
 *     pb_buffer_is_end_iterator
 *   will now return true, and forward iteration should go no further.
 *   It is not advised to call:
 *     pb_buffer_next_iterator
 *   on the 'end' reference, as the result may be an invalid iterator,
 *   depending on the implementation of the pb_buffer subclass.
 *
 * - To reverse through the pages of a pb_buffer, execute the following buffer
 *   operation on an initialised, non-'end' iterator:
 *     pb_buffer_prev_iterator
 *   If the previously referenced buffer page was the first in the pb_buffer
 *   instances sequence, the iterator will now point to the 'end' data
 *   reference and the buffer operation:
 *     pb_buffer_is_end_iterator
 *   will now return true and reverse iteration should go no further.
 *   It is not advised to call:
 *     pb_buffer_prev_iterator
 *   on the 'end' reference, as the result may be an invalid iterator,
 *   depending on the implementation of the pb_buffer subclass.
 * 
 * Implementors who create new subclasses of pb_buffer should make sure their
 * implementations obey the above behaviour.
 */
struct pb_buffer_iterator {
  struct pb_data_vec *data_vec;
};



/** Functional interfaces for accessing a memory region through the
 *  pb_buffer_iterator class.
 *
 * These functions are public and may be called by end users.
 */
void *pb_buffer_iterator_get_base(
                             const struct pb_buffer_iterator *buffer_iterator);
void *pb_buffer_iterator_get_base_at(
                             const struct pb_buffer_iterator *buffer_iterator,
                             size_t offset);
size_t pb_buffer_iterator_get_len(
                             const struct pb_buffer_iterator *buffer_iterator);






/** A structure used to sequentially access data regions in a pb_buffer,
 *  one byte at a time.
 *
 * The semantics of initialisation and manipulation of a byte iterator is
 * the same as iterators:
 * - Operations:
 *     pb_buffer_get_byte_iterator
 *     pb_buffer_get_end_byte_iterator
 *     pb_buffer_is_end_byte_iterator
 *     pb_buffer_next_byte_iterator
 *     pb_buffer_prev_byte_iterator
 *
 * - To access the byte currently referenced by the iterator:
 *     pb_buffer_byte_iterator_get_current_byte
 * */
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
char pb_buffer_byte_iterator_get_current_byte(
                   const struct pb_buffer_byte_iterator *buffer_byte_iterator);






/** The default size of pb_buffer memory regions. */
#define PB_BUFFER_DEFAULT_PAGE_SIZE                       4096
/** The hard maximum size of automatically sized memory regions. */
#define PB_BUFFER_MAX_PAGE_SIZE                           16777216L



/** A structure that describes the internal strategy of a pb_buffer.
 *
 * A buffer strategy describes of a pb_buffer instance (or class), the
 * property of: page_size
 *
 * A buffer strategy also describes how a pb_buffer subclass will behave during
 * specific internal operations.  These behaviours can be categorised as:
 * - Data Treatment: how data written into the buffer is treated, which may
 *   be for the purpose of preserving specific arrangements of data inside the
 *   pb_buffer subclass, or data treatment may reflect some aspect of the
 *   specific implementation of the pb_buffer subclass.
 *
 * - Feature: control specific operations on bufferred data.  These flags are
 *   usually artefacts of specific pb_buffer subclass backends, that due to
 *   their nature, may or may not support some operations.
 *
 * Some buffer classes may accept strategies as parameters to their factory
 * methods or constructors, in which case behaviour may be tuned on an instance
 * by instance basis.  Some buffer classes may not allow modification of
 * strategy due to their specific internal implementation or intended use.
 *
 * Once a strategy is implanted in a pb_buffer instance (its values are copied
 * into the pb_buffer instance), then these strategy values will not be changed
 * internally and certainly shouldnt be changed externally.
 *
 * Implementors who create new subclasses of pb_buffer should restrict or disable
 * access to their subclasses' buffer strategy as necessary, because the
 * strategy not only informs the implementation on how it should behave, it
 * also informs Authors on how implementations will behave.
 */
struct pb_buffer_strategy {
  /** The size of memory regions that the pb_buffer will internally dynamically
   *  allocate.
   *
   * If this value is zero, there will be no limit on fragment size,
   * therefore write operations will cause allocations of memory regions equal
   * in size to the length of the source data.  This logic will apply
   * implicitly in the different scenarios explained below.
   *
   * However a hard limit to the page size is described in the variable
   * PB_BUFFER_MAX_PAGE_SIZE introduced above.
   */
  size_t page_size;

  /** Data Treatment Flags: how data is to be treated as it is written into the
   *  buffer.
   */

  /** clone_on_write: indicates whether data written into the buffer (from
   *  another buffer) is to be referenced or copied.
   *
   * Supported behaviours:
   * false(not cloned): data is moved between the source and target buffer by
   *                    transferring page data, and incrementing pb_data
   *                    reference counts.
   *
   * true     (cloned): data is moved between the source and target buffer by
   *                    copying: a new buffer page is created with a new memory
   *                    region, and the contents of the source memory region is
   *                    copied to the new data region.
   */
  bool clone_on_write;

  /** fragment_as_target: indicates how data written to the pb_buffer will be
   *  fragmented.
   *
   * Supported behaviours:
   * false (as source): source data fragmentation takes precedence
   *
   *                    clone_on_write (false):
   *                    When clone_on_write is false, pages from the source
   *                    buffer are moved to the target buffer status quo,
   *                    without being fragmented further, unless the size of a
   *                    source page exceeds the page_size of the target, in
   *                    which case it is split into multiple pages in the
   *                    target, albeit each referencing different parts of the
   *                    same pb_data instance.
   *
   *                    clone_on_write (true):
   *                    When clone_on_write is true, data is copied between the
   *                    source and target buffers, with the fragmentation of
   *                    the new target pages matching that of the source pages,
   *                    unless the source page size exceeds the target buffer
   *                    page_size.
   *
   * true  (as target): target pb_buffer page_size takes precedence:
   *
   *                    clone_on_write (false):
   *                    When clone_on_write is false, source pages that
   *                    are larger than the target page_size limit are split
   *                    into multiple pages in the target.
   *
   *                    clone_on_write (true):
   *                    When clone_on_write is true, source pages will be
   *                    copied and packed into target fragments up to the
   *                    target page_size in size.
   */
  bool fragment_as_target;

  /** Feature Flags: control access to functions that alter the state of
   *  the buffer.
   */

  /** Indicates whether a pb_buffer rejects (fails to support) insert
   *  operations.  That is: operations that write to places in the buffer
   *  other than the end.
   *
   * Available behaviours:
   * no reject (false): insert operations can be performed with expected
   *                    results.
   *
   * reject     (true): insert operations will immediately return 0.
   */
  bool rejects_insert;

  /** Indicates whether a pb_buffer rejects (fails to support) the extend or
   *  reserve operations.
   *
   * Available behaviours:
   * no reject (false): extend/reserve operations can be performed with
   *                    expected results.
   *
   * reject     (true): extend/reserve operations will immediately return 0.
   */
  bool rejects_extend;

  /** Indicates whether a pb_buffer rejects (fails to support) the rewind
   *  operation.
   *
   * Available behaviours:
   * no reject (false): rewind operations can be performed with expected
   *                    results.
   *
   * reject     (true): rewind operations will immediately return 0.
   */
  bool rejects_rewind;

  /** Indicates whether a pb_buffer rejects (fails to support) the seek
   *  operation.
   *
   * Available behaviours:
   * no reject (false): seek operations can be performed with expected
   *                    results.
   *
   * reject     (true): seek operations will immediately return 0.
   */
  bool rejects_seek;

  /** Indicates whether a pb_buffer rejects (fails to support) the trim
   *  operation.
   *
   * Available behaviours:
   * no reject (false): trim operations can be performed with expected
   *                    results.
   *
   * reject     (true): trim operations will immediately return 0.
   */
  bool rejects_trim;

  /** Indicates whether a pb_buffer rejects (fails to support) write
   *  operations
   *
   * Available behaviours:
   * no reject (false): write operations can be performed with expected
   *                    results.
   *
   * reject     (true): write operations will immediately return 0.
   */
  bool rejects_write;

  /** Indicates whether a pb_buffer rejects (fails to support) the overwrite
   *  operation.
   *
   * Available behaviours:
   * no reject (false): overwrite operations can be performed with expected
   *                    results.
   *
   * reject     (true): overwrite operations will immediately return 0.
   */
  bool rejects_overwrite;
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
   * External readers such as line readers can use changes in this value to
   * determine whether their view on the buffer data is still relevant, thus
   * allowing them to keep reading, or whether their data view is invalidated
   * thus requiring them to reset or invalidate (immediately go to the end).
   *
   * Operations that cause a change in data revision in trivial buffers are:
   * seek, rewind, trimm, insert, overwrite
   *
   * Operations that don't cause a change in data revision in trivial buffers
   * are:
   * expand, write (to the end of the buffer), read, iteration.
   */
  uint64_t (*get_data_revision)(struct pb_buffer * const buffer);
  /** Increment the data revision.
   *
   * This is a protected function and should not be called externally.
   */
  void (*increment_data_revision)(
                                struct pb_buffer * const buffer);

  /** Return the amount of data in the buffer, in bytes.
   */
  uint64_t (*get_data_size)(struct pb_buffer * const buffer);


  /** Initialise an iterator to point to the first page in the buffer, or
   *  to the 'end' page if the buffer is empty.
   */
  void (*get_iterator)(struct pb_buffer * const buffer,
                       struct pb_buffer_iterator * const buffer_iterator);
  /** Initialise an iterator to point to the 'end' page of the buffer.
   */
  void (*get_end_iterator)(struct pb_buffer * const buffer,
                           struct pb_buffer_iterator * const buffer_iterator);

  /** Indicates whether an iterator is currently pointing to the 'end' of
   *  a buffer or not.
   */
  bool (*is_end_iterator)(struct pb_buffer * const buffer,
                          const struct pb_buffer_iterator *buffer_iterator);
  /** Compare two iterators and indicate whether they are equal where equal is
   *  defined as pointing to the same page in the same buffer.
   */
  bool (*cmp_iterator)(struct pb_buffer * const buffer,
                       const struct pb_buffer_iterator *lvalue,
                       const struct pb_buffer_iterator *rvalue);

  /** Moves an iterator to the next page in the data sequence.
   */
  void (*next_iterator)(struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator);
  /** Moves an iterator to the previous page in the data sequence.
   */
  void (*prev_iterator)(struct pb_buffer * const buffer,
                        struct pb_buffer_iterator * const buffer_iterator);


  /** Initialise a byte iterator to the first byte of the first page in the
   *  buffer, or to the 'end' byte if the buffer is empty.
   */
  void (*get_byte_iterator)(struct pb_buffer * const buffer,
                            struct pb_buffer_byte_iterator * const
                              buffer_byte_iterator);
  /** Initialise a byte iterator to the 'end' of the pb_buffer instance data.
   */
  void (*get_end_byte_iterator)(struct pb_buffer * const buffer,
                                struct pb_buffer_byte_iterator * const
                                  buffer_byte_iterator);

  /** Indicates whether a byte iterator is currently pointing to the 'end' of
   *  a buffer or not.
   */
  bool (*is_end_byte_iterator)(struct pb_buffer * const buffer,
                               struct pb_buffer_byte_iterator * const
                                 buffer_byte_iterator);
  /** Compare two byte iterators and indicate whether they are equal where
   *  equal is defined as pointing to the same byte in the same page in the
   *  same buffer.
   */
  bool (*cmp_byte_iterator)(struct pb_buffer * const buffer,
                            const struct pb_buffer_byte_iterator *lvalue,
                            const struct pb_buffer_byte_iterator *rvalue);

  /** Moves a byte iterator to the next byte in the data sequence.
   */
  void (*next_byte_iterator)(struct pb_buffer * const buffer,
                             struct pb_buffer_byte_iterator * const
                               buffer_byte_iterator);
  /** Moves a byte iterator to the previous byte in the data sequence.
   */
  void (*prev_byte_iterator)(struct pb_buffer * const buffer,
                             struct pb_buffer_byte_iterator * const
                               buffer_byte_iterator);


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
  uint64_t (*insert)(
                   struct pb_buffer * const buffer,
                   const struct pb_buffer_iterator *buffer_iterator,
                   size_t offset,
                   struct pb_page * const page);
  /** Increase the size of the buffer by adding data to the end.
   *
   * len: the amount of data to add in bytes.
   *
   * The return value is the amount of capacity successfully added to the
   * buffer.
   */
  uint64_t (*extend)(
                   struct pb_buffer * const buffer,
                   uint64_t len);
  /** Assure the size of the buffer is at least a specific size.
   *
   * size: the minimum size of the buffer.
   *
   * If the current size of the buffer is less than the requested size, then
   * the buffer will be extended to meet that size.
   * If the current size of the buffer is greater than the requested size, then
   * the buffer will remain unchanged.
   *
   * The return value is the amount of capacity successfully added to the
   * buffer.
   */
  uint64_t (*reserve)(
                   struct pb_buffer * const buffer,
                   uint64_t size);
  /** Increase the size of the buffer by adding data to the head.
   *
   * len: the amount of data to add in bytes.
   *
   * The return value is the amount of data successfully added to the buffer.
   */
  uint64_t (*rewind)(
                   struct pb_buffer * const buffer,
                   uint64_t len);
  /** Seek the starting point of the buffer.
   *
   * len: the amount of data to seek in bytes.
   *
   * The seek operation may cause one or more pages at the begining of the
   * buffer to be released, causing those pages to be freed and the
   * corresponding data pages to have their reference count decremented.
   *
   * The return value is the amount of data successfully seeked in the buffer.
   */
  uint64_t (*seek)(struct pb_buffer * const buffer,
                   uint64_t len);
  /** Trim the end of the buffer data.
   *
   * len: the amount of data to seek in bytes.
   *
   * The trim operation may cause one or more pages at the end of the buffer to
   * be released, causing those pages to be freed and their corresponding data
   * pages to have their reference count decremented.
   *
   * The return value is the amount of data successfully trimmed from the
   * buffer.
   */
  uint64_t (*trim)(struct pb_buffer * const buffer,
                   uint64_t len);


  /** Insert data from a memory region to the buffer.
   *
   * buffer_iterator: the page in the buffer, before or into which the data
   *                  will be inserted.
   *
   * offset: the position within the iterator page, before which the data will
   *         be inserted.
   *
   * buf: the start of the source memory region.
   *
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
  /** Insert data from a memory region to the buffer, referencing only.
   *
   * buffer_iterator: the page in the buffer, before or into which the data
   *                  will be inserted.
   *
   * offset: the position within the iterator page, before which the data will
   *         be inserted.
   *
   * buf: the start of the source memory region.
   *
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
                              const void *buf,
                              uint64_t len);
  /** Insert data from a source buffer to the buffer.
   *
   * buffer_iterator: the page in the buffer, before or into which the data
   *                  will be inserted.
   *
   * offset: the position within the iterator page, before which the data will
   *         be inserted.
   *
   * src_buffer: the buffer to write from.  This pb_buffer instance will not
   *             have its data modified.
   *
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


  /** Write data from a memory region to the buffer.
   *
   * buf: the start of the source memory region.
   *
   * len: the amount of data to write in bytes.
   *
   * Data will be appended to the end of the buffer.
   *
   * The return value is the amount of data successfully written to the
   * buffer.
   */
  uint64_t (*write_data)(struct pb_buffer * const buffer,
                         const void *buf,
                         uint64_t len);
  /** Write data from memory region to the buffer, referencing only.
   *
   * buf: the start of the source memory region.
   *
   * len: the amount of data to write in bytes.
   *
   * Data will be appended to the end of the buffer.
   *
   * The return value is the amount of data successfully written to the
   * buffer.
   */
  uint64_t (*write_data_ref)(struct pb_buffer * const buffer,
                             const void *buf,
                             uint64_t len);
  /** Write data from a source buffer to the buffer.
   *
   * src_buffer: the buffer to write from.  This pb_buffer instance will not
   *             have its data modified.
   *
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


  /** Overwrite the head of a buffer with data from a memory region.
   *
   * buf: the start of the source memory region.
   *
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
                             const void *buf,
                             uint64_t len);
  /** Overwrite the head of a buffer with data from another buffer.
   *
   * src_buffer: the buffer to write from.  This pb_buffer instance will not
   *             have its data modified.
   *
   * len: the amount of data to write in bytes.
   *
   * Data is written to the head of the buffer, overwriting existing data.
   * No new storage will be allocated if len is greater than the size of the
   * buffer.
   *
   * The return value is the amount of data successfully written to the
   * buffer.
   */
  uint64_t (*overwrite_buffer)(struct pb_buffer * const buffer,
                               struct pb_buffer * const src_buffer,
                               uint64_t len);


  /** Read data from the head of a buffer to a memory region.
   *
   * buf: the start of the target memory region.
   *
   * len: the amount of data to read in bytes.
   *
   * Data is read from the head of the buffer.  The amount of data read is the
   * lower of the size of the buffer and the value of len.
   *
   * The return value is the amount of data successfully read from the buffer.
   */
  uint64_t (*read_data)(struct pb_buffer * const buffer,
                        void * const buf,
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
 * These functions are public and are intended to be called by end users.
 */
uint64_t pb_buffer_get_data_revision(struct pb_buffer * const buffer);

uint64_t pb_buffer_get_data_size(struct pb_buffer * const buffer);


void pb_buffer_get_iterator(struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_buffer_get_end_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
bool pb_buffer_is_end_iterator(
                            struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *buffer_iterator);
bool pb_buffer_cmp_iterator(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *lvalue,
                            const struct pb_buffer_iterator *rvalue);
void pb_buffer_next_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
void pb_buffer_prev_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);


void pb_buffer_get_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_buffer_get_end_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_buffer_is_end_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
bool pb_buffer_cmp_byte_iterator(
                         struct pb_buffer * const buffer,
                         const struct pb_buffer_byte_iterator *lvalue,
                         const struct pb_buffer_byte_iterator *rvalue);
void pb_buffer_next_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
void pb_buffer_prev_byte_iterator(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);


uint64_t pb_buffer_insert(
                        struct pb_buffer * const buffer,
                        const struct pb_buffer_iterator *buffer_iterator,
                        size_t offset,
                        struct pb_page * const page);
uint64_t pb_buffer_extend(
                        struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_reserve(
                        struct pb_buffer * const buffer, uint64_t size);
uint64_t pb_buffer_rewind(
                        struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_seek(struct pb_buffer * const buffer, uint64_t len);
uint64_t pb_buffer_trim(struct pb_buffer * const buffer, uint64_t len);


uint64_t pb_buffer_insert_data(struct pb_buffer * const buffer,
                               const struct pb_buffer_iterator *buffer_iterator,
                               size_t offset,
                               const void *buf,
                               uint64_t len);
uint64_t pb_buffer_insert_data_ref(
                               struct pb_buffer * const buffer,
                               const struct pb_buffer_iterator *buffer_iterator,
                               size_t offset,
                               const void *buf,
                               uint64_t len);
uint64_t pb_buffer_insert_buffer(
                               struct pb_buffer * const buffer,
                               const struct pb_buffer_iterator *buffer_iterator,
                               size_t offset,
                               struct pb_buffer * const src_buffer,
                               uint64_t len);


uint64_t pb_buffer_write_data(struct pb_buffer * const buffer,
                              const void *buf,
                              uint64_t len);
uint64_t pb_buffer_write_data_ref(
                              struct pb_buffer * const buffer,
                              const void *buf,
                              uint64_t len);
uint64_t pb_buffer_write_buffer(
                              struct pb_buffer * const buffer,
                              struct pb_buffer * const src_buffer,
                              uint64_t len);


uint64_t pb_buffer_overwrite_data(
                              struct pb_buffer * const buffer,
                              const void *buf,
                              uint64_t len);
uint64_t pb_buffer_overwrite_buffer(
                              struct pb_buffer * const buffer,
                              struct pb_buffer * const src_buffer,
                              uint64_t len);


uint64_t pb_buffer_read_data(struct pb_buffer * const buffer,
                             void * const buf,
                             uint64_t len);


void pb_buffer_clear(struct pb_buffer * const buffer);
void pb_buffer_destroy(
                     struct pb_buffer * const buffer);






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
 */
const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void);



/** Get a trivial buffer operations structure.
 *
 * This is a protected function and should not be called externally.
 */
const struct pb_buffer_operations *pb_get_trivial_buffer_operations(void);



/** Factory functions producing trivial pb_trivial_buffer instances.
 *
 * Users may use either use the default trivial strategy and/or trivial
 * allocator, or supply their own.
 *
 * The instances are returned as a pointer to the embedded pb_buffer struct.
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






/* Pre-declare operations. */
struct pb_data_reader_operations;



/** An interface for reading data from a buffer.
 *
 * The data reader attaches to a pb_buffer instance and provides an interface
 * for performing sequential reads from that buffer.  The data reader keeps
 * track of its last read position in the buffer as it completes a read and
 * allows the user to continue from that same point in the next read.
 *
 * When a data reader is initialised, or reset, it will be set back to the
 * beginning of the buffer that it observes.
 */
struct pb_data_reader {
  /** The structure storing the operations of the data reader. */
  const struct pb_data_reader_operations *operations;

  /** The buffer instance that is being operated on by the data reader. */
  struct pb_buffer *buffer;
};



/** The structure that holds the operations that implement pb_data_reader
 *  functionality.
 */
struct pb_data_reader_operations {
  /** Read data from the pb_buffer instance, into a memory region.
   *
   * buf: the start of the target memory region.
   *
   * len: the amount of data to read in bytes.
   *
   * Data is read from the head of the buffer.  The amount of data read is the
   * lower of the size of the buffer and the value of len.
   *
   * Following a data read, the data reader will retain the position of the
   * end of the read, thus, a subsequent call to read will continue from
   * where the last read finished.
   *
   * However, in the meantime, if the buffer undergoes an operation that alters
   * its data revision, the subsequent call to read on the data reader will
   * read from the beginning of the buffer.
   *
   * The return value is the amount of data successfully read from the buffer.
   */
  uint64_t (*read)(struct pb_data_reader * const data_reader,
                   void * const buf, uint64_t len);

  /** Consume data from the pb_buffer instance, into a memory region.
   *
   * buf: the start of the target memory region.
   *
   * len: the amount of data to consume in bytes.
   *
   * Data is consumed from the head of the buffer.  The amount of data
   * consumed is the lower of the size of the buffer and the value of len.
   *
   * Following a data read, the underlying buffer will be seeked to the
   * position of the end of the read, thus, consuming the data in the buffer.
   * A subsequent call to read or consume will continue from where the last
   * consume finished.
   *
   * The return value is the amount of data successfully consumed from the
   * buffer.
   */
  uint64_t (*consume)(
                  struct pb_data_reader * const data_reader,
                  void * const buf, uint64_t len);

  /** Clone the state of the data reader into a new instance. */
  struct pb_data_reader *(*clone)(struct pb_data_reader * const data_reader);

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
                             void * const buf, uint64_t len);
uint64_t pb_data_reader_consume(
                             struct pb_data_reader * const data_reader,
                             void * const buf, uint64_t len);

struct pb_data_reader *pb_data_reader_clone(
                             struct pb_data_reader * const data_reader);

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
                                     void * const buf, uint64_t len);
uint64_t pb_trivial_data_reader_consume(
                                     struct pb_data_reader * const data_reader,
                                     void * const buf, uint64_t len);

struct pb_data_reader *pb_trivial_data_reader_clone(
                             struct pb_data_reader * const data_reader);

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
 * wriitten to the end of the buffer however, modifications to the buffer that
 * cause the data revision to be updated will invalidate the line search and
 * require the line reader to re-start at the begining of the buffer.
 */
struct pb_line_reader {
  const struct pb_line_reader_operations *operations;

  struct pb_buffer *buffer;
};



/** The maximum size of lines supported by pb_line_reader will be limited to
 *  this value. Any line discovery that reaches this position value during a
 *  search will set an artificial newline at this point.
 */
#define PB_LINE_READER_MAX_LINE_SIZE                      16777216L



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
   * If no line was discovered or has_line was never called or has_line was
   * called but returned false, this function will return zero.
   */
  size_t (*get_line_len)(struct pb_line_reader * const line_reader);
  /** Read data from the discovered line into a memory region.
   *
   * buf: the start of the target memory region.
   *
   * len: the amount of data to read in bytes.
   *
   * Data is read from the head of the buffer.  The amount of data read is the
   * lower of the length of the line and the value of len.
   */
  size_t (*get_line_data)(struct pb_line_reader * const line_reader,
                          void * const buf, uint64_t len);

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
                                   void * const buf, uint64_t len);

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
                                    void * const buf, uint64_t len);

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
