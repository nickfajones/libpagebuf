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

#include "pagebuf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>



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
};

static void pb_trivial_free(const struct pb_allocator *allocator,
    enum pb_allocator_type type, void *obj, size_t size) {
  memset(obj, 0, size);

  free(obj);
}

static struct pb_allocator pb_trivial_allocator = {
  .alloc = pb_trivial_alloc,
  .free = pb_trivial_free,
};

const struct pb_allocator *pb_get_trivial_allocator(void) {
  return &pb_trivial_allocator;
}



/*******************************************************************************
 */
struct pb_data *pb_data_create(uint8_t * const buf, size_t len,
    const struct pb_allocator *allocator) {
  struct pb_data *data =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_data));
  if (!data)
    return NULL;

  data->data_vec.base = buf;
  data->data_vec.len = len;

  data->use_count = 1;

  data->responsibility = pb_data_owned;

  data->allocator = allocator;

  return data;
}

struct pb_data *pb_data_create_ref(const uint8_t *buf, size_t len,
    const struct pb_allocator *allocator) {
  struct pb_data *data =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_data));
  if (!data)
    return NULL;

  data->data_vec.base = (uint8_t*)buf; // we won't actually change it
  data->data_vec.len = len;

  data->use_count = 1;

  data->responsibility = pb_data_referenced;

  data->allocator = allocator;

  return data;
}

struct pb_data *pb_data_clone(uint8_t * const buf, size_t len, size_t src_off,
    const struct pb_data *src_data,
    const struct pb_allocator *allocator) {
  struct pb_data *data =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_data));
  if (!data)
    return NULL;

  data->data_vec.base = buf;
  data->data_vec.len = len;

  memcpy(data->data_vec.base, src_data->data_vec.base + src_off, len);

  data->use_count = 1;

  data->responsibility = pb_data_owned;

  data->allocator = allocator;

  return data;
  }

void pb_data_destroy(struct pb_data * const data) {
  const struct pb_allocator *allocator = data->allocator;

  if (data->responsibility == pb_data_owned)
    allocator->free(
      allocator, pb_alloc_type_struct, data->data_vec.base, data->data_vec.len);

  allocator->free(
    allocator, pb_alloc_type_struct, data, sizeof(struct pb_data));
}

/*******************************************************************************
 */
void pb_data_get(struct pb_data *data) {
  __sync_add_and_fetch(&data->use_count, 1);
}

void pb_data_put(struct pb_data *data) {
  if (__sync_sub_and_fetch(&data->use_count, 1) != 0)
    return;

  pb_data_destroy(data);
}



/*******************************************************************************
 */
struct pb_page *pb_page_create(struct pb_data *data,
    const struct pb_allocator *allocator) {
  struct pb_page *page =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_page));
  if (!page)
    return NULL;

  pb_data_get(data);

  page->data_vec.base = data->data_vec.base;
  page->data_vec.len = data->data_vec.len;
  page->data = data;
  page->prev = NULL;
  page->next = NULL;

  return page;
}

struct pb_page *pb_page_transfer(const struct pb_page *src_page,
    size_t len, size_t src_off,
    const struct pb_allocator *allocator) {
  struct pb_page *page =
    allocator->alloc(allocator, pb_alloc_type_struct, sizeof(struct pb_page));
  if (!page)
    return NULL;

  pb_data_get(src_page->data);

  page->data_vec.base = src_page->data_vec.base + src_off;
  page->data_vec.len = len;
  page->data = src_page->data;
  page->prev = NULL;
  page->next = NULL;

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

  allocator->free(
    allocator, pb_alloc_type_struct, page, sizeof(struct pb_page));
}



/*******************************************************************************
 */
static struct pb_buffer_strategy pb_trivial_buffer_strategy = {
  .page_size = PB_TRIVIAL_BUFFER_DEFAULT_PAGE_SIZE,
  .clone_on_write = false,
  .fragment_as_target = false,
};

const struct pb_buffer_strategy *pb_get_trivial_buffer_strategy(void) {
  return &pb_trivial_buffer_strategy;
}



/*******************************************************************************
 */
struct pb_trivial_buffer {
  struct pb_buffer buffer;

  struct pb_page page_end;

  uint64_t data_revision;
  uint64_t data_size;
};

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
  struct pb_trivial_buffer *trivial_buffer =
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_buffer));
  if (!trivial_buffer)
    return NULL;

  ((struct pb_buffer_strategy*)&trivial_buffer->buffer.strategy)->
    page_size = strategy->page_size;
  ((struct pb_buffer_strategy*)&trivial_buffer->buffer.strategy)->
    clone_on_write = strategy->clone_on_write;
  ((struct pb_buffer_strategy*)&trivial_buffer->buffer.strategy)->
    fragment_as_target = strategy->fragment_as_target;

  trivial_buffer->buffer.get_data_size = &pb_trivial_buffer_get_data_size;
  trivial_buffer->buffer.get_data_revision = &pb_trivial_buffer_get_data_revision;

  trivial_buffer->buffer.insert = &pb_trivial_buffer_insert;

  trivial_buffer->buffer.reserve = &pb_trivial_buffer_reserve;
  trivial_buffer->buffer.rewind = &pb_trivial_buffer_rewind;
  trivial_buffer->buffer.seek = &pb_trivial_buffer_seek;

  trivial_buffer->buffer.get_iterator = &pb_trivial_buffer_get_iterator;
  trivial_buffer->buffer.get_iterator_end = &pb_trivial_buffer_get_iterator_end;
  trivial_buffer->buffer.is_iterator_end = &pb_trivial_buffer_is_iterator_end;

  trivial_buffer->buffer.iterator_next = &pb_trivial_buffer_iterator_next;
  trivial_buffer->buffer.iterator_prev = &pb_trivial_buffer_iterator_prev;

  trivial_buffer->buffer.write_data = &pb_trivial_buffer_write_data;
  trivial_buffer->buffer.write_buffer = &pb_trivial_buffer_write_buffer;

  trivial_buffer->buffer.overwrite_data = &pb_trivial_buffer_overwrite_data;

  trivial_buffer->buffer.read_data = &pb_trivial_buffer_read_data;

  trivial_buffer->buffer.clear = &pb_trivial_buffer_clear;
  trivial_buffer->buffer.destroy = &pb_trivial_buffer_destroy;

  trivial_buffer->buffer.allocator = allocator;

  trivial_buffer->page_end.prev = &trivial_buffer->page_end;
  trivial_buffer->page_end.next = &trivial_buffer->page_end;

  trivial_buffer->data_revision = 0;
  trivial_buffer->data_size = 0;

  return &trivial_buffer->buffer;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_get_data_size(struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  return trivial_buffer->data_size;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_get_data_revision(struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  return trivial_buffer->data_revision;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_insert(struct pb_buffer * const buffer,
    struct pb_page * const page,
    struct pb_buffer_iterator * const buffer_iterator,
    size_t offset) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  while ((offset > 0) &&
         (offset >= buffer_iterator->page->data_vec.len)) {
    trivial_buffer->buffer.iterator_next(
      &trivial_buffer->buffer, buffer_iterator);

    if (!trivial_buffer->buffer.is_iterator_end(
          &trivial_buffer->buffer, buffer_iterator))
      offset -= buffer_iterator->page->data_vec.len;
    else
      offset = 0;
  }

  struct pb_page *page1;
  struct pb_page *page2;

  if (offset != 0) {
    page1 = buffer_iterator->page;
    page2 =
      pb_page_transfer(
        page1, page1->data_vec.len, 0,
        trivial_buffer->buffer.allocator);
    if (!page2)
      return 0;

    page2->data_vec.base += offset;
    page2->data_vec.len -= offset;
    page2->prev = page1;
    page2->next = page1->next;

    page1->data_vec.len = offset;
    page1->next->prev = page2;
    page1->next = page2;
  } else {
    page1 = buffer_iterator->page->prev;
    page2 = buffer_iterator->page;
  }

  page->prev = page1;
  page->next = page2;

  page1->next = page;
  page2->prev = page;

  trivial_buffer->data_size += page->data_vec.len;

  return page->data_vec.len;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_reserve(struct pb_buffer * const buffer,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  const struct pb_allocator *allocator = trivial_buffer->buffer.allocator;

  uint64_t reserved = 0;

  while (len > 0) {
    uint64_t reserve_len =
      (trivial_buffer->buffer.strategy.page_size != 0) ?
      (trivial_buffer->buffer.strategy.page_size < len) ?
       trivial_buffer->buffer.strategy.page_size : len :
       len;

    void *buf =
      allocator->alloc(allocator, pb_alloc_type_region, reserve_len);
    if (!buf)
      return reserved;

    struct pb_data *data = pb_data_create(buf, reserve_len, allocator);
    if (!data) {
      allocator->free(allocator, pb_alloc_type_region, buf, reserve_len);

      return reserved;
    }

    struct pb_page *page = pb_page_create(data, allocator);
    if (!page) {
      pb_data_put(data);

      return reserved;
    }

    pb_data_put(data);

    struct pb_buffer_iterator end_itr;
    trivial_buffer->buffer.get_iterator_end(&trivial_buffer->buffer, &end_itr);

    len -= reserve_len;
    reserved +=
      trivial_buffer->buffer.insert(
        &trivial_buffer->buffer, page, &end_itr, 0);
  }

  return reserved;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_rewind(struct pb_buffer * const buffer,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  const struct pb_allocator *allocator = trivial_buffer->buffer.allocator;

  ++trivial_buffer->data_revision;

  uint64_t rewinded = 0;

  while (len > 0) {
    uint64_t reserve_len =
      (trivial_buffer->buffer.strategy.page_size != 0) ?
      (trivial_buffer->buffer.strategy.page_size < len) ?
       trivial_buffer->buffer.strategy.page_size : len :
       len;

    void *buf =
      allocator->alloc(allocator, pb_alloc_type_region, reserve_len);
    if (!buf)
      return rewinded;

    struct pb_data *data = pb_data_create(buf, reserve_len, allocator);
    if (!data) {
      allocator->free(allocator, pb_alloc_type_region, buf, reserve_len);

      return rewinded;
    }

    struct pb_page *page = pb_page_create(data, allocator);
    if (!page) {
      pb_data_put(data);

      return rewinded;
    }

    pb_data_put(data);

    struct pb_buffer_iterator itr;
    trivial_buffer->buffer.get_iterator_end(&trivial_buffer->buffer, &itr);

    len -= reserve_len;
    rewinded +=
      trivial_buffer->buffer.insert(
        &trivial_buffer->buffer, page, &itr, 0);
  }

  return rewinded;
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_seek(struct pb_buffer * const buffer, uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  const struct pb_allocator *allocator = trivial_buffer->buffer.allocator;

  ++trivial_buffer->data_revision;

  uint64_t seeked = 0;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator(&trivial_buffer->buffer, &itr);

  while ((len > 0) &&
         (!trivial_buffer->buffer.is_iterator_end(
            &trivial_buffer->buffer, &itr))) {
    uint64_t seek_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    itr.page->data_vec.base += seek_len;
    itr.page->data_vec.len -= seek_len;

    if (itr.page->data_vec.len == 0) {
      struct pb_page *page = itr.page;

      trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);

      page->prev->next = itr.page;
      itr.page->prev = page->prev;

      page->prev = NULL;
      page->next = NULL;

      pb_page_destroy(page, allocator);
    }

    len -= seek_len;
    seeked += seek_len;

    trivial_buffer->data_size -= seek_len;
  }

  return seeked;
}

/*******************************************************************************
 */
void pb_trivial_buffer_get_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  iterator->page = trivial_buffer->page_end.next;
}

void pb_trivial_buffer_get_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  iterator->page = &trivial_buffer->page_end;
}

bool pb_trivial_buffer_is_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  return (iterator->page == &trivial_buffer->page_end);
}

/*******************************************************************************
 */
void pb_trivial_buffer_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  iterator->page = iterator->page->next;
}

void pb_trivial_buffer_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const iterator) {
  iterator->page = iterator->page->prev;
}

/*******************************************************************************
 * fragment_as_target: false
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_data1(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  if (trivial_buffer->buffer.get_data_size(&trivial_buffer->buffer) == 0)
    ++trivial_buffer->data_revision;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator_end(&trivial_buffer->buffer, &itr);
  trivial_buffer->buffer.iterator_prev(&trivial_buffer->buffer, &itr);

  uint64_t written = 0;

  while (len > 0) {
    uint64_t write_len =
      (trivial_buffer->buffer.strategy.page_size != 0) ?
      (trivial_buffer->buffer.strategy.page_size < len) ?
       trivial_buffer->buffer.strategy.page_size : len :
       len;

    write_len =
      trivial_buffer->buffer.reserve(&trivial_buffer->buffer, write_len);

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    len -= write_len;
    written += write_len;

    trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);
  }

  return written;
}

/*
 * fragment_as_target: true
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_data2(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  if (trivial_buffer->buffer.get_data_size(&trivial_buffer->buffer) == 0)
    ++trivial_buffer->data_revision;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator_end(&trivial_buffer->buffer, &itr);
  trivial_buffer->buffer.iterator_prev(&trivial_buffer->buffer, &itr);

  len = trivial_buffer->buffer.reserve(&trivial_buffer->buffer, len);

  if (!trivial_buffer->buffer.is_iterator_end(&trivial_buffer->buffer, &itr)) {
    trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);
  } else {
    trivial_buffer->buffer.get_iterator(&trivial_buffer->buffer, &itr);
  }

  uint64_t written = 0;

  while (len > 0) {
    uint64_t write_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    len -= write_len;
    written += write_len;

    trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);
  }

  return written;
}

uint64_t pb_trivial_buffer_write_data(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  if (!buffer->strategy.fragment_as_target)
    return pb_trivial_buffer_write_data1(buffer, buf, len);

  return pb_trivial_buffer_write_data2(buffer, buf, len);
}

/*******************************************************************************
 * clone_on_write: false
 * fragment_as_target: false
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_buffer1(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  const struct pb_allocator *allocator = trivial_buffer->buffer.allocator;

  if (trivial_buffer->buffer.get_data_size(&trivial_buffer->buffer) == 0)
    ++trivial_buffer->data_revision;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator_end(&trivial_buffer->buffer, &itr);

  struct pb_buffer_iterator src_itr;
  src_buffer->get_iterator(src_buffer, &src_itr);

  uint64_t written = 0;

  while ((len > 0) &&
         (!src_buffer->is_iterator_end(src_buffer, &src_itr))) {
    uint64_t write_len =
      (src_itr.page->data_vec.len < len) ?
       src_itr.page->data_vec.len : len;

    struct pb_page *page =
      pb_page_transfer(src_itr.page, write_len, 0, allocator);
    if (!page)
      return written;

    write_len =
      trivial_buffer->buffer.insert(&trivial_buffer->buffer, page, &itr, 0);

    len -= write_len;
    written += write_len;

    src_buffer->iterator_next(src_buffer, &src_itr);
  }

  return written;
}

/*
 * clone_on_write: false
 * fragment_as_target: true
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_buffer2(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  const struct pb_allocator *allocator = trivial_buffer->buffer.allocator;

  if (trivial_buffer->buffer.get_data_size(&trivial_buffer->buffer) == 0)
    ++trivial_buffer->data_revision;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator_end(&trivial_buffer->buffer, &itr);

  struct pb_buffer_iterator src_itr;
  src_buffer->get_iterator(src_buffer, &src_itr);

  uint64_t written = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!src_buffer->is_iterator_end(src_buffer, &src_itr))) {
    uint64_t write_len =
      (trivial_buffer->buffer.strategy.page_size != 0) ?
      (trivial_buffer->buffer.strategy.page_size < len) ?
       trivial_buffer->buffer.strategy.page_size : len :
       len;

    write_len =
      (src_itr.page->data_vec.len - src_offset < write_len) ?
       src_itr.page->data_vec.len - src_offset : write_len;

    struct pb_page *page =
      pb_page_transfer(src_itr.page, write_len, src_offset, allocator);
    if (!page)
      return written;

    write_len =
      trivial_buffer->buffer.insert(&trivial_buffer->buffer, page, &itr, 0);

    len -= write_len;
    written += write_len;
    src_offset += write_len;

    if (src_offset == src_itr.page->data_vec.len) {
      src_buffer->iterator_next(src_buffer, &src_itr);

      src_offset = 0;
    }
  }

  return written;
}

/*
 * clone_on_write: true
 * fragment_as_target: false
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_buffer3(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  if (trivial_buffer->buffer.get_data_size(&trivial_buffer->buffer) == 0)
    ++trivial_buffer->data_revision;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator_end(&trivial_buffer->buffer, &itr);
  trivial_buffer->buffer.iterator_prev(&trivial_buffer->buffer, &itr);

  struct pb_buffer_iterator src_itr;
  src_buffer->get_iterator(src_buffer, &src_itr);

  uint64_t written = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!src_buffer->is_iterator_end(src_buffer, &src_itr))) {
    uint64_t write_len =
      (trivial_buffer->buffer.strategy.page_size != 0) ?
      (trivial_buffer->buffer.strategy.page_size < len) ?
       trivial_buffer->buffer.strategy.page_size : len :
       len;

    write_len =
      (src_itr.page->data_vec.len - src_offset < write_len) ?
       src_itr.page->data_vec.len - src_offset : write_len;

    write_len =
      trivial_buffer->buffer.reserve(&trivial_buffer->buffer, write_len);

    if (!trivial_buffer->buffer.is_iterator_end(&trivial_buffer->buffer, &itr))
      trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);
    else
      trivial_buffer->buffer.get_iterator(&trivial_buffer->buffer, &itr);

    memcpy(
      itr.page->data_vec.base,
      src_itr.page->data_vec.base + src_offset,
      write_len);

    len -= write_len;
    written += write_len;
    src_offset += write_len;

    if (src_offset == src_itr.page->data_vec.len) {
      src_buffer->iterator_next(src_buffer, &src_itr);

      src_offset = 0;
    }
  }

  return written;
}

/*
 * clone_on_write: true
 * fragment_as_target: true
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_buffer_write_buffer4(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  if (trivial_buffer->buffer.get_data_size(&trivial_buffer->buffer) == 0)
    ++trivial_buffer->data_revision;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator_end(&trivial_buffer->buffer, &itr);
  trivial_buffer->buffer.iterator_prev(&trivial_buffer->buffer, &itr);

  struct pb_buffer_iterator src_itr;
  src_buffer->get_iterator(src_buffer, &src_itr);

  len = trivial_buffer->buffer.reserve(&trivial_buffer->buffer, len);

  if (!trivial_buffer->buffer.is_iterator_end(&trivial_buffer->buffer, &itr)) {
    trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);
  } else {
    trivial_buffer->buffer.get_iterator(&trivial_buffer->buffer, &itr);
  }

  uint64_t written = 0;
  size_t offset = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!trivial_buffer->buffer.is_iterator_end(
            &trivial_buffer->buffer, &itr)) &&
         (!src_buffer->is_iterator_end(src_buffer, &src_itr))) {
    uint64_t write_len =
      (itr.page->data_vec.len - offset < len) ?
       itr.page->data_vec.len - offset: len;

    write_len =
      (src_itr.page->data_vec.len - src_offset < write_len) ?
       src_itr.page->data_vec.len - src_offset: write_len;

    memcpy(
      itr.page->data_vec.base + offset,
      src_itr.page->data_vec.base + src_offset,
      write_len);

    len -= write_len;
    written += write_len;
    offset += write_len;
    src_offset += write_len;

    if (offset == itr.page->data_vec.len) {
      trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);

      offset = 0;
    }

    if (src_offset == src_itr.page->data_vec.len) {
      src_buffer->iterator_next(src_buffer, &src_itr);

      src_offset = 0;
    }
  }

  return written;
}

uint64_t pb_trivial_buffer_write_buffer(struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  if ((!buffer->strategy.clone_on_write) &&
      (!buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_buffer1(buffer, src_buffer, len);
  } else if ((!buffer->strategy.clone_on_write) &&
             (buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_buffer2(buffer, src_buffer, len);
  } else if ((buffer->strategy.clone_on_write) &&
             (!buffer->strategy.fragment_as_target)) {
    return pb_trivial_buffer_write_buffer3(buffer, src_buffer, len);
  }
  /*else if ((buffer->strategy->clone_on_write) &&
             (buffer->strategy->fragment_as_target)) {*/
  return pb_trivial_buffer_write_buffer4(buffer, src_buffer, len);
}

/*******************************************************************************
 */
uint64_t pb_trivial_buffer_overwrite_data(struct pb_buffer * const buffer,
    const uint8_t *buf,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  ++trivial_buffer->data_revision;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator(&trivial_buffer->buffer, &itr);

  uint64_t written = 0;

  while ((len > 0) &&
         (!trivial_buffer->buffer.is_iterator_end(
            &trivial_buffer->buffer, &itr))) {
    uint64_t write_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    len -= write_len;
    written += write_len;

    trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);
  }

  return written;
}

uint64_t pb_trivial_buffer_read_data(struct pb_buffer * const buffer,
    uint8_t * const buf,
    uint64_t len) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  struct pb_buffer_iterator itr;
  trivial_buffer->buffer.get_iterator(&trivial_buffer->buffer, &itr);

  uint64_t readed = 0;

  while ((len > 0) &&
         (!trivial_buffer->buffer.is_iterator_end(
            &trivial_buffer->buffer, &itr))) {
    size_t read_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(buf + readed, itr.page->data_vec.base, read_len);\

    len -= read_len;
    readed += read_len;

    trivial_buffer->buffer.iterator_next(&trivial_buffer->buffer, &itr);
  }

  return readed;
}

/*******************************************************************************
 */
void pb_trivial_buffer_clear(struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;

  uint64_t data_size =
    trivial_buffer->buffer.get_data_size(&trivial_buffer->buffer);

  trivial_buffer->buffer.seek(&trivial_buffer->buffer, data_size);
}

/*******************************************************************************
 */
void pb_trivial_buffer_destroy(struct pb_buffer * const buffer) {
  struct pb_trivial_buffer *trivial_buffer = (struct pb_trivial_buffer*)buffer;
  const struct pb_allocator *allocator = trivial_buffer->buffer.allocator;

  pb_trivial_buffer_clear(&trivial_buffer->buffer);

  allocator->free(
    allocator,
    pb_alloc_type_struct, trivial_buffer, sizeof(struct pb_trivial_buffer));
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
struct pb_data_reader *pb_trivial_data_reader_create(
    struct pb_buffer * const buffer) {
  return
    pb_trivial_data_reader_create_with_alloc(buffer, pb_get_trivial_allocator());
}

struct pb_data_reader *pb_trivial_data_reader_create_with_alloc(
    struct pb_buffer * const buffer, const struct pb_allocator *allocator) {
  struct pb_trivial_data_reader *trivial_data_reader =
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_data_reader));
  if (!trivial_data_reader)
    return NULL;

  trivial_data_reader->data_reader.read = &pb_trivial_data_reader_read;

  trivial_data_reader->data_reader.reset = &pb_trivial_data_reader_reset;
  trivial_data_reader->data_reader.destroy = &pb_trivial_data_reader_destroy;

  trivial_data_reader->data_reader.buffer = buffer;
  trivial_data_reader->data_reader.allocator = allocator;

  trivial_data_reader->data_reader.reset(&trivial_data_reader->data_reader);

  return &trivial_data_reader->data_reader;
}

uint64_t pb_trivial_data_reader_read(
    struct pb_data_reader * const data_reader,
    uint8_t * const buf,
    uint64_t len) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  struct pb_buffer *buffer = trivial_data_reader->data_reader.buffer;
  struct pb_buffer_iterator *itr = &trivial_data_reader->buffer_iterator;

  if (buffer->get_data_revision(buffer) != trivial_data_reader->buffer_data_revision)
    trivial_data_reader->data_reader.reset(&trivial_data_reader->data_reader);

  if (trivial_data_reader->page_offset == itr->page->data_vec.len)
    buffer->iterator_next(buffer, itr);

  uint64_t readed = 0;

  while ((len > 0) &&
         (!buffer->is_iterator_end(buffer, itr))) {
    uint64_t read_len =
      (itr->page->data_vec.len < len) ?
       itr->page->data_vec.len : len;

    memcpy(
      buf + trivial_data_reader->page_offset + readed,
      itr->page->data_vec.base,
      read_len);

    len -= read_len;
    readed += read_len;

    trivial_data_reader->page_offset += read_len;

    if (trivial_data_reader->page_offset != itr->page->data_vec.len)
      return readed;

    buffer->iterator_next(buffer, itr);

    trivial_data_reader->page_offset = 0;
  }

  if (buffer->is_iterator_end(buffer, itr)) {
    buffer->iterator_prev(buffer, itr);

    trivial_data_reader->page_offset = itr->page->data_vec.len;
  }

  return readed;
}

void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  struct pb_buffer *buffer = trivial_data_reader->data_reader.buffer;

  buffer->get_iterator(buffer, &trivial_data_reader->buffer_iterator);

  trivial_data_reader->buffer_data_revision = buffer->get_data_revision(buffer);

  trivial_data_reader->page_offset = 0;
}

void pb_trivial_data_reader_destroy(struct pb_data_reader * const data_reader) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  const struct pb_allocator *allocator =
    trivial_data_reader->data_reader.allocator;

  allocator->free(
    allocator,
    pb_alloc_type_struct,
    trivial_data_reader, sizeof(struct pb_trivial_data_reader));
}



/*******************************************************************************
 */
struct pb_trivial_byte_reader {
  struct pb_byte_reader byte_reader;

  struct pb_buffer_iterator buffer_iterator;

  uint64_t buffer_data_revision;

  size_t page_offset;
};

/*******************************************************************************
 */
struct pb_byte_reader *pb_trivial_byte_reader_create(
    struct pb_buffer * const buffer) {
  return
    pb_trivial_byte_reader_create_with_alloc(
      buffer, pb_get_trivial_allocator());
}

struct pb_byte_reader *pb_trivial_byte_reader_create_with_alloc(
    struct pb_buffer * const buffer,
    const struct pb_allocator *allocator) {
  struct pb_trivial_byte_reader *trivial_byte_reader =
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_byte_reader));
  if (!trivial_byte_reader)
    return NULL;

  trivial_byte_reader->byte_reader.get_current_byte =
    &pb_trivial_byte_reader_get_current_byte;

  trivial_byte_reader->byte_reader.next =
    &pb_trivial_byte_reader_next;
  trivial_byte_reader->byte_reader.prev =
    &pb_trivial_byte_reader_prev;

  trivial_byte_reader->byte_reader.is_end =
    &pb_trivial_byte_reader_is_end;

  trivial_byte_reader->byte_reader.destroy =
    &pb_trivial_byte_reader_destroy;

  trivial_byte_reader->byte_reader.reset =
    &pb_trivial_byte_reader_reset;

  trivial_byte_reader->byte_reader.buffer = buffer;

  trivial_byte_reader->byte_reader.allocator = allocator;

  pb_trivial_byte_reader_reset(&trivial_byte_reader->byte_reader);

  return &trivial_byte_reader->byte_reader;
}

uint8_t pb_trivial_byte_reader_get_current_byte(
    struct pb_byte_reader * const byte_reader) {
  struct pb_trivial_byte_reader *trivial_byte_reader =
    (struct pb_trivial_byte_reader*)byte_reader;
  struct pb_buffer *buffer = trivial_byte_reader->byte_reader.buffer;
  struct pb_buffer_iterator *itr = &trivial_byte_reader->buffer_iterator;

  if (buffer->get_data_revision(buffer) != trivial_byte_reader->buffer_data_revision)
    trivial_byte_reader->byte_reader.reset(&trivial_byte_reader->byte_reader);

  if (buffer->is_iterator_end(buffer, itr))
    return '\0';

  return itr->page->data_vec.base[trivial_byte_reader->page_offset];
}

void pb_trivial_byte_reader_next(struct pb_byte_reader * const byte_reader) {
  struct pb_trivial_byte_reader *trivial_byte_reader =
    (struct pb_trivial_byte_reader*)byte_reader;
  struct pb_buffer *buffer = trivial_byte_reader->byte_reader.buffer;
  struct pb_buffer_iterator *itr = &trivial_byte_reader->buffer_iterator;

  if (buffer->get_data_revision(buffer) != trivial_byte_reader->buffer_data_revision) {
    trivial_byte_reader->byte_reader.reset(&trivial_byte_reader->byte_reader);

    return;
  }

  if (buffer->is_iterator_end(buffer, itr))
    return;

  ++trivial_byte_reader->page_offset;

  if (trivial_byte_reader->page_offset == itr->page->data_vec.len) {
    buffer->iterator_next(buffer, itr);

    trivial_byte_reader->page_offset = 0;
  }
}

void pb_trivial_byte_reader_prev(struct pb_byte_reader * const byte_reader) {
  struct pb_trivial_byte_reader *trivial_byte_reader =
    (struct pb_trivial_byte_reader*)byte_reader;
  struct pb_buffer *buffer = trivial_byte_reader->byte_reader.buffer;
  struct pb_buffer_iterator *itr = &trivial_byte_reader->buffer_iterator;

  if (buffer->get_data_revision(buffer) != trivial_byte_reader->buffer_data_revision) {
    trivial_byte_reader->byte_reader.reset(&trivial_byte_reader->byte_reader);

    return;
  }

  if (trivial_byte_reader->page_offset == 0) {
    buffer->iterator_prev(buffer, itr);

    trivial_byte_reader->page_offset = itr->page->data_vec.len;
  }

  if (buffer->is_iterator_end(buffer, itr))
    return;

  --trivial_byte_reader->page_offset;
}

bool pb_trivial_byte_reader_is_end(struct pb_byte_reader * const byte_reader) {
  struct pb_trivial_byte_reader *trivial_byte_reader =
    (struct pb_trivial_byte_reader*)byte_reader;
  struct pb_buffer *buffer = trivial_byte_reader->byte_reader.buffer;

  if (buffer->get_data_revision(buffer) != trivial_byte_reader->buffer_data_revision)
    trivial_byte_reader->byte_reader.reset(&trivial_byte_reader->byte_reader);

  return buffer->is_iterator_end(buffer, &trivial_byte_reader->buffer_iterator);
}

void pb_trivial_byte_reader_reset(struct pb_byte_reader * const byte_reader) {
  struct pb_trivial_byte_reader *trivial_byte_reader =
    (struct pb_trivial_byte_reader*)byte_reader;
  struct pb_buffer *buffer = trivial_byte_reader->byte_reader.buffer;

  buffer->get_iterator(buffer, &trivial_byte_reader->buffer_iterator);

  trivial_byte_reader->buffer_data_revision = buffer->get_data_revision(buffer);

  trivial_byte_reader->page_offset = 0;
}

void pb_trivial_byte_reader_destroy(struct pb_byte_reader * const byte_reader) {
  struct pb_trivial_byte_reader *trivial_byte_reader =
    (struct pb_trivial_byte_reader*)byte_reader;
  const struct pb_allocator *allocator =
    trivial_byte_reader->byte_reader.allocator;

  allocator->free(
    allocator,
    pb_alloc_type_struct,
    trivial_byte_reader, sizeof(struct pb_trivial_byte_reader));
}



/*******************************************************************************
 */
struct pb_trivial_line_reader {
  struct pb_line_reader line_reader;

  struct pb_buffer_iterator buffer_iterator;

  uint64_t buffer_data_revision;

  uint64_t buffer_offset;

  size_t page_offset;

  bool has_line;
  bool has_cr;
  bool is_terminated;
};

/*******************************************************************************
 */
struct pb_line_reader *pb_trivial_line_reader_create(
    struct pb_buffer * const buffer) {
  return
    pb_trivial_line_reader_create_with_alloc(
      buffer, pb_get_trivial_allocator());
}

struct pb_line_reader *pb_trivial_line_reader_create_with_alloc(
    struct pb_buffer * const buffer,
    const struct pb_allocator *allocator) {
  struct pb_trivial_line_reader *trivial_line_reader =
    allocator->alloc(
      allocator, pb_alloc_type_struct, sizeof(struct pb_trivial_line_reader));
  if (!trivial_line_reader)
    return NULL;

  trivial_line_reader->line_reader.has_line = &pb_trivial_line_reader_has_line;

  trivial_line_reader->line_reader.get_line_len =
    &pb_trivial_line_reader_get_line_len;
  trivial_line_reader->line_reader.get_line_data =
    &pb_trivial_line_reader_get_line_data;

  trivial_line_reader->line_reader.seek_line =
    &pb_trivial_line_reader_seek_line;

  trivial_line_reader->line_reader.is_crlf =
    &pb_trivial_line_reader_is_crlf;
  trivial_line_reader->line_reader.is_end =
    &pb_trivial_line_reader_is_end;

  trivial_line_reader->line_reader.terminate_line =
    &pb_trivial_line_reader_terminate_line;

  trivial_line_reader->line_reader.destroy = &pb_trivial_line_reader_destroy;

  trivial_line_reader->line_reader.buffer = buffer;

  trivial_line_reader->line_reader.allocator = allocator;

  pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  return &trivial_line_reader->line_reader;
}

/*******************************************************************************
 */
bool pb_trivial_line_reader_has_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;
  struct pb_buffer_iterator *itr = &trivial_line_reader->buffer_iterator;

  if (trivial_line_reader->buffer_data_revision != buffer->get_data_revision(buffer))
    pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  if (trivial_line_reader->has_line)
    return true;

  if (buffer->is_iterator_end(buffer, itr))
    buffer->iterator_prev(buffer, itr);

  while (!buffer->is_iterator_end(buffer, itr)) {
    const struct pb_data_vec *data_vec =
      &trivial_line_reader->buffer_iterator.page->data_vec;

    while (trivial_line_reader->page_offset < data_vec->len) {
      if (data_vec->base[trivial_line_reader->page_offset] == '\n')
        return (trivial_line_reader->has_line = true);
      else if (data_vec->base[trivial_line_reader->page_offset] == '\r')
        trivial_line_reader->has_cr = true;
      else
        trivial_line_reader->has_cr = false;

      ++trivial_line_reader->buffer_offset;
      ++trivial_line_reader->page_offset;
    }

    buffer->iterator_next(buffer, itr);

    trivial_line_reader->page_offset = 0;
  }

  if (buffer->is_iterator_end(buffer, itr)) {
    buffer->iterator_prev(buffer, itr);

    trivial_line_reader->page_offset = itr->page->data_vec.len;

    if (trivial_line_reader->is_terminated)
      return (trivial_line_reader->has_line = true);
  }

  return false;
}

/*******************************************************************************
 */
uint64_t pb_trivial_line_reader_get_line_len(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;

  if (trivial_line_reader->buffer_data_revision != buffer->get_data_revision(buffer))
    pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  if (trivial_line_reader->has_cr)
    return (trivial_line_reader->buffer_offset - 1);

  return trivial_line_reader->buffer_offset;
}

/*******************************************************************************
 */
uint64_t pb_trivial_line_reader_get_line_data(
    struct pb_line_reader * const line_reader,
    uint8_t * const buf, uint64_t len) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;

  if (trivial_line_reader->buffer_data_revision != buffer->get_data_revision(buffer))
    pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  uint64_t getted = 0;

  uint64_t line_len =
    trivial_line_reader->line_reader.get_line_len(
      &trivial_line_reader->line_reader);

  struct pb_buffer_iterator itr;
  buffer->get_iterator(buffer, &itr);

  while ((len > 0) &&
         (line_len > 0) &&
         (!buffer->is_iterator_end(buffer, &itr))) {
    size_t to_get =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    to_get =
      (to_get < line_len) ?
       to_get : line_len;

    memcpy(buf + getted, itr.page->data_vec.base, to_get);

    len -= to_get;
    line_len -= to_get;
    getted += to_get;

    buffer->iterator_next(buffer, &itr);
  }

  return getted;
}

/*******************************************************************************
 */
uint64_t pb_trivial_line_reader_seek_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;

  if (trivial_line_reader->buffer_data_revision !=
        buffer->get_data_revision(buffer))
    pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  uint64_t to_seek =
    (trivial_line_reader->is_terminated) ?
       trivial_line_reader->buffer_offset :
       trivial_line_reader->buffer_offset + 1;

  to_seek = buffer->seek(buffer, to_seek);

  pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

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

  return buffer->is_iterator_end(buffer, &trivial_line_reader->buffer_iterator);
}

/*******************************************************************************
 */
void pb_trivial_line_reader_terminate_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;

  trivial_line_reader->is_terminated = true;
}

/*******************************************************************************
 */
void pb_trivial_line_reader_reset(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_buffer *buffer = trivial_line_reader->line_reader.buffer;

  buffer->get_iterator(buffer, &trivial_line_reader->buffer_iterator);

  trivial_line_reader->buffer_data_revision = buffer->get_data_revision(buffer);

  trivial_line_reader->buffer_offset = 0;
  trivial_line_reader->page_offset = 0;

  trivial_line_reader->has_line = false;
  trivial_line_reader->has_cr = false;
  trivial_line_reader->is_terminated = false;
}

/*******************************************************************************
 */
void pb_trivial_line_reader_destroy(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  const struct pb_allocator *allocator =
    trivial_line_reader->line_reader.allocator;

  pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  allocator->free(
    allocator,
    pb_alloc_type_struct,
    trivial_line_reader, sizeof(struct pb_trivial_line_reader));
}
