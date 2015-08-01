/*******************************************************************************
 *  Copyright 2015 Nick Jones <nick.fa.jones@gmail.com>
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

#include "pagebuf_mmap.h"

#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>


/** Pre declare mmap_data.
 */
struct pb_mmap_data;



/*******************************************************************************
 */
struct pb_mmap_allocator {
  struct pb_allocator allocator;

  const struct pb_allocator *struct_allocator;

  char *file_path;

  enum pb_mmap_open_action open_action;
  enum pb_mmap_close_action close_action;

  int file_fd;

  off64_t head_offset;
  off64_t tail_offset;

  struct pb_mmap_data *data_tree;

  /** Use count, maintained with atomic operations. */
  uint16_t use_count;
};



/*******************************************************************************
 */
static void *pb_mmap_allocator_alloc(const struct pb_allocator *allocator,
    enum pb_allocator_type type, size_t size) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)allocator;

  if (type == pb_alloc_type_struct)
    return mmap_allocator->struct_allocator->alloc(allocator, type, size);

  return NULL;
}

static void pb_mmap_allocator_free(const struct pb_allocator *allocator,
    enum pb_allocator_type type, void *obj, size_t size) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)allocator;

  if (type == pb_alloc_type_struct) {
    pb_allocator_free(mmap_allocator->struct_allocator, type, obj, size);

    return;
  }

}


/*******************************************************************************
 */
static struct pb_mmap_allocator *pb_mmap_allocator_create(const char *file_path,
    enum pb_mmap_open_action open_action,
    enum pb_mmap_close_action close_action,
    const struct pb_allocator *allocator) {
  struct pb_mmap_allocator *mmap_allocator =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_mmap_allocator));
  if (!mmap_allocator)
    return NULL;

  mmap_allocator->allocator.alloc = &pb_mmap_allocator_alloc;
  mmap_allocator->allocator.free = &pb_mmap_allocator_free;

  mmap_allocator->struct_allocator = allocator;

  mmap_allocator->file_path = strdup(file_path);
  if (!mmap_allocator->file_path) {
    int temp_errno = errno;

    pb_allocator_free(
      allocator,
      pb_alloc_type_struct, mmap_allocator, sizeof(struct pb_mmap_allocator));

    errno = temp_errno;

    return NULL;
  }

  mmap_allocator->open_action = open_action;
  mmap_allocator->close_action = close_action;

  return mmap_allocator;
}

/*******************************************************************************
 */
static void pb_mmap_allocator_get(struct pb_mmap_allocator *mmap_allocator) {
  __sync_add_and_fetch(&mmap_allocator->use_count, 1);
}

static void pb_mmap_allocator_put(struct pb_mmap_allocator *mmap_allocator) {
  const struct pb_allocator *struct_allocator =
    mmap_allocator->struct_allocator;

  if (__sync_sub_and_fetch(&mmap_allocator->use_count, 1) != 0)
    return;

  pb_allocator_free(
    struct_allocator,
    pb_alloc_type_struct, mmap_allocator, sizeof(struct pb_mmap_allocator));
}






/*******************************************************************************
 */
static void pb_mmap_data_get(struct pb_data * const data);
static void pb_mmap_data_put(struct pb_data * const data);



/*******************************************************************************
 */
static struct pb_data_operations pb_mmap_data_operations = {
  .get = &pb_mmap_data_get,
  .put = &pb_mmap_data_put,
};

static const struct pb_data_operations *pb_get_mmap_data_operations(void) {
  return &pb_mmap_data_operations;
}



/*******************************************************************************
 */
struct pb_mmap_data {
  struct pb_data data;

  struct pb_mmap_allocator *mmap_allocator;

  off64_t file_offset;
};



/*******************************************************************************
 */
static struct pb_mmap_data *pb_mmap_data_create(
    size_t len,
    struct pb_mmap_allocator *mmap_allocator) {
  struct pb_mmap_data *mmap_data =
    pb_allocator_alloc(
      &mmap_allocator->allocator, pb_alloc_type_struct,
      sizeof(struct pb_mmap_data));
  if (!mmap_data)
    return NULL;

  uint64_t head_offset = mmap_allocator->head_offset;

  // reserve mmap file data

  mmap_data->data.operations = pb_get_mmap_data_operations();
  mmap_data->data.allocator = &mmap_allocator->allocator;

  mmap_data->mmap_allocator = mmap_allocator;

  mmap_data->file_offset = head_offset;

  return NULL;
}

static struct pb_mmap_data *pb_mmap_data_create_ref(
    const uint8_t *buf, size_t len,
    struct pb_mmap_allocator *mmap_allocator) {
  struct pb_mmap_data *mmap_data = pb_mmap_data_create(len, mmap_allocator);

  memcpy(mmap_data->data.data_vec.base, buf, mmap_data->data.data_vec.len);

  return NULL;
}

/*******************************************************************************
 */
static void pb_mmap_data_get(struct pb_data * const data) {
  __sync_add_and_fetch(&data->use_count, 1);
}

static void pb_mmap_data_put(struct pb_data * const data) {
  struct pb_mmap_data *mmap_data = (struct pb_mmap_data*)data;
  struct pb_mmap_allocator *mmap_allocator = mmap_data->mmap_allocator;

  if (__sync_sub_and_fetch(&data->use_count, 1) != 1)
    return;



  pb_mmap_allocator_put(mmap_allocator);
}




#if 0
/** Strategy for the mmap buffer. */
static struct pb_buffer_strategy pb_mmap_buffer_strategy = {
  .page_size = 4096,
  .clone_on_write = true,
  .fragment_as_target = true,
  .rejects_insert = true,
};

static const struct pb_buffer_strategy *pb_get_mmap_buffer_strategy(void) {
  return &pb_mmap_buffer_strategy;
}
#endif



/** Operations function overrides for mmap buffer. */
static uint64_t pb_mmap_buffer_get_data_size(struct pb_buffer * const buffer);


static struct pb_data *pb_mmap_buffer_data_create(
                                              struct pb_buffer * const buffer,
                                              size_t len);

static struct pb_data *pb_mmap_buffer_data_create_ref(
                                              struct pb_buffer * const buffer,
                                              const uint8_t *buf, size_t len);


/*\
static uint64_t pb_mmap_buffer_insert(
                             struct pb_buffer * const buffer,
                             struct pb_page * const page,
                             struct pb_buffer_iterator * const buffer_iterator,
                             size_t offset);
static uint64_t pb_mmap_buffer_seek(struct pb_buffer * const buffer,
                             uint64_t len);
static uint64_t pb_mmap_buffer_reserve(
                             struct pb_buffer * const buffer,
                             uint64_t len);
static uint64_t pb_mmap_buffer_rewind(
                             struct pb_buffer * const buffer,
                             uint64_t len);


void pb_mmap_buffer_byte_iterator_next(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
static void pb_mmap_buffer_byte_iterator_prev(
                         struct pb_buffer * const buffer,
                         struct pb_buffer_byte_iterator * const byte_iterator);
 */


static void pb_mmap_buffer_clear(struct pb_buffer * const buffer);
static void pb_mmap_buffer_destroy(
                                 struct pb_buffer * const buffer);



/*******************************************************************************
 */
static struct pb_buffer_operations pb_mmap_buffer_operations = {
  .get_data_revision = &pb_trivial_buffer_get_data_revision,

  .get_data_size = &pb_mmap_buffer_get_data_size,

  .data_create = &pb_mmap_buffer_data_create,
  .data_create_ref = &pb_mmap_buffer_data_create_ref,

  .insert = &pb_trivial_buffer_insert,
  .seek = &pb_trivial_buffer_seek,
  .reserve = &pb_trivial_buffer_reserve,
  .rewind = &pb_trivial_buffer_rewind,

  .get_iterator = &pb_trivial_buffer_get_iterator,
  .get_iterator_end = &pb_trivial_buffer_get_iterator_end,
  .iterator_is_end = &pb_trivial_buffer_iterator_is_end,
  .iterator_cmp = &pb_trivial_buffer_iterator_cmp,
  .iterator_next = &pb_trivial_buffer_iterator_next,
  .iterator_prev = &pb_trivial_buffer_iterator_prev,

  .get_byte_iterator = &pb_trivial_buffer_get_byte_iterator,
  .get_byte_iterator_end = &pb_trivial_buffer_get_byte_iterator_end,
  .byte_iterator_is_end = &pb_trivial_buffer_byte_iterator_is_end,
  .byte_iterator_cmp = &pb_trivial_buffer_byte_iterator_cmp,
  .byte_iterator_next = &pb_trivial_buffer_byte_iterator_next,
  .byte_iterator_prev = &pb_trivial_buffer_byte_iterator_prev,

  .write_data = &pb_trivial_buffer_write_data,
  .write_data_ref = &pb_trivial_buffer_write_data_ref,
  .write_buffer = &pb_trivial_buffer_write_buffer,

  .overwrite_data = &pb_trivial_buffer_overwrite_data,

  .read_data = &pb_trivial_buffer_read_data,

  .clear = &pb_mmap_buffer_clear,
  .destroy = &pb_mmap_buffer_destroy,
};

static const struct pb_buffer_operations *pb_get_mmap_buffer_operations(void) {
  return &pb_mmap_buffer_operations;
}



/*******************************************************************************
 */
struct pb_mmap_buffer {
  struct pb_trivial_buffer trivial_buffer;
};



/*******************************************************************************
 */
struct pb_buffer *pb_mmap_buffer_create(const char *file_path,
    enum pb_mmap_open_action open_action,
    enum pb_mmap_close_action close_action) {
  return
    pb_mmap_buffer_create_with_alloc(
      file_path, open_action, close_action,
      pb_get_trivial_allocator());
}

struct pb_buffer *pb_mmap_buffer_create_with_alloc(const char *file_path,
    enum pb_mmap_open_action open_action,
    enum pb_mmap_close_action close_action,
    const struct pb_allocator *allocator) {

  struct pb_mmap_buffer *mmap_buffer =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_mmap_buffer));
  if (!mmap_buffer)
    return NULL;

  mmap_buffer->trivial_buffer.buffer.operations =
    pb_get_mmap_buffer_operations();

  struct pb_mmap_allocator *mmap_allocator =
    pb_mmap_allocator_create(file_path, open_action, close_action, allocator);
  if (!mmap_allocator) {
    pb_buffer_destroy(&mmap_buffer->trivial_buffer.buffer);

    errno = ENOMEM;

    return NULL;
  }

  pb_mmap_allocator_get(mmap_allocator);

  return &mmap_buffer->trivial_buffer.buffer;
}



/*******************************************************************************
 */
static uint64_t pb_mmap_buffer_get_data_size(struct pb_buffer * const buffer) {

  return 0;
}

/*******************************************************************************
 */
static struct pb_data *pb_mmap_buffer_data_create(struct pb_buffer * const buffer,
    size_t len) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)&buffer->allocator;

  struct pb_mmap_data *mmap_data = pb_mmap_data_create(len, mmap_allocator);
  if (!mmap_data)
    return NULL;

  return &mmap_data->data;
}

static struct pb_data *pb_mmap_buffer_data_create_ref(
    struct pb_buffer * const buffer,
    const uint8_t *buf, size_t len) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)&buffer->allocator;

  struct pb_mmap_data *mmap_data =
    pb_mmap_data_create_ref(buf, len, mmap_allocator);
  if (!mmap_data)
    return NULL;

  return &mmap_data->data;
}

/*******************************************************************************
 */
static void pb_mmap_buffer_clear(struct pb_buffer * const buffer) {

}

static void pb_mmap_buffer_destroy(struct pb_buffer * const buffer) {

}
