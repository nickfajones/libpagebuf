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

#include <string.h>



/*******************************************************************************
 */
static struct pb_buffer_operations pb_mmap_buffer_operations = {
  .get_data_revision = &pb_trivial_buffer_get_data_revision,

  .get_data_size = &pb_mmap_buffer_get_data_size,

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

  char *file_path;
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
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_mmap_buffer));
  if (!mmap_buffer)
    return NULL;

  mmap_buffer->trivial_buffer.buffer.operations =
    pb_get_mmap_buffer_operations();

  mmap_buffer->file_path = strdup(file_path);
  if (mmap_buffer->file_path == 0) {
    pb_buffer_destroy(&mmap_buffer->trivial_buffer.buffer);

    return NULL;
  }

  return NULL;
}



/*******************************************************************************
 */
uint64_t pb_mmap_buffer_get_data_size(struct pb_buffer * const buffer) {

  return 0;
}


/*******************************************************************************
 */
void pb_mmap_buffer_clear(struct pb_buffer * const buffer) {

}

void pb_mmap_buffer_destroy(struct pb_buffer * const buffer) {

}
