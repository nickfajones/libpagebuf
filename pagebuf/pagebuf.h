/*******************************************************************************
 *  Copyright 2013 Nick Jones <nick.fa.jones@gmail.com>
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



/** Responsibility that the pb_data instance has over the memory region.
 *  Either owned, thus freed when use count reaches zero,
 *  or merely referenced.
 */
enum pb_data_responsibility {
  pb_data_owned,
  pb_data_referenced,
};

/**
 * Reference counted structure that represents a memory region.
 *
 * Instances of this object are created using the create routines below, but
 * it should rarely be explicitly destroyed.  Instead the get and put
 * functions should be used when accepting or releasing an instance.
 * When a call to put finds a zero use count it will internally call destroy.
 */
struct pb_data {
  /** The start of the region. */
  void *base;
  /** The length of the data region in octets. */
  uint16_t len;

  /** Use count, maintained with atomic operations. */
  uint16_t use_count;

  /** Responsibility that this structure has over the memory region. */
  enum pb_data_responsibility responsibility;

  /**
   * pb_data representing the root of a large memory region that is at
   * the head of this page of data.
   */
  struct pb_data *root;
};

/**
 * Create a pb_data instance.
 *
 * Memory region buf is now owned by the pb_data instance and will be freed
 * when the instance is destroyed.
 *
 * Use count is initialised to one, therefore returned data instance must
 * be treated with the pb_data_put function when the creator is finished with
 * it, whether it is passed to another owner or not.
 */
struct pb_data *pb_data_create(void *buf, uint16_t len);
/**
 * Create a pb_data instance.
 *
 * Memory region buf is not owned by the pb_data instance.
 *
 * Use count is initialised to one, therefore returned data instance must
 * be treated with the pb_data_put function when the creator is finished with
 * it, whether it is passed to another owner or not.
 */
struct pb_data *pb_data_create_buf_ref(const void *buf, uint16_t len);
/**
 * Create a pb_data instance.
 *
 * Memory region is owned by root_data, so a reference is held by this new
 * pb_data instance to the root.  Root should be created using pb_data_create
 * and since it is a reference only, the root may have zero length;
 *
 * Use count is initialised to one, therefore returned data instance must
 * be treated with the pb_data_put function when the creator is finished with
 * it, whether it is passed to another owner or not.
 */
struct pb_data *pb_data_create_data_ref(
  struct pb_data *root_data, uint64_t off, uint16_t len);
/**
 * Clone a pb_data instance.
 *
 * A new memory region is allocated and the source memory region is copied
 * byte by byte.
 */
struct pb_data *pb_data_clone(const struct pb_data *src_data);
/**
 * Destroy a pb_data instance.
 *
 * Not to be called directly unless the instance was never accepted by a
 * container.
 * Memory region will be freed if it is owned by the instance.
 * pb_data instance will be cleared and freed.
 */
void pb_data_destroy(struct pb_data *data);
/**
 * Increment the use count of the pb_data instance, thus accepting it.
 */
void pb_data_get(struct pb_data *data);
/**
 * Decrement the use count of the pb_data instance, thus releasing it.
 * Will destroy the instance if the use count has reached zero.
 */
void pb_data_put(struct pb_data *data);



/**
 * Reference and usage of a pb_data instance.
 *
 * Embodies the usage and consumption of the memory region referenced by the
 * pb_data instance by means of a unique data pointer and length.
 * Multiple pb_page instances may refer to one pb_data instance.
 */
struct pb_page {
  /** The offset into the referenced region. */
  void *base;
  /** The remaining length of the referenced in octets. */
  uint16_t len;

  /** The reference to the pb_data instance, will raise the use count. */
  struct pb_data *data;

  /** Previous page in a list structure. */
  struct pb_page *prev;
  /** Next page in a list structure */
  struct pb_page *next;
};

/**
 * Create a pb_page instance.
 *
 * pb_data instance has its use count incremented.
 */
struct pb_page *pb_page_create(struct pb_data *data);
/**
 * Copy a pb_page instance.
 *
 * Duplicate the base and len values of the source page, and reference its
 * pb_data instance.
 */
struct pb_page *pb_page_copy(const struct pb_page *src_page);
/**
 * Clone an existing pb_page instance
 *
 * Duplicate the base and len values as well as a full clone of the referenced
 * pb_data instance.
 */
struct pb_page *pb_page_clone(const struct pb_page *src_page);
/**
 * Destroy the pb_page instance.
 *
 * pb_data instance will be released.
 * pb_page instance will be cleared and freed.
 */
void pb_page_destroy(struct pb_page *page);



/**
 * The callbacks that a pb_list instance can use to allocate and free memory.
 *
 * The value ops passed the the various functions is the ops instance itself,
 * allowing a user to use the ops instance as a token for their specific
 * allocate and free implementation.
 */
struct pb_list_mem_ops {
  void *(*alloc_fn)(struct pb_list_mem_ops *ops, size_t size);
  void  (*free_fn)(struct pb_list_mem_ops *ops, void *ptr);
};



/**
 * List of pb_page structures that represent scattered memory regions for
 * reading and writing
 *
 * The pages allocated for or assigned to this list are considered in order
 * within the list, but may not be strictly in order in the process name space.
 */
struct pb_page_list {
  struct pb_page *head;
  struct pb_page *tail;

  struct pb_list_mem_ops *mem_ops;
};

/**
 * Clear all of the pages in a pb_page_list instance.
 */
void pb_page_list_clear(struct pb_page_list *list);

/**
 * Get the combined size of all non consumed data in a page list
 */
uint64_t pb_page_list_get_size(const struct pb_page_list *list);

/**
 * Internal functions that add data to pb_page_list structures.
 *
 * The pb_data instance will be
 */
bool pb_page_list_prepend_data(
  struct pb_page_list *list, struct pb_data *data);
bool pb_page_list_append_data(
  struct pb_page_list *list, struct pb_data *data);

void pb_page_list_prepend_page(
  struct pb_page_list *list, struct pb_page *page);
void pb_page_list_append_page(
  struct pb_page_list *list, struct pb_page *page);

bool pb_page_list_append_page_copy(
  struct pb_page_list *list, const struct pb_page *page);
bool pb_page_list_append_page_clone(
  struct pb_page_list *list, const struct pb_page *page);

/**
 * Expand a list, adding data and pages containing len bytes
 */
uint64_t pb_page_list_reserve(
  struct pb_page_list *list, uint64_t len, uint16_t max_page_len);

/**
 * Internal functions that write data to a pb_page_list from various sources
 */
uint64_t pb_page_list_write_data(
  struct pb_page_list *list, const void *buf, uint64_t len,
  uint16_t max_page_len);
uint64_t pb_page_list_write_data_ref(
  struct pb_page_list *list, const void *buf, uint64_t len);
uint64_t pb_page_list_write_page_list(
  struct pb_page_list *list, const struct pb_page_list *src_list,
  uint64_t len);

/**
 * Internal functions that manipulate the starting and ending points of a list
 */
uint64_t pb_page_list_push_page_list(
  struct pb_page_list *list, struct pb_page_list *src_list,
  uint64_t len);
uint64_t pb_page_list_seek(struct pb_page_list *list, uint64_t len);
uint64_t pb_page_list_trim(struct pb_page_list *list, uint64_t len);
uint64_t pb_page_list_rewind(struct pb_page_list *list, uint64_t len);

/**
 * Internal function that reads data from a list.
 */
uint64_t pb_page_list_read_data(
  struct pb_page_list *list, void *buf, uint64_t len);

/**
 * Internal function that duplicates part of a page list into another.
 */
bool pb_page_list_dup(
  struct pb_page_list *list, const struct pb_page_list* src_list,
  uint64_t off, uint64_t len);

/**
 * Internal function that inserts one list into another.
 */
uint64_t pb_page_list_insert_page_list(
  struct pb_page_list *list, uint64_t off,
  struct pb_page_list *src_list, uint64_t src_off,
  uint64_t len);



/**
 * A structure representing a data region for external consumption.
 */
struct pb_vec {
  void *base;
  size_t len;
};

/**
 * A structure used to iterate over buffer data.
 */
struct pb_iterator {
  struct pb_vec vec;
  struct pb_page *page;

  bool is_reverse;
};

/**
 * Initialise a pb_iterator given a pb_buffer instance.
 *
 * Iterator instance should be created by the caller.  As a stack variable
 * is the recommended method
 */
void pb_iterator_init(
  const struct pb_page_list *buffer, struct pb_iterator *iterator,
  bool is_reverse);
/**
 * Is the iterator a reverse iterator?
 */
bool pb_iterator_is_reverse(const struct pb_iterator *iterator);
/**
 * Does the iterator reference a valid page?
 */
bool pb_iterator_is_valid(const struct pb_iterator *iterator);
/**
 * Advance the iterator to the next element.
 */
void pb_iterator_next(struct pb_iterator *iterator);
/**
 * Get the pb_vec of the current iterator location.
 */
const struct pb_vec *pb_iterator_get_vec(struct pb_iterator *iterator);



/**
 * When a pb_buffer is asked to reserve write space, the requested size of
 * the reservation will be split into pages of this size by default.
 */
#define PB_BUFFER_DEFAULT_PAGE_SIZE                       1024


/**
 * A structure representing data and operations and information available
 * for it
 *
 * The operations include:
 * - Writing and reading, from and to various sources and destinations.
 * - Manipulation of the data including seeking, trimming, substring
 *   isolation and insertion.
 */
struct pb_buffer {
  /**
   * List of pages that are pre-allocated for writing
   */
  struct pb_page_list write_list;
  /**
   * List of pages that represent the buffer data.  These pages are either
   * pushed in from the write list, or directly created using the write
   * interfaces.
   */
  struct pb_page_list data_list;
  /**
   * List of pages that were previously seeked out of the data list.
   * The amount retained depends on the configuration of the buffer.
   * Pages from this list will be re-used if the buffer is rewound.
   */
  struct pb_page_list retain_list;

  /**
   * Last recorded data size of a non-dirty buffer instance.
   */
  uint64_t data_size;

  /**
   * Amount of data to retain after seeking.
   */
  uint64_t retain_max_size;
  /**
   * Maximum length of pages allocated when reserving pages for writing.
   */
  uint16_t reserve_max_page_len;

  /**
   * Has the data list been altered since its size was last measured?
   */
  bool is_data_dirty;
};

/**
 * Create a pb_buffer instance.
 */
struct pb_buffer *pb_buffer_create(void);
/**
 * Create a pb_buffer instance, providing specific list mem ops.
 */
struct pb_buffer *pb_buffer_create_ops(struct pb_list_mem_ops *ops);
/**
 * Destroy a pb_buffer instance.
 */
void pb_buffer_destroy(struct pb_buffer *buffer);

/**
 * Clean up write and retain lists but leave data in the pb_buffer unchanged.
 */
void pb_buffer_optimise(struct pb_buffer *buffer);
/**
 * Clear all data in the pb_buffer instance.
 */
void pb_buffer_clear(struct pb_buffer *buffer);

/**
 * Get the size of the data in the pb_buffer instance that is const.
 */
uint64_t pb_buffer_get_data_size_ro(const struct pb_buffer *buffer);
/**
 * Get the size of the data in the pb_buffer instance, caching the result.
 */
uint64_t pb_buffer_get_data_size(struct pb_buffer *buffer);
/**
 * Test whether a const pb_buffer instance is empty.
 */
bool pb_buffer_is_empty_ro(const struct pb_buffer *buffer);
/**
 * Test whether a pb_buffer instance is empty, caching the result.
 */
bool pb_buffer_is_empty(struct pb_buffer *buffer);


/**
 * Reserve writing capacity in a pb_buffer instance.
 */
uint64_t pb_buffer_reserve(struct pb_buffer *buffer, uint64_t len);
/**
 * Push reserved and written pages into the data list of a pb_buffer instance.
 */
uint64_t pb_buffer_push(struct pb_buffer *buffer, uint64_t len);


/**
 * Write a memory region to the pb_buffer instance by allocating and copying.
 */
uint64_t pb_buffer_write_data(
  struct pb_buffer *buffer, const void *buf, uint64_t len);
/**
 * Reference a memory region in a pb_buffer instance.
 */
uint64_t pb_buffer_write_data_ref(
  struct pb_buffer *buffer, const void *buf, uint64_t len);
/**
 * Write len bytes of src_buffer to a pb_buffer instance.
 */
uint64_t pb_buffer_write_buf(
  struct pb_buffer *buffer, const struct pb_buffer *src_buffer, uint64_t len);

/**
 * Get iterator to the pb_buffer write list.
 */
void pb_buffer_get_write_iterator(
  struct pb_buffer *buffer, struct pb_iterator *iterator);

/**
 * Seek len bytes into data list of a pb_buffer instance.
 */
uint64_t pb_buffer_seek(struct pb_buffer *buffer, uint64_t len);
/**
 * Trim len bytes from the end of a pb_buffer instance.
 */
uint64_t pb_buffer_trim(struct pb_buffer *buffer, uint64_t len);
/**
 * Rewind a pb_buffer instance by len bytes
 */
uint64_t pb_buffer_rewind(struct pb_buffer *buffer, uint64_t len);

/**
 * Read len bytes from a pb_buffer instance, into a memory region
 */
uint64_t pb_buffer_read_data(
  struct pb_buffer *buffer, void *buf, uint64_t len);

/**
 * Get iterator to the pb_buffer data list.
 */
void pb_buffer_get_data_iterator(
  struct pb_buffer *buffer, struct pb_iterator *iterator);

/**
 * Duplicate
 */
struct pb_buffer *pb_buffer_dup(struct pb_buffer *src_buffer);
struct pb_buffer *pb_buffer_dup_seek(
  struct pb_buffer *src_buffer, uint64_t off);
struct pb_buffer *pb_buffer_dup_trim(
  struct pb_buffer *src_buffer, uint64_t len);
struct pb_buffer *pb_buffer_dup_sub(
  struct pb_buffer *src_buffer, uint64_t off, uint64_t len);

/**
 * Insert
 */
uint64_t pb_buffer_insert_buf(
  struct pb_buffer *buffer, uint64_t off,
  struct pb_buffer *src_buffer, uint64_t src_off,
  uint64_t len);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif /* PAGEBUF_H */
