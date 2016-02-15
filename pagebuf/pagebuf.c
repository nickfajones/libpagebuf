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

#include "pagebuf.h"

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>




/*******************************************************************************
 */
void *pb_allocator_alloc(const struct pb_allocator *allocator,
    enum pb_allocator_type type, size_t size) {
  return allocator->operations->alloc(allocator, type, size);
}
void pb_allocator_free(const struct pb_allocator *allocator,
    enum pb_allocator_type type, void *obj, size_t size) {
  allocator->operations->free(allocator, type, obj, size);
}

/*******************************************************************************
 */
static void *pb_trivial_alloc(const struct pb_allocator *allocator,
    enum pb_allocator_type type, size_t size) {
  void *obj = malloc(size);
  if (!obj)
    return NULL;

  if (type == pb_alloc_type_struct)
    memset(obj, 0, size);

  return obj;
}

static void pb_trivial_free(const struct pb_allocator *allocator,
    enum pb_allocator_type type, void *obj, size_t size) {
  memset(obj, 0, size);

  free(obj);
}


/*******************************************************************************
 */
static struct pb_allocator_operations pb_trivial_allocator_operations = {
  .alloc = pb_trivial_alloc,
  .free = pb_trivial_free,
};

const struct pb_allocator_operations *pb_get_trivial_allocator_operations(void) {
  return &pb_trivial_allocator_operations;
}



/*******************************************************************************
 */
static struct pb_allocator pb_trivial_allocator = {
  .operations = &pb_trivial_allocator_operations,
};

const struct pb_allocator *pb_get_trivial_allocator(void) {
  return &pb_trivial_allocator;
}






/*******************************************************************************
 */
static struct pb_data_operations pb_trivial_data_operations = {
  .get = &pb_trivial_data_get,
  .put = &pb_trivial_data_put,
};

const struct pb_data_operations *pb_get_trivial_data_operations(void) {
  return &pb_trivial_data_operations;
}


/*******************************************************************************
 */
void pb_data_get(struct pb_data *data) {
  data->operations->get(data);
}

void pb_data_put(struct pb_data *data) {
  data->operations->put(data);
}



/*******************************************************************************
 */
struct pb_data *pb_trivial_data_create(size_t len,
    const struct pb_allocator *allocator) {
  void *buf = pb_allocator_alloc(allocator, pb_alloc_type_region, len);
  if (!buf)
    return NULL;

  struct pb_data *data =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_data));
  if (!data) {
    int temp_errno = errno;

    pb_allocator_free(allocator, pb_alloc_type_region, buf, len);

    errno = temp_errno;

    return NULL;
  }

  data->data_vec.base = buf;
  data->data_vec.len = len;

  data->responsibility = pb_data_owned;

  data->use_count = 1;

  data->operations = pb_get_trivial_data_operations();
  data->allocator = allocator;

  return data;
}

struct pb_data *pb_trivial_data_create_ref(const uint8_t *buf, size_t len,
    const struct pb_allocator *allocator) {
  struct pb_data *data =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_data));
  if (!data)
    return NULL;

  data->data_vec.base = (uint8_t*)buf; // we won't actually change it
  data->data_vec.len = len;

  data->responsibility = pb_data_referenced;

  data->use_count = 1;

  data->operations = pb_get_trivial_data_operations();
  data->allocator = allocator;

  return data;
}

void pb_trivial_data_get(struct pb_data *data) {
  ++data->use_count;
}

void pb_trivial_data_put(struct pb_data *data) {
  const struct pb_allocator *allocator = data->allocator;

  if (--data->use_count != 0)
    return;

  if (data->responsibility == pb_data_owned)
    pb_allocator_free(
      allocator, pb_alloc_type_struct,
      pb_data_get_base(data), pb_data_get_len(data));

  pb_allocator_free(
    allocator, pb_alloc_type_struct, data, sizeof(struct pb_data));
}



/*******************************************************************************
 */
void *pb_data_get_base(const struct pb_data *data) {
  return data->data_vec.base;
}

void *pb_data_get_base_at(const struct pb_data *data, size_t offset) {
  return (uint8_t*)data->data_vec.base + offset;
}

size_t pb_data_get_len(const struct pb_data *data) {
  return data->data_vec.len;
}






/*******************************************************************************
 */
struct pb_page *pb_page_create(struct pb_data *data,
    const struct pb_allocator *allocator) {
  struct pb_page *page =
    pb_allocator_alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_page));
  if (!page)
    return NULL;

  page->data_vec.base = pb_data_get_base(data);
  page->data_vec.len = pb_data_get_len(data);
  page->data = data;
  page->prev = NULL;
  page->next = NULL;

  pb_data_get(data);

  return page;
}

struct pb_page *pb_page_transfer(const struct pb_page *src_page,
    size_t len, size_t src_off,
    const struct pb_allocator *allocator) {
  struct pb_page *page =
    pb_allocator_alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_page));
  if (!page)
    return NULL;

  page->data_vec.base = pb_page_get_base_at(src_page, src_off);
  page->data_vec.len = len;
  page->data = src_page->data;
  page->prev = NULL;
  page->next = NULL;

  pb_data_get(src_page->data);

  return page;
}

void pb_page_destroy(struct pb_page *page,
    const struct pb_allocator *allocator) {
  pb_data_put(page->data);

  page->data_vec.base = NULL;
  page->data_vec.len = 0;
  page->data = NULL;
  page->prev = NULL;
  page->next = NULL;

  pb_allocator_free(
    allocator, pb_alloc_type_struct, page, sizeof(struct pb_page));
}



/*******************************************************************************
 */
void *pb_page_get_base(const struct pb_page *page) {
  return page->data_vec.base;
}

void *pb_page_get_base_at(const struct pb_page *page, size_t offset) {
  return (uint8_t*)page->data_vec.base + offset;
}

size_t pb_page_get_len(const struct pb_page *page) {
  return page->data_vec.len;
}






/*******************************************************************************
 */
void *pb_buffer_iterator_get_base(
    const struct pb_buffer_iterator *buffer_iterator) {
  return buffer_iterator->page->data_vec.base;
}

void *pb_buffer_iterator_get_base_at(
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset) {
  return (uint8_t*)buffer_iterator->page->data_vec.base + offset;
}

size_t pb_buffer_iterator_get_len(
    const struct pb_buffer_iterator *buffer_iterator) {
  return buffer_iterator->page->data_vec.len;
}






/*******************************************************************************
 */
const char *pb_buffer_byte_iterator_get_current_byte(
    const struct pb_buffer_byte_iterator *buffer_byte_iterator) {
  return buffer_byte_iterator->current_byte;
}






/*******************************************************************************
 */
static struct pb_buffer_strategy pb_trivial_buffer_strategy = {
  .page_size = PB_BUFFER_DEFAULT_PAGE_SIZE,
  .clone_on_write = false,
  .fragment_as_target = false,
  .rejects_insert = false,
};

const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void) {
  return &pb_trivial_buffer_strategy;
}



/*******************************************************************************
 */
static struct pb_buffer_operations pb_trivial_buffer_operations = {
  .get_data_revision = &pb_trivial_buffer_get_data_revision,
  .increment_data_revision = &pb_trivial_buffer_increment_data_revision,

  .get_data_size = &pb_trivial_buffer_get_data_size,

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

  .insert = &pb_trivial_buffer_insert,
  .extend = &pb_trivial_buffer_extend,
  .rewind = &pb_trivial_buffer_rewind,
  .seek = &pb_trivial_buffer_seek,
  .trim = &pb_trivial_buffer_trim,

  .insert_data = &pb_trivial_buffer_insert_data,
  .insert_data_ref = &pb_trivial_buffer_insert_data_ref,
  .insert_buffer = &pb_trivial_buffer_insert_buffer,

  .write_data = &pb_trivial_buffer_write_data,
  .write_data_ref = &pb_trivial_buffer_write_data_ref,
  .write_buffer = &pb_trivial_buffer_write_buffer,

  .overwrite_data = &pb_trivial_buffer_overwrite_data,
  .overwrite_buffer = &pb_trivial_buffer_overwrite_buffer,

  .read_data = &pb_trivial_buffer_read_data,

  .clear = &pb_trivial_buffer_clear,
  .destroy = &pb_trivial_buffer_destroy,
};

const struct pb_buffer_operations *pb_get_trivial_buffer_operations(void) {
  return &pb_trivial_buffer_operations;
}



/*******************************************************************************
 */
uint64_t pb_buffer_get_data_revision(struct pb_buffer * const buffer) {
  return buffer->operations->get_data_revision(buffer);
}

/*******************************************************************************
 */
uint64_t pb_buffer_get_data_size(struct pb_buffer * const buffer) {
  return buffer->operations->get_data_size(buffer);
}

/*******************************************************************************
 */
void pb_buffer_get_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  buffer->operations->get_iterator(buffer, buffer_iterator);
}

void pb_buffer_get_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  buffer->operations->get_iterator_end(buffer, buffer_iterator);
}

bool pb_buffer_iterator_is_end(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator) {
  return buffer->operations->iterator_is_end(buffer, buffer_iterator);
}

bool pb_buffer_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *lvalue,
    const struct pb_buffer_iterator *rvalue) {
  return buffer->operations->iterator_cmp(buffer, lvalue, rvalue);
}

void pb_buffer_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  buffer->operations->iterator_next(buffer, buffer_iterator);
}

void pb_buffer_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  buffer->operations->iterator_prev(buffer, buffer_iterator);
}

/*******************************************************************************
 */
void pb_buffer_get_byte_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  buffer->operations->get_byte_iterator(buffer, byte_iterator);
}

void pb_buffer_get_byte_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  buffer->operations->get_byte_iterator_end(buffer, byte_iterator);
}

bool pb_buffer_byte_iterator_is_end(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  return buffer->operations->byte_iterator_is_end(buffer, byte_iterator);
}

bool pb_buffer_byte_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_byte_iterator *lvalue,
    const struct pb_buffer_byte_iterator *rvalue) {
  return buffer->operations->byte_iterator_cmp(buffer, lvalue, rvalue);
}

void pb_buffer_byte_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  buffer->operations->byte_iterator_next(buffer, byte_iterator);
}

void pb_buffer_byte_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  buffer->operations->byte_iterator_prev(buffer, byte_iterator);
}

/*******************************************************************************
 */
uint64_t pb_buffer_insert(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_page * const page) {
  return buffer->operations->insert(buffer, buffer_iterator, offset, page);
}

uint64_t pb_buffer_extend(struct pb_buffer * const buffer, uint64_t len) {
  return buffer->operations->extend(buffer, len);
}

uint64_t pb_buffer_rewind(struct pb_buffer * const buffer, uint64_t len) {
  return buffer->operations->rewind(buffer, len);
}

uint64_t pb_buffer_seek(struct pb_buffer * const buffer, uint64_t len) {
  return buffer->operations->seek(buffer, len);
}

uint64_t pb_buffer_trim(struct pb_buffer * const buffer, uint64_t len) {
  return buffer->operations->trim(buffer, len);
}

/*******************************************************************************
 */
uint64_t pb_buffer_insert_data(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    const void *buf,
    uint64_t len) {
  return
    buffer->operations->insert_data(buffer, buffer_iterator, offset, buf, len);
}

uint64_t pb_buffer_insert_data_ref(
    struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    const void *buf,
    uint64_t len) {
  return
    buffer->operations->insert_data_ref(
      buffer, buffer_iterator, offset, buf, len);
}

uint64_t pb_buffer_insert_buffer(
    struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  return
    buffer->operations->insert_buffer(
      buffer, buffer_iterator, offset, src_buffer, len);
}


uint64_t pb_buffer_write_data(struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  return buffer->operations->write_data(buffer, buf, len);
}

uint64_t pb_buffer_write_data_ref(struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  return buffer->operations->write_data_ref(buffer, buf, len);
}

uint64_t pb_buffer_write_buffer(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  return buffer->operations->write_buffer(buffer, src_buffer, len);
}


uint64_t pb_buffer_overwrite_data(struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  return buffer->operations->overwrite_data(buffer, buf, len);
}

uint64_t pb_buffer_overwrite_buffer(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  return buffer->operations->overwrite_buffer(buffer, src_buffer, len);
}


uint64_t pb_buffer_read_data(struct pb_buffer * const buffer,
    void * const buf,
    uint64_t len) {
  return buffer->operations->read_data(buffer, buf, len);
}

/*******************************************************************************
 */
void pb_buffer_clear(struct pb_buffer * const buffer) {
  buffer->operations->clear(buffer);
}

void pb_buffer_destroy(struct pb_buffer * const buffer) {
  buffer->operations->destroy(buffer);
}



/*******************************************************************************
 */
struct pb_buffer* pb_trivial_buffer_create(void) {
  return
    pb_trivial_buffer_create_with_strategy_with_alloc(
      pb_get_trivial_buffer_strategy(), pb_get_trivial_allocator());
}

struct pb_buffer *pb_trivial_buffer_create_with_strategy(
    const struct pb_buffer_strategy *strategy) {
  return
    pb_trivial_buffer_create_with_strategy_with_alloc(
      strategy, pb_get_trivial_allocator());
}

struct pb_buffer *pb_trivial_buffer_create_with_alloc(
    const struct pb_allocator *allocator) {
  return
    pb_trivial_buffer_create_with_strategy_with_alloc(
      pb_get_trivial_buffer_strategy(), allocator);
}

struct pb_buffer *pb_trivial_buffer_create_with_strategy_with_alloc(
    const struct pb_buffer_strategy *strategy,
    const struct pb_allocator *allocator) {
  struct pb_buffer_strategy *buffer_strategy =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_buffer_strategy));
  if (!buffer_strategy)
    return NULL;

  memcpy(buffer_strategy, strategy, sizeof(struct pb_buffer_strategy));

  struct pb_trivial_buffer *trivial_buffer =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_buffer));
  if (!trivial_buffer) {
    pb_allocator_free(
      allocator,
      pb_alloc_type_struct, buffer_strategy, sizeof(struct pb_buffer_strategy));

    return NULL;
  }

  trivial_buffer->buffer.strategy = buffer_strategy;

  trivial_buffer->buffer.operations = pb_get_trivial_buffer_operations();

  trivial_buffer->buffer.allocator = allocator;

  trivial_buffer->page_end.prev = &trivial_buffer->page_end;
  trivial_buffer->page_end.next = &trivial_buffer->page_end;

  trivial_buffer->data_revision = 0;
  trivial_buffer->data_size = 0;

  return &trivial_buffer->buffer;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_get_data_revision(struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  return trivial_buffer->data_revision;
}

void pb_trivial_buffer_increment_data_revision(
    struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  ++trivial_buffer->data_revision;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_get_data_size(struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

#ifndef NDEBUG
  // Audit the data_size figure
  uint64_t audit_size = 0;

  struct pb_buffer_iterator buffer_iterator;
  pb_trivial_buffer_get_iterator(buffer, &buffer_iterator);

  while (!pb_trivial_buffer_iterator_is_end(buffer, &buffer_iterator)) {
    audit_size += pb_buffer_iterator_get_len(&buffer_iterator);

    pb_trivial_buffer_iterator_next(buffer, &buffer_iterator);
  }

  assert(audit_size == trivial_buffer->data_size);
#endif

  return trivial_buffer->data_size;
}

void pb_trivial_buffer_increment_data_size(
  struct pb_buffer * const buffer, uint64_t size) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  trivial_buffer->data_size += size;
}

void pb_trivial_buffer_decrement_data_size(
  struct pb_buffer * const buffer, uint64_t size) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  trivial_buffer->data_size -= size;
}

/*******************************************************************************
 */
void pb_trivial_buffer_get_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  buffer_iterator->page = trivial_buffer->page_end.next;
}

void pb_trivial_buffer_get_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  buffer_iterator->page = &trivial_buffer->page_end;
}

bool pb_trivial_buffer_iterator_is_end(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  return (buffer_iterator->page == &trivial_buffer->page_end);
}

bool pb_trivial_buffer_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *lvalue,
    const struct pb_buffer_iterator *rvalue) {

  return (lvalue->page == rvalue->page);
}

void pb_trivial_buffer_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  buffer_iterator->page = buffer_iterator->page->next;
}

void pb_trivial_buffer_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  buffer_iterator->page = buffer_iterator->page->prev;
}

/*******************************************************************************
 */
static char pb_trivial_buffer_byte_iterator_null_char = '\0';

/*******************************************************************************
 *  */
void pb_trivial_buffer_get_byte_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  pb_buffer_get_iterator(buffer, &byte_iterator->buffer_iterator);

  byte_iterator->page_offset = 0;

  if (pb_buffer_iterator_is_end(buffer, &byte_iterator->buffer_iterator)) {
    byte_iterator->current_byte = &pb_trivial_buffer_byte_iterator_null_char;

    return;
  }

  byte_iterator->current_byte =
    (const char*)
      pb_buffer_iterator_get_base_at(
        &byte_iterator->buffer_iterator, byte_iterator->page_offset);
}

void pb_trivial_buffer_get_byte_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  pb_buffer_get_iterator_end(buffer, &byte_iterator->buffer_iterator);

  byte_iterator->page_offset = 0;

  byte_iterator->current_byte = &pb_trivial_buffer_byte_iterator_null_char;
}

bool pb_trivial_buffer_byte_iterator_is_end(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  return pb_buffer_iterator_is_end(buffer, &byte_iterator->buffer_iterator);
}

bool pb_trivial_buffer_byte_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_byte_iterator *lvalue,
    const struct pb_buffer_byte_iterator *rvalue) {
  if (!pb_buffer_iterator_cmp(buffer,
        &lvalue->buffer_iterator, &rvalue->buffer_iterator))
    return false;

  return (lvalue->page_offset == rvalue->page_offset);
}

void pb_trivial_buffer_byte_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  ++byte_iterator->page_offset;

  if (byte_iterator->page_offset >=
        pb_buffer_iterator_get_len(&byte_iterator->buffer_iterator)) {
    pb_buffer_iterator_next(buffer, &byte_iterator->buffer_iterator);

    byte_iterator->page_offset = 0;

    if (pb_buffer_iterator_is_end(buffer, &byte_iterator->buffer_iterator)) {
      byte_iterator->current_byte = &pb_trivial_buffer_byte_iterator_null_char;

      return;
    }
  }

  byte_iterator->current_byte =
    (const char*)
      pb_buffer_iterator_get_base_at(
        &byte_iterator->buffer_iterator, byte_iterator->page_offset);
}

void pb_trivial_buffer_byte_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_byte_iterator * const byte_iterator) {
  if (byte_iterator->page_offset == 0) {
    pb_buffer_iterator_prev(buffer, &byte_iterator->buffer_iterator);

    byte_iterator->page_offset =
      pb_buffer_iterator_get_len(&byte_iterator->buffer_iterator);

    if (pb_buffer_iterator_is_end(buffer, &byte_iterator->buffer_iterator)) {
      byte_iterator->current_byte = &pb_trivial_buffer_byte_iterator_null_char;

      return;
    }
  }

  --byte_iterator->page_offset;

  byte_iterator->current_byte =
    (const char*)
      pb_buffer_iterator_get_base_at(
        &byte_iterator->buffer_iterator, byte_iterator->page_offset);
}


/*******************************************************************************
 */
uint64_t pb_trivial_buffer_insert(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_page * const page) {
  if (!pb_buffer_iterator_is_end(buffer, buffer_iterator) &&
       buffer->strategy->rejects_insert)
    return 0;

  if (!pb_buffer_iterator_is_end(buffer, buffer_iterator) ||
      (pb_buffer_get_data_size(buffer) == 0))
    pb_trivial_buffer_increment_data_revision(buffer);

  if (offset > pb_buffer_iterator_get_len(buffer_iterator))
    offset = pb_buffer_iterator_get_len(buffer_iterator);

  struct pb_page *page1;
  struct pb_page *page2;

  if (offset != 0) {
    page2 = buffer_iterator->page;
    page1 =
      pb_page_transfer(
        page2, pb_page_get_len(page2), 0,
        buffer->allocator);
    if (!page1)
      return 0;

    page1->data_vec.len = offset;
    page1->prev = page2->prev;
    page1->next = page2;

    page2->data_vec.base += offset;
    page2->data_vec.len -= offset;
    page2->prev->next = page1;
    page2->prev = page1;
  } else {
    page1 = buffer_iterator->page->prev;
    page2 = buffer_iterator->page;
  }

  page->prev = page1;
  page->next = page2;

  page1->next = page;
  page2->prev = page;

  pb_trivial_buffer_increment_data_size(buffer, pb_page_get_len(page));

  return pb_page_get_len(page);
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_extend(struct pb_buffer * const buffer,
    uint64_t len) {
  uint64_t extended = 0;

  while (len > 0) {
    size_t extend_len =
      ((buffer->strategy->page_size != 0) &&
       (buffer->strategy->page_size < len)) ?
        buffer->strategy->page_size : len;

    struct pb_buffer_iterator buffer_iterator;
    pb_buffer_get_iterator_end(buffer, &buffer_iterator);

    struct pb_page *page =
      pb_trivial_buffer_page_create(buffer, extend_len);
    if (!page)
      return extended;

    extend_len = pb_buffer_insert(buffer, &buffer_iterator, 0, page);
    if (extend_len == 0)
      break;

    len -= extend_len;
    extended += extend_len;
  }

  return extended;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_rewind(struct pb_buffer * const buffer,
    uint64_t len) {
  uint64_t rewinded = 0;

  while (len > 0) {
    uint64_t extend_len =
      ((buffer->strategy->page_size != 0) &&
       (buffer->strategy->page_size < len)) ?
        buffer->strategy->page_size : len;

    struct pb_buffer_iterator buffer_iterator;
    pb_buffer_get_iterator(buffer, &buffer_iterator);

    struct pb_page *page =
      pb_trivial_buffer_page_create(buffer, extend_len);
    if (!page)
      return rewinded;

    extend_len = pb_buffer_insert(buffer, &buffer_iterator, 0, page);
    if (extend_len == 0)
      break;

    len -= extend_len;
    rewinded += extend_len;
  }

  return rewinded;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_seek(struct pb_buffer * const buffer, uint64_t len) {
  pb_trivial_buffer_increment_data_revision(buffer);

  uint64_t seeked = 0;

  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator(buffer, &buffer_iterator);

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &buffer_iterator))) {
    uint64_t seek_len =
      (pb_buffer_iterator_get_len(&buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&buffer_iterator) : len;

    buffer_iterator.page->data_vec.base += seek_len;
    buffer_iterator.page->data_vec.len -= seek_len;

    if (pb_buffer_iterator_get_len(&buffer_iterator) == 0) {
      struct pb_page *page = buffer_iterator.page;

      pb_buffer_iterator_next(buffer, &buffer_iterator);

      page->prev->next = buffer_iterator.page;
      buffer_iterator.page->prev = page->prev;

      page->prev = NULL;
      page->next = NULL;

      pb_page_destroy(page, buffer->allocator);
    }

    len -= seek_len;
    seeked += seek_len;

    pb_trivial_buffer_decrement_data_size(buffer, seek_len);
  }

  return seeked;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_trim(struct pb_buffer * const buffer, uint64_t len) {
  pb_trivial_buffer_increment_data_revision(buffer);

  uint64_t trimmed = 0;

  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator_end(buffer, &buffer_iterator);
  pb_buffer_iterator_prev(buffer, &buffer_iterator);

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &buffer_iterator))) {
    size_t trim_len =
      (pb_buffer_iterator_get_len(&buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&buffer_iterator) : len;

    buffer_iterator.page->data_vec.len -= trim_len;

    if (pb_buffer_iterator_get_len(&buffer_iterator) == 0) {
      struct pb_page *page = buffer_iterator.page;

      pb_buffer_iterator_prev(buffer, &buffer_iterator);

      page->next->prev = buffer_iterator.page;
      buffer_iterator.page->next = page->next;

      page->prev = NULL;
      page->next = NULL;

      pb_page_destroy(page, buffer->allocator);
    }

    len -= trim_len;
    trimmed += trim_len;

    pb_trivial_buffer_decrement_data_size(buffer, trim_len);

    pb_buffer_iterator_prev(buffer, &buffer_iterator);
  }

  return trimmed;
}


/*******************************************************************************
 */
struct pb_page *pb_trivial_buffer_page_create(
    struct pb_buffer * const buffer,
    size_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  struct pb_data *data = pb_trivial_data_create(len, allocator);
  if (!data)
    return NULL;

  struct pb_page *page = pb_page_create(data, allocator);
  if (!page) {
    pb_data_put(data);

    return NULL;
  }

  pb_data_put(data);

  return page;
}

struct pb_page *pb_trivial_buffer_page_create_ref(
    struct pb_buffer * const buffer,
    const uint8_t *buf, size_t len) {
  const struct pb_allocator *allocator = buffer->allocator;

  struct pb_data *data = pb_trivial_data_create_ref(buf, len, allocator);
  if (!data)
    return NULL;

  struct pb_page *page = pb_page_create(data, allocator);
  if (!page) {
    pb_data_put(data);

    return NULL;
  }

  pb_data_put(data);

  return page;
}


/*******************************************************************************
 */
static uint64_t pb_trivial_buffer_insert_data1(
    struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    const uint8_t *buf,
    uint64_t len) {
  uint64_t inserted = 0;

  while (len > 0) {
    uint64_t insert_len =
      ((buffer->strategy->page_size != 0) &&
       (buffer->strategy->page_size < len)) ?
        buffer->strategy->page_size : len;

    struct pb_page *page =
      pb_trivial_buffer_page_create(buffer, insert_len);
    if (!page)
      return inserted;

    memcpy(
      pb_page_get_base(page),
      buf + inserted,
      pb_page_get_len(page));

    insert_len = pb_buffer_insert(buffer, buffer_iterator, offset, page);

    offset = 0;

    len -= insert_len;
    inserted += insert_len;
  }

  return inserted;
}

uint64_t pb_trivial_buffer_insert_data(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    const void *buf,
    uint64_t len) {
  if (!pb_buffer_iterator_is_end(buffer, buffer_iterator) &&
       buffer->strategy->rejects_insert)
    return 0;

  return
    pb_trivial_buffer_insert_data1(buffer, buffer_iterator, offset, buf, len);
}

/*******************************************************************************
 */
static uint64_t pb_trivial_buffer_insert_data_ref1(
    struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    const uint8_t *buf,
    uint64_t len) {
  uint64_t inserted = 0;

  while (len > 0) {
    uint64_t insert_len =
      ((buffer->strategy->page_size != 0) &&
       (buffer->strategy->page_size < len)) ?
        buffer->strategy->page_size : len;

    struct pb_page *page =
      pb_trivial_buffer_page_create_ref(buffer, buf + inserted, insert_len);
    if (!page)
      return inserted;

    insert_len = pb_buffer_insert(buffer, buffer_iterator, offset, page);

    offset = 0;

    len -= insert_len;
    inserted += insert_len;
  }

  return inserted;
}

uint64_t pb_trivial_buffer_insert_data_ref(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    const void *buf,
    uint64_t len) {
  if (!pb_buffer_iterator_is_end(buffer, buffer_iterator) &&
       buffer->strategy->rejects_insert)
    return 0;

  return
    pb_trivial_buffer_insert_data_ref1(
      buffer, buffer_iterator, offset, buf, len);
}

/*******************************************************************************
 * clone_on_write: false
 * fragment_as_target: false
 */
static uint64_t pb_trivial_buffer_insert_buffer1(
    struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_buffer_iterator src_buffer_iterator;
  pb_buffer_get_iterator(src_buffer, &src_buffer_iterator);

  uint64_t inserted = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_buffer_iterator))) {
    uint64_t insert_len =
      (pb_buffer_iterator_get_len(&src_buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&src_buffer_iterator) : len;

    struct pb_page *page =
      pb_page_transfer(
        src_buffer_iterator.page, insert_len, 0, buffer->allocator);
    if (!page)
      return inserted;

    insert_len = pb_buffer_insert(buffer, buffer_iterator, offset, page);

    offset = 0;

    len -= insert_len;
    inserted += insert_len;

    pb_buffer_iterator_next(src_buffer, &src_buffer_iterator);
  }

  return inserted;
}

/*
 * clone_on_write: true
 * fragment_as_target: false
 */
static uint64_t pb_trivial_buffer_insert_buffer2(
    struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_buffer_iterator src_buffer_iterator;
  pb_buffer_get_iterator(src_buffer, &src_buffer_iterator);

  uint64_t inserted = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_buffer_iterator))) {
    uint64_t insert_len =
      (pb_buffer_iterator_get_len(&src_buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&src_buffer_iterator) : len;

    struct pb_page* page =
      pb_trivial_buffer_page_create(buffer, insert_len);
    if (!page)
      return inserted;

    memcpy(
      pb_page_get_base(page),
      pb_buffer_iterator_get_base_at(&src_buffer_iterator, src_offset),
      pb_page_get_len(page));

    insert_len = pb_buffer_insert(buffer, buffer_iterator, offset, page);

    offset = 0;

    len -= insert_len;
    inserted += insert_len;
    src_offset += insert_len;

    if (src_offset == pb_buffer_iterator_get_len(&src_buffer_iterator)) {
      pb_buffer_iterator_next(src_buffer, &src_buffer_iterator);

      src_offset = 0;
    }
  }

  return inserted;
}

/*
 * clone_on_write: false
 * fragment_as_target: true
 */
static uint64_t pb_trivial_buffer_insert_buffer3(
    struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_buffer_iterator src_buffer_iterator;
  pb_buffer_get_iterator(src_buffer, &src_buffer_iterator);

  uint64_t inserted = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_buffer_iterator))) {
    uint64_t insert_len =
      (pb_buffer_iterator_get_len(&src_buffer_iterator) - src_offset < len) ?
       pb_buffer_iterator_get_len(&src_buffer_iterator) - src_offset : len;

    insert_len =
      ((buffer->strategy->page_size != 0) &&
       (buffer->strategy->page_size < insert_len)) ?
        buffer->strategy->page_size : insert_len;

    struct pb_page *page =
      pb_page_transfer(
        src_buffer_iterator.page, insert_len, src_offset, buffer->allocator);
    if (!page)
      return inserted;

    insert_len = pb_buffer_insert(buffer, buffer_iterator, offset, page);

    offset = 0;

    len -= insert_len;
    inserted += insert_len;
    src_offset += insert_len;

    if (src_offset == pb_buffer_iterator_get_len(&src_buffer_iterator)) {
      pb_buffer_iterator_next(src_buffer, &src_buffer_iterator);

      src_offset = 0;
    }
  }

  return inserted;
}

/*
 * clone_on_write: true
 * fragment_as_target: true
 */
static uint64_t pb_trivial_buffer_insert_buffer4(
    struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_buffer_iterator src_buffer_iterator;
  pb_buffer_get_iterator(src_buffer, &src_buffer_iterator);

  uint64_t inserted = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_buffer_iterator))) {
    uint64_t insert_len =
      (pb_buffer_iterator_get_len(&src_buffer_iterator) - src_offset < len) ?
       pb_buffer_iterator_get_len(&src_buffer_iterator) - src_offset : len;

    insert_len =
      ((buffer->strategy->page_size != 0) &&
       (buffer->strategy->page_size < insert_len)) ?
        buffer->strategy->page_size : insert_len;

    struct pb_page *page =
      pb_trivial_buffer_page_create(buffer, insert_len);
    if (!page)
      return inserted;

    memcpy(
      pb_page_get_base(page),
      pb_buffer_iterator_get_base_at(&src_buffer_iterator, src_offset),
      pb_page_get_len(page));
    
    insert_len = pb_buffer_insert(buffer, buffer_iterator, offset, page);

    offset = 0;

    len -= insert_len;
    inserted += insert_len;
    src_offset += insert_len;

    if (src_offset == pb_buffer_iterator_get_len(&src_buffer_iterator)) {
      pb_buffer_iterator_next(src_buffer, &src_buffer_iterator);

      src_offset = 0;
    }
  }

  return inserted;
}

uint64_t pb_trivial_buffer_insert_buffer(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  if (!pb_buffer_iterator_is_end(buffer, buffer_iterator) &&
       buffer->strategy->rejects_insert)
    return 0;

  if (!buffer->strategy->clone_on_write &&
      !buffer->strategy->fragment_as_target) {
    return
      pb_trivial_buffer_insert_buffer1(
        buffer, buffer_iterator, offset, src_buffer, len);
  } else if ( buffer->strategy->clone_on_write &&
             !buffer->strategy->fragment_as_target) {
    return
      pb_trivial_buffer_insert_buffer2(
        buffer, buffer_iterator, offset, src_buffer, len);
  } else if (!buffer->strategy->clone_on_write &&
              buffer->strategy->fragment_as_target) {
    return
      pb_trivial_buffer_insert_buffer3(
        buffer, buffer_iterator, offset, src_buffer, len);
  }
  /*else if (buffer->strategy->clone_on_write &&
             buffer->strategy->fragment_as_target) { */
  return
    pb_trivial_buffer_insert_buffer4(
      buffer, buffer_iterator, offset, src_buffer, len);
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_write_data(struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator_end(buffer, &buffer_iterator);

  return pb_trivial_buffer_insert_data1(buffer, &buffer_iterator, 0, buf, len);
}

uint64_t pb_trivial_buffer_write_data_ref(struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator_end(buffer, &buffer_iterator);

  return
    pb_trivial_buffer_insert_data_ref1(buffer, &buffer_iterator, 0, buf, len);
}

uint64_t pb_trivial_buffer_write_buffer(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator_end(buffer, &buffer_iterator);

  if (!buffer->strategy->clone_on_write &&
      !buffer->strategy->fragment_as_target) {
    return
      pb_trivial_buffer_insert_buffer1(
        buffer, &buffer_iterator, 0, src_buffer, len);
  } else if ( buffer->strategy->clone_on_write &&
             !buffer->strategy->fragment_as_target) {
    return
      pb_trivial_buffer_insert_buffer2(
        buffer, &buffer_iterator, 0, src_buffer, len);
  } else if (!buffer->strategy->clone_on_write &&
              buffer->strategy->fragment_as_target) {
    return
      pb_trivial_buffer_insert_buffer3(
        buffer, &buffer_iterator, 0, src_buffer, len);
  }
  /*else if (buffer->strategy->clone_on_write &&
             buffer->strategy->fragment_as_target) { */
  return
    pb_trivial_buffer_insert_buffer4(
      buffer, &buffer_iterator, 0, src_buffer, len);
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_overwrite_data(struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator(buffer, &buffer_iterator);

  uint64_t written = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &buffer_iterator))) {
    uint64_t write_len =
      (pb_buffer_iterator_get_len(&buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&buffer_iterator) : len;

    memcpy(
      pb_buffer_iterator_get_base(&buffer_iterator),
      (uint8_t*)buf + written,
      write_len);

    len -= write_len;
    written += write_len;

    pb_buffer_iterator_next(buffer, &buffer_iterator);
  }

  return written;
}

uint64_t pb_trivial_buffer_overwrite_buffer(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator(buffer, &buffer_iterator);

  struct pb_buffer_iterator src_buffer_iterator;
  pb_buffer_get_iterator(src_buffer, &src_buffer_iterator);

  uint64_t written = 0;
  size_t offset = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &buffer_iterator)) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_buffer_iterator))) {
    uint64_t write_len =
      (pb_buffer_iterator_get_len(&buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&buffer_iterator) : len;

    write_len =
      (pb_buffer_iterator_get_len(&src_buffer_iterator) < write_len) ?
       pb_buffer_iterator_get_len(&src_buffer_iterator) : write_len;

    memcpy(
      pb_buffer_iterator_get_base_at(&buffer_iterator, offset),
      pb_buffer_iterator_get_base_at(&src_buffer_iterator, src_offset),
      write_len);

    len -= write_len;
    written += write_len;
    offset += write_len;
    src_offset += write_len;

    if (offset == pb_buffer_iterator_get_len(&buffer_iterator)) {
      pb_buffer_iterator_next(buffer, &buffer_iterator);

      offset = 0;
    }

    if (src_offset == pb_buffer_iterator_get_len(&src_buffer_iterator)) {
      pb_buffer_iterator_next(src_buffer, &src_buffer_iterator);

      src_offset = 0;
    }
  }

  return written;
}


/*******************************************************************************
 */
uint64_t pb_trivial_buffer_read_data(struct pb_buffer * const buffer,
    void * const buf,
    uint64_t len) {
  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator(buffer, &buffer_iterator);

  uint64_t readed = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &buffer_iterator))) {
    size_t read_len =
      (pb_buffer_iterator_get_len(&buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&buffer_iterator) : len;

    memcpy(
      (uint8_t*)buf + readed,
      pb_buffer_iterator_get_base(&buffer_iterator),
      read_len);

    len -= read_len;
    readed += read_len;

    pb_buffer_iterator_next(buffer, &buffer_iterator);
  }

  return readed;
}

/*******************************************************************************
 */
static void pb_trivial_buffer_clear_impl(struct pb_buffer * const buffer,
    void (*get_iterator)(struct pb_buffer * const buffer,
                         struct pb_buffer_iterator * const buffer_iterator),
    bool (*iterator_is_end)(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *buffer_iterator),
    void (*iterator_next)(struct pb_buffer * const buffer,
                          struct pb_buffer_iterator * const buffer_iterator)) {
  pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  trivial_buffer->data_size = 0;

  struct pb_buffer_iterator buffer_iterator;
  get_iterator(buffer, &buffer_iterator);

  while (!iterator_is_end(buffer, &buffer_iterator)) {
    struct pb_page *page = buffer_iterator.page;

    iterator_next(buffer, &buffer_iterator);

    page->prev->next = buffer_iterator.page;
    buffer_iterator.page->prev = page->prev;

    page->prev = NULL;
    page->next = NULL;

    pb_page_destroy(page, buffer->allocator);
  }
}

void pb_trivial_buffer_clear(struct pb_buffer * const buffer) {
  pb_trivial_buffer_clear_impl(
    buffer,
    &pb_buffer_get_iterator,
    &pb_buffer_iterator_is_end,
    &pb_buffer_iterator_next);
}

void pb_trivial_pure_buffer_clear(struct pb_buffer * const buffer) {
  pb_trivial_buffer_clear_impl(
    buffer,
    &pb_trivial_buffer_get_iterator,
    &pb_trivial_buffer_iterator_is_end,
    &pb_trivial_buffer_iterator_next);
}

/*******************************************************************************
 */
void pb_trivial_buffer_destroy(struct pb_buffer * const buffer) {
  pb_buffer_clear(buffer);

  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  struct pb_buffer_strategy *buffer_strategy =
    (struct pb_buffer_strategy*)buffer->strategy;
  const struct pb_allocator *allocator = buffer->allocator;

  pb_allocator_free(
    allocator,
    pb_alloc_type_struct, trivial_buffer, sizeof(struct pb_trivial_buffer));

  pb_allocator_free(
    allocator,
    pb_alloc_type_struct, buffer_strategy, sizeof(struct pb_buffer_strategy));
}






/*******************************************************************************
 */
struct pb_data_reader_operations pb_trivial_data_reader_operations = {
  .read = pb_trivial_data_reader_read,
  .reset = pb_trivial_data_reader_reset,
  .destroy = pb_trivial_data_reader_destroy,
};

const struct pb_data_reader_operations *pb_get_trivial_data_reader_operations(void) {
  return &pb_trivial_data_reader_operations;
}



/*******************************************************************************
 */
struct pb_trivial_data_reader {
  struct pb_data_reader data_reader;

  struct pb_buffer_iterator buffer_iterator;

  uint64_t buffer_data_revision;

  uint64_t page_offset;
};



/*******************************************************************************
 */
uint64_t pb_data_reader_read(struct pb_data_reader * const data_reader,
    void * const buf, uint64_t len) {
  return data_reader->operations->read(data_reader, buf, len);
}

void pb_data_reader_reset(struct pb_data_reader * const data_reader) {
  data_reader->operations->reset(data_reader);
}

void pb_data_reader_destroy(struct pb_data_reader * const data_reader) {
  data_reader->operations->destroy(data_reader);
}




/*******************************************************************************
 */
struct pb_data_reader *pb_trivial_data_reader_create(
    struct pb_buffer * const buffer) {
  const struct pb_allocator *allocator = buffer->allocator;

  struct pb_trivial_data_reader *trivial_data_reader =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_data_reader));
  if (!trivial_data_reader)
    return NULL;

  trivial_data_reader->data_reader.operations =
    pb_get_trivial_data_reader_operations();

  trivial_data_reader->data_reader.buffer = buffer;

  pb_data_reader_reset(&trivial_data_reader->data_reader);

  return &trivial_data_reader->data_reader;
}

/*******************************************************************************
 */
uint64_t pb_trivial_data_reader_read(
    struct pb_data_reader * const data_reader,
    void * const buf,
    uint64_t len) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  struct pb_buffer *buffer = trivial_data_reader->data_reader.buffer;
  struct pb_buffer_iterator *buffer_iterator =
    &trivial_data_reader->buffer_iterator;

  if (pb_buffer_get_data_revision(buffer) !=
      trivial_data_reader->buffer_data_revision)
    pb_data_reader_reset(&trivial_data_reader->data_reader);

  if (trivial_data_reader->page_offset ==
        pb_buffer_iterator_get_len(buffer_iterator))
    pb_buffer_iterator_next(buffer, buffer_iterator);

  uint64_t readed = 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, buffer_iterator))) {
    uint64_t read_len =
      (pb_buffer_iterator_get_len(buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(buffer_iterator) : len;

    memcpy(
      (uint8_t*)buf + trivial_data_reader->page_offset + readed,
      pb_buffer_iterator_get_base(buffer_iterator),
      read_len);

    len -= read_len;
    readed += read_len;

    trivial_data_reader->page_offset += read_len;

    if (trivial_data_reader->page_offset !=
          pb_buffer_iterator_get_len(buffer_iterator))
      return readed;

    pb_buffer_iterator_next(buffer, buffer_iterator);

    trivial_data_reader->page_offset = 0;
  }

  if (pb_buffer_iterator_is_end(buffer, buffer_iterator)) {
    pb_buffer_iterator_prev(buffer, buffer_iterator);

    trivial_data_reader->page_offset =
      pb_buffer_iterator_get_len(buffer_iterator);
  }

  return readed;
}

/*******************************************************************************
 */
void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  struct pb_buffer *buffer = trivial_data_reader->data_reader.buffer;

  pb_buffer_get_iterator(buffer, &trivial_data_reader->buffer_iterator);

  trivial_data_reader->buffer_data_revision =
    pb_buffer_get_data_revision(buffer);

  trivial_data_reader->page_offset = 0;
}

void pb_trivial_data_reader_destroy(struct pb_data_reader * const data_reader) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  const struct pb_allocator *allocator =
    trivial_data_reader->data_reader.buffer->allocator;

  pb_allocator_free(
    allocator,
    pb_alloc_type_struct,
    trivial_data_reader, sizeof(struct pb_trivial_data_reader));
}






/*******************************************************************************
 */
static struct pb_line_reader_operations pb_trivial_line_reader_operations = {
  .has_line = &pb_trivial_line_reader_has_line,

  .get_line_len = &pb_trivial_line_reader_get_line_len,
  .get_line_data = &pb_trivial_line_reader_get_line_data,

  .seek_line = &pb_trivial_line_reader_seek_line,

  .is_crlf = &pb_trivial_line_reader_is_crlf,
  .is_end = &pb_trivial_line_reader_is_end,

  .terminate_line = &pb_trivial_line_reader_terminate_line,
  .terminate_line_check_cr = &pb_trivial_line_reader_terminate_line_check_cr,

  .clone = &pb_trivial_line_reader_clone,

  .reset = &pb_trivial_line_reader_reset,
  .destroy = &pb_trivial_line_reader_destroy,
};

const struct pb_line_reader_operations *pb_get_trivial_line_reader_operations() {
  return &pb_trivial_line_reader_operations;
}



/*******************************************************************************
 */
struct pb_trivial_line_reader {
  struct pb_line_reader line_reader;

  struct pb_buffer_byte_iterator byte_iterator;

  uint64_t buffer_data_revision;

  size_t buffer_offset;

  bool has_cr;
  bool has_line;
  bool is_terminated;
  bool is_terminated_with_cr;
};



/*******************************************************************************
 */
bool pb_line_reader_has_line(struct pb_line_reader * const line_reader) {
  return line_reader->operations->has_line(line_reader);
}

size_t pb_line_reader_get_line_len(struct pb_line_reader * const line_reader) {
  return line_reader->operations->get_line_len(line_reader);
}

size_t pb_line_reader_get_line_data(struct pb_line_reader * const line_reader,
    void * const buf, uint64_t len) {
  return line_reader->operations->get_line_data(line_reader, buf, len);
}

bool pb_line_reader_is_crlf(struct pb_line_reader * const line_reader) {
  return line_reader->operations->is_crlf(line_reader);
}

bool pb_line_reader_is_end(struct pb_line_reader * const line_reader) {
  return line_reader->operations->is_end(line_reader);
}

void pb_line_reader_terminate_line(struct pb_line_reader * const line_reader) {
  line_reader->operations->terminate_line(line_reader);
}

void pb_line_reader_terminate_line_check_cr(
    struct pb_line_reader * const line_reader) {
  line_reader->operations->terminate_line_check_cr(line_reader);
}

size_t pb_line_reader_seek_line(struct pb_line_reader * const line_reader) {
  return line_reader->operations->seek_line(line_reader);
}

struct pb_line_reader *pb_line_reader_clone(
    struct pb_line_reader * const line_reader) {
  return line_reader->operations->clone(line_reader);
}

void pb_line_reader_reset(struct pb_line_reader * const line_reader) {
  line_reader->operations->reset(line_reader);
}

void pb_line_reader_destroy(struct pb_line_reader * const line_reader) {
  line_reader->operations->destroy(line_reader);
}



/*******************************************************************************
 */
struct pb_line_reader *pb_trivial_line_reader_create(
    struct pb_buffer * const buffer) {
  const struct pb_allocator *allocator = buffer->allocator;

  struct pb_trivial_line_reader *trivial_line_reader =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_line_reader));
  if (!trivial_line_reader)
    return NULL;

  trivial_line_reader->line_reader.operations =
    pb_get_trivial_line_reader_operations();

  trivial_line_reader->line_reader.buffer = buffer;

  pb_line_reader_reset(&trivial_line_reader->line_reader);

  return &trivial_line_reader->line_reader;
}

/*******************************************************************************
 */
bool pb_trivial_line_reader_has_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = line_reader->buffer;
  struct pb_buffer_byte_iterator *byte_iterator =
    &trivial_line_reader->byte_iterator;

  if (trivial_line_reader->buffer_data_revision !=
        pb_buffer_get_data_revision(buffer))
    pb_line_reader_reset(line_reader);

  if (trivial_line_reader->has_line)
    return true;

  if (pb_buffer_get_data_size(buffer) == 0)
    return false;

  while (!pb_buffer_byte_iterator_is_end(buffer, byte_iterator)) {
    if (*byte_iterator->current_byte == '\n')
      return (trivial_line_reader->has_line = true);
    else if (*byte_iterator->current_byte == '\r')
      trivial_line_reader->has_cr = true;
    else
      trivial_line_reader->has_cr = false;

    if (trivial_line_reader->buffer_offset == PB_LINE_READER_MAX_LINE_SIZE) {
      trivial_line_reader->has_cr = false;

      return (trivial_line_reader->has_line = true);
    }

    pb_buffer_byte_iterator_next(buffer, byte_iterator);

    ++trivial_line_reader->buffer_offset;
  }

  // reset back to last position
  pb_buffer_byte_iterator_prev(buffer, byte_iterator);

  --trivial_line_reader->buffer_offset;

  if (trivial_line_reader->is_terminated_with_cr)
    return (trivial_line_reader->has_line = true);

  if (trivial_line_reader->is_terminated) {
    trivial_line_reader->has_cr = false;

    return (trivial_line_reader->has_line = true);
  }

  return false;
}

/*******************************************************************************
 */
size_t pb_trivial_line_reader_get_line_len(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = line_reader->buffer;

  if (trivial_line_reader->buffer_data_revision !=
        pb_buffer_get_data_revision(buffer))
    pb_line_reader_reset(line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  if (trivial_line_reader->has_cr)
    return (trivial_line_reader->buffer_offset - 1);

  return trivial_line_reader->buffer_offset;
}

/*******************************************************************************
 */
size_t pb_trivial_line_reader_get_line_data(
    struct pb_line_reader * const line_reader,
    void * const buf, uint64_t len) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = line_reader->buffer;

  if (trivial_line_reader->buffer_data_revision !=
        pb_buffer_get_data_revision(buffer))
    pb_line_reader_reset(line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  size_t getted = 0;

  size_t line_len = pb_line_reader_get_line_len(line_reader);

  struct pb_buffer_iterator buffer_iterator;
  pb_buffer_get_iterator(buffer, &buffer_iterator);

  while ((len > 0) &&
         (line_len > 0) &&
         (!pb_buffer_iterator_is_end(buffer, &buffer_iterator))) {
    size_t to_get =
      (pb_buffer_iterator_get_len(&buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&buffer_iterator) : len;

    to_get =
      (to_get < line_len) ?
       to_get : line_len;

    memcpy(
      (uint8_t*)buf + getted,
      pb_buffer_iterator_get_base(&buffer_iterator),
      to_get);

    len -= to_get;
    line_len -= to_get;
    getted += to_get;

    pb_buffer_iterator_next(buffer, &buffer_iterator);
  }

  return getted;
}

/*******************************************************************************
 */
size_t pb_trivial_line_reader_seek_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = line_reader->buffer;

  if (trivial_line_reader->buffer_data_revision !=
        pb_buffer_get_data_revision(buffer))
    pb_line_reader_reset(line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  size_t to_seek =
    (trivial_line_reader->is_terminated) ?
      trivial_line_reader->buffer_offset :
      trivial_line_reader->buffer_offset + 1;

  to_seek = pb_buffer_seek(buffer, to_seek);

  pb_line_reader_reset(line_reader);

  return to_seek;
}

/*******************************************************************************
 */
bool pb_trivial_line_reader_is_crlf(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;

  return trivial_line_reader->has_cr;
}

bool pb_trivial_line_reader_is_end(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;

  return
    pb_buffer_byte_iterator_is_end(buffer, &trivial_line_reader->byte_iterator);
}

/*******************************************************************************
 */
void pb_trivial_line_reader_terminate_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;

  trivial_line_reader->is_terminated = true;
}

void pb_trivial_line_reader_terminate_line_check_cr(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;

  trivial_line_reader->is_terminated_with_cr = true;
}

/*******************************************************************************
 */
struct pb_line_reader *pb_trivial_line_reader_clone(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  const struct pb_allocator *allocator =
    line_reader->buffer->allocator;

  struct pb_trivial_line_reader *trivial_line_reader_clone =
    pb_allocator_alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_line_reader));
  if (!trivial_line_reader_clone)
    return NULL;

  memcpy(
    trivial_line_reader_clone,
    trivial_line_reader,
    sizeof(struct pb_line_reader));

  return &trivial_line_reader_clone->line_reader;
}

/*******************************************************************************
 */
void pb_trivial_line_reader_reset(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;

  pb_buffer_get_byte_iterator(buffer, &trivial_line_reader->byte_iterator);

  trivial_line_reader->buffer_data_revision =
    pb_buffer_get_data_revision(buffer);

  trivial_line_reader->buffer_offset = 0;

  trivial_line_reader->has_cr = false;
  trivial_line_reader->has_line = false;
  trivial_line_reader->is_terminated = false;
  trivial_line_reader->is_terminated_with_cr = false;
}

/*******************************************************************************
 */
void pb_trivial_line_reader_destroy(struct pb_line_reader * const line_reader) {
  const struct pb_allocator *allocator =
    line_reader->buffer->allocator;

  pb_line_reader_reset(line_reader);

  pb_allocator_free(
    allocator,
    pb_alloc_type_struct,
    line_reader, sizeof(struct pb_trivial_line_reader));
}
