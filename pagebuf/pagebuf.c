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
static void *pb_trivial_alloc(enum pb_allocator_type type, size_t size,
    const struct pb_allocator *allocator) {
  void *obj = malloc(size);
  if (!obj)
    return NULL;

  if (type == pb_alloc_type_struct)
    memset(obj, 0, size);

  return obj;
};

static void pb_trivial_free(enum pb_allocator_type type, void *obj, size_t size,
    const struct pb_allocator *allocator) {
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
    allocator->alloc(pb_alloc_type_struct, sizeof(struct pb_data), allocator);
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
    allocator->alloc(pb_alloc_type_struct, sizeof(struct pb_data), allocator);
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
    allocator->alloc(pb_alloc_type_struct, sizeof(struct pb_data), allocator);
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
      pb_alloc_type_struct, data->data_vec.base, data->data_vec.len, allocator);

  allocator->free(
    pb_alloc_type_struct, data, sizeof(struct pb_data), allocator);
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
    allocator->alloc(pb_alloc_type_struct, sizeof(struct pb_page), allocator);
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
    allocator->alloc(pb_alloc_type_struct, sizeof(struct pb_page), allocator);
  if (!page)
    return NULL;

  pb_data_get(src_page->data);

  page->data_vec.base = src_page->data_vec.base;
  page->data_vec.len = src_page->data_vec.len;
  page->data = src_page->data;
  page->prev = NULL;
  page->next = NULL;

  return page;
}

struct pb_page *pb_page_clone(const struct pb_page *src_page,
    uint8_t * const buf, size_t len, size_t src_off,
    const struct pb_allocator *allocator) {
  struct pb_page *page =
    allocator->alloc(pb_alloc_type_struct, sizeof(struct pb_page), allocator);
  if (!page)
    return NULL;

  page->data = pb_data_clone(buf, len, src_off, src_page->data, allocator);
  if (!page->data) {
    allocator->free(
      pb_alloc_type_struct, page, sizeof(struct pb_page), allocator);

    return NULL;
  }

  page->data_vec.base = page->data->data_vec.base;
  page->data_vec.len = page->data->data_vec.len;
  page->prev = NULL;
  page->next = NULL;

  return page;
}

void pb_page_destroy(struct pb_page *page,
    const struct pb_allocator *allocator) {
  page->data_vec.base = NULL;
  page->data_vec.len = 0;
  page->data = NULL;
  page->prev = NULL;
  page->next = NULL;

  pb_data_put(page->data);

  allocator->free(
    pb_alloc_type_struct, page, sizeof(struct pb_page), allocator);
}



/*******************************************************************************
 */
#define PB_TRIVIAL_LIST_PAGE_SIZE                         4096



/*******************************************************************************
 */
static struct pb_list_strategy pb_trivial_list_strategy = {
  .page_size = PB_TRIVIAL_LIST_PAGE_SIZE,
  .clone_on_write = false,
  .fragment_as_target = false,
};

const struct pb_list_strategy *pb_get_trivial_list_strategy(void) {
  return &pb_trivial_list_strategy;
}



/*******************************************************************************
 */
struct pb_trivial_list {
  struct pb_list list;

  struct pb_page page_end;

  uint64_t data_revision;
  uint64_t data_size;
};

/*******************************************************************************
 */
struct pb_list* pb_trivial_list_create(void) {
  return
    pb_trivial_list_create_with_strategy_with_alloc(
      pb_get_trivial_list_strategy(), pb_get_trivial_allocator());
}

struct pb_list *pb_trivial_list_create_with_alloc(
    const struct pb_allocator *allocator) {
  return
    pb_trivial_list_create_with_strategy_with_alloc(
      pb_get_trivial_list_strategy(), allocator);
}

struct pb_list *pb_trivial_list_create_with_strategy(
    const struct pb_list_strategy *strategy) {
  return
    pb_trivial_list_create_with_strategy_with_alloc(
      strategy, pb_get_trivial_allocator());
}

struct pb_list *pb_trivial_list_create_with_strategy_with_alloc(
    const struct pb_list_strategy *strategy,
    const struct pb_allocator *allocator) {
  struct pb_trivial_list *trivial_list =
    allocator->alloc(
      pb_alloc_type_struct, sizeof(struct pb_trivial_list), allocator);
  if (!trivial_list)
    return NULL;

  trivial_list->list.get_data_size = &pb_trivial_list_get_data_size;
  trivial_list->list.get_data_revision = &pb_trivial_list_get_data_revision;

  trivial_list->list.reserve = &pb_trivial_list_reserve;

  trivial_list->list.seek = &pb_trivial_list_seek;
  trivial_list->list.rewind = &pb_trivial_list_rewind;

  trivial_list->list.get_iterator = &pb_trivial_list_get_iterator;
  trivial_list->list.iterator_next = &pb_trivial_list_iterator_next;
  trivial_list->list.iterator_end = &pb_trivial_list_iterator_end;

  trivial_list->list.write_data = &pb_trivial_list_write_data;
  trivial_list->list.write_list = &pb_trivial_list_write_list;

  trivial_list->list.overwrite_data = &pb_trivial_list_overwrite_data;

  trivial_list->list.clear = &pb_trivial_list_clear;
  trivial_list->list.destroy = &pb_trivial_list_destroy;

  trivial_list->list.strategy = strategy;
  trivial_list->list.allocator = allocator;

  trivial_list->page_end.prev = &trivial_list->page_end;
  trivial_list->page_end.next = &trivial_list->page_end;

  trivial_list->data_revision = 0;
  trivial_list->data_size = 0;

  return &trivial_list->list;
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_get_data_size(struct pb_list * const list) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  return trivial_list->data_size;
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_get_data_revision(struct pb_list * const list) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  return trivial_list->data_revision;
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_reserve(struct pb_list * const list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  uint64_t reserved = 0;

  while (len > 0) {
    uint64_t reserve_len =
      (trivial_list->list.strategy->page_size != 0) ?
      (trivial_list->list.strategy->page_size < len) ?
       trivial_list->list.strategy->page_size : len :
       len;

    void *buf =
      trivial_list->list.allocator->alloc(
        pb_alloc_type_region, reserve_len, trivial_list->list.allocator);
    if (!buf)
      return reserved;

    struct pb_data *data =
      pb_data_create(buf, reserve_len, trivial_list->list.allocator);
    if (!data) {
      trivial_list->list.allocator->free(
        pb_alloc_type_region, buf, reserve_len, trivial_list->list.allocator);

      return reserved;
    }

    struct pb_page *page =
      pb_page_create(data, trivial_list->list.allocator);
    if (!page) {
      pb_data_put(data);

      return reserved;
    }

    pb_data_put(data);

    page->prev = trivial_list->page_end.prev;
    page->next = &trivial_list->page_end;

    trivial_list->page_end.prev->next = page;
    trivial_list->page_end.prev = page;

    len -= reserve_len;
    reserved += reserve_len;

    trivial_list->data_size += reserve_len;
  }

  return reserved;
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_seek(struct pb_list * const list, uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  ++trivial_list->data_revision;

  uint64_t seeked = 0;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator(&trivial_list->list, &itr);

  while ((len > 0) &&
         (!trivial_list->list.iterator_end(&trivial_list->list, &itr))) {
    uint64_t seek_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    itr.page->data_vec.base += seek_len;
    itr.page->data_vec.len -= seek_len;

    if (itr.page->data_vec.len == 0) {
      struct pb_page *page = itr.page;

      trivial_list->list.iterator_next(&trivial_list->list, &itr);

      trivial_list->page_end.next->prev = &trivial_list->page_end;
      trivial_list->page_end.next = page->next;

      page->prev = NULL;
      page->next = NULL;

      pb_page_destroy(page, trivial_list->list.allocator);
    }

    len -= seek_len;
    seeked += seek_len;

    trivial_list->data_size -= seek_len;
  }

  return seeked;
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_rewind(struct pb_list * const list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  ++trivial_list->data_revision;

  uint64_t rewinded = 0;

  while (len > 0) {
    uint64_t reserve_len =
      (trivial_list->list.strategy->page_size != 0) ?
      (trivial_list->list.strategy->page_size < len) ?
       trivial_list->list.strategy->page_size : len :
       len;

    void *buf =
      trivial_list->list.allocator->alloc(
        pb_alloc_type_region, reserve_len, trivial_list->list.allocator);
    if (!buf)
      return rewinded;

    struct pb_data *data =
      pb_data_create(buf, reserve_len, trivial_list->list.allocator);
    if (!data) {
      trivial_list->list.allocator->free(
        pb_alloc_type_region, buf, reserve_len, trivial_list->list.allocator);

      return rewinded;
    }

    struct pb_page *page =
      pb_page_create(data, trivial_list->list.allocator);
    if (!page) {
      pb_data_put(data);

      return rewinded;
    }

    pb_data_put(data);

    page->prev = trivial_list->page_end.prev;
    page->next = trivial_list->page_end.next;

    trivial_list->page_end.next->prev = page;
    trivial_list->page_end.next = page;

    len -= reserve_len;
    rewinded += reserve_len;

    trivial_list->data_size += reserve_len;
  }

  return rewinded;
}

/*******************************************************************************
 */
void pb_trivial_list_get_iterator(struct pb_list * const list,
    struct pb_list_iterator * const iterator) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  iterator->page = trivial_list->page_end.next;
}

/*******************************************************************************
 */
bool pb_trivial_list_iterator_end(struct pb_list * const list,
    struct pb_list_iterator * const iterator) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  return (iterator->page == &trivial_list->page_end);
}

/*******************************************************************************
 */
void pb_trivial_list_iterator_next(struct pb_list * const list,
    struct pb_list_iterator * const iterator) {
  iterator->page = iterator->page->next;
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_write_data(struct pb_list * const list,
    const uint8_t *buf,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  struct pb_list_iterator itr;

  if (trivial_list->page_end.next != &trivial_list->page_end) {
    itr.page = trivial_list->page_end.prev;
    pb_trivial_list_reserve(&trivial_list->list, len);
  } else {
    pb_trivial_list_reserve(&trivial_list->list, len);
    itr.page = trivial_list->page_end.next;
  }

  uint64_t written = 0;
  uint64_t offset = 0;

  while ((len > 0) &&
         (!trivial_list->list.iterator_end(&trivial_list->list, &itr))) {
    uint64_t write_len = len;

    if (write_len > itr.page->data_vec.len)
      write_len = itr.page->data_vec.len;

    memcpy(itr.page->data_vec.base + offset, buf + written, write_len);

    len -= write_len;
    written += write_len;
    offset += write_len;

    trivial_list->data_size += write_len;

    if ((offset + write_len) == itr.page->data_vec.len) {
      trivial_list->list.iterator_next(&trivial_list->list, &itr);

      offset = 0;
    }
  }

  return written;
}

uint64_t pb_trivial_list_write_data2(struct pb_list * const list,
    const uint8_t *buf,
    uint64_t len);

uint64_t pb_trivial_list_write_data2(struct pb_list * const list,
    const uint8_t *buf,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  struct pb_list_iterator itr;

  if (trivial_list->page_end.next != &trivial_list->page_end) {
    itr.page = trivial_list->page_end.prev;
    pb_trivial_list_reserve(&trivial_list->list, len);
  } else {
    pb_trivial_list_reserve(&trivial_list->list, len);
    itr.page = trivial_list->page_end.next;
  }

  uint64_t written = 0;
  uint64_t offset = 0;

  while ((len > 0) &&
         (!trivial_list->list.iterator_end(&trivial_list->list, &itr))) {
    uint64_t write_len = len;

    if (write_len > itr.page->data_vec.len)
      write_len = itr.page->data_vec.len;

    if ((trivial_list->list.strategy->page_size != 0) &&
          (write_len > trivial_list->list.strategy->page_size))
        write_len = trivial_list->list.strategy->page_size;

    memcpy(itr.page->data_vec.base + offset, buf + written, write_len);

    len -= write_len;
    written += write_len;
    offset += write_len;

    trivial_list->data_size += write_len;

    if ((offset + write_len) == itr.page->data_vec.len) {
      trivial_list->list.iterator_next(&trivial_list->list, &itr);

      offset = 0;
    }
  }

  return written;
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_write_list(struct pb_list * const list,
    const struct pb_list * src_list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  uint64_t written = 0;
  uint64_t src_offset = 0;

  struct pb_list_iterator src_itr;
  trivial_list->list.get_iterator(&trivial_list->list, &src_itr);

  while ((len > 0) &&
         (!trivial_list->list.iterator_end(&trivial_list->list, &src_itr))) {
    uint64_t write_len = len;

    if (write_len > src_itr.page->data_vec.len)
      write_len = src_itr.page->data_vec.len;

    struct pb_page *page =
      pb_page_transfer(
        src_itr.page, write_len, 0, trivial_list->list.allocator);
    if (!page)
      return written;

    page->prev = trivial_list->page_end.prev;
    page->next = trivial_list->page_end.prev->next;

    trivial_list->page_end.prev->next = page;
    trivial_list->page_end.prev = page;

    len -= write_len;
    written += write_len;
    src_offset += write_len;

    trivial_list->data_size += write_len;

    trivial_list->list.iterator_next(&trivial_list->list, &src_itr);

    src_offset = 0;
  }

  return written;
}

uint64_t pb_trivial_list_write_list2(struct pb_list * const list,
    const struct pb_list * src_list,
    uint64_t len);

uint64_t pb_trivial_list_write_list2(struct pb_list * const list,
    const struct pb_list * src_list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  uint64_t written = 0;
  uint64_t src_offset = 0;

  struct pb_list_iterator src_itr;
  trivial_list->list.get_iterator(&trivial_list->list, &src_itr);

  while ((len > 0) &&
         (!trivial_list->list.iterator_end(&trivial_list->list, &src_itr))) {
    uint64_t write_len = len;

    if (write_len > src_itr.page->data_vec.len)
      write_len = src_itr.page->data_vec.len;

    if ((trivial_list->list.strategy->page_size != 0) &&
        (write_len > trivial_list->list.strategy->page_size))
      write_len = trivial_list->list.strategy->page_size;

    uint8_t *buf =
      trivial_list->list.allocator->alloc(
        pb_alloc_type_region, write_len, trivial_list->list.allocator);
    if (!buf)
      return written;

    struct pb_page *page =
      pb_page_clone(
        src_itr.page, buf, write_len, src_offset, trivial_list->list.allocator);
    if (!page) {
      trivial_list->list.allocator->free(
        pb_alloc_type_region, buf, write_len, trivial_list->list.allocator);

      return written;
    }

    page->prev = trivial_list->page_end.prev;
    page->next = trivial_list->page_end.prev->next;

    trivial_list->page_end.prev->next = page;
    trivial_list->page_end.prev = page;

    len -= write_len;
    written += write_len;
    src_offset += write_len;

    trivial_list->data_size += write_len;

    if ((src_offset + write_len) == src_itr.page->data_vec.len) {
      trivial_list->list.iterator_next(&trivial_list->list, &src_itr);

      src_offset = 0;
    }
  }

  return written;
}


/*******************************************************************************
 */
uint64_t pb_trivial_list_overwrite_data(struct pb_list * const list,
    const uint8_t *buf,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  ++trivial_list->data_revision;

  struct pb_list_iterator itr;
  itr.page = trivial_list->page_end.next;

  uint64_t written = 0;

  while ((len > 0) &&
         (!trivial_list->list.iterator_end(&trivial_list->list, &itr))) {
    uint64_t write_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    trivial_list->list.iterator_next(&trivial_list->list, &itr);

    len -= write_len;
    written += write_len;
  }

  return written;
}

/*******************************************************************************
 */
void pb_trivial_list_clear(struct pb_list * const list) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  ++trivial_list->data_revision;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator(&trivial_list->list, &itr);

  while (!trivial_list->list.iterator_end(&trivial_list->list, &itr)) {
    struct pb_page *page = itr.page;

    trivial_list->list.iterator_next(&trivial_list->list, &itr);

    trivial_list->page_end.next->prev = &trivial_list->page_end;
    trivial_list->page_end.next = page->next;

    page->prev = NULL;
    page->next = NULL;

    pb_page_destroy(page, trivial_list->list.allocator);
  }
}

/*******************************************************************************
 */
void pb_trivial_list_destroy(struct pb_list * const list) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  pb_trivial_list_clear(&trivial_list->list);

  const struct pb_allocator *allocator = trivial_list->list.allocator;

  allocator->free(
    pb_alloc_type_struct, trivial_list, sizeof(struct pb_trivial_list),
    allocator);
}



/*******************************************************************************
 */
struct pb_trivial_data_reader {
  struct pb_data_reader data_reader;

  struct pb_list_iterator list_iterator;

  uint64_t list_data_revision;

  uint64_t page_offset;
};
/*******************************************************************************
 */
struct pb_data_reader *pb_trivial_data_reader_create(
    struct pb_list * const list) {
  return
    pb_trivial_data_reader_create_with_alloc(list, pb_get_trivial_allocator());
}

struct pb_data_reader *pb_trivial_data_reader_create_with_alloc(
    struct pb_list * const list, const struct pb_allocator *allocator) {
  struct pb_trivial_data_reader *trivial_data_reader =
    allocator->alloc(
      pb_alloc_type_struct, sizeof(struct pb_trivial_data_reader), allocator);
  if (!trivial_data_reader)
    return NULL;

  trivial_data_reader->data_reader.read = &pb_trivial_data_reader_read;

  trivial_data_reader->data_reader.reset = &pb_trivial_data_reader_reset;
  trivial_data_reader->data_reader.destroy = &pb_trivial_data_reader_destroy;

  trivial_data_reader->data_reader.list = list;
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
  struct pb_list *list = trivial_data_reader->data_reader.list;
  struct pb_list_iterator *itr = &trivial_data_reader->list_iterator;

  if (list->get_data_revision(list) != trivial_data_reader->list_data_revision)
    trivial_data_reader->data_reader.reset(&trivial_data_reader->data_reader);

  if (trivial_data_reader->page_offset == itr->page->data_vec.len)
    list->iterator_next(list, itr);

  uint64_t readed = 0;

  while ((len > 0) &&
         (!list->iterator_end(list, itr))) {
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

    list->iterator_next(list, itr);

    trivial_data_reader->page_offset = 0;
  }

  if (list->iterator_end(list, itr)) {
    list->iterator_prev(list, itr);

    trivial_data_reader->page_offset = itr->page->data_vec.len;
  }

  return readed;
}

void pb_trivial_data_reader_reset(struct pb_data_reader * const data_reader) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  struct pb_list *list = trivial_data_reader->data_reader.list;

  list->get_iterator(list, &trivial_data_reader->list_iterator);

  trivial_data_reader->list_data_revision = list->get_data_revision(list);

  trivial_data_reader->page_offset = 0;
}

void pb_trivial_data_reader_destroy(struct pb_data_reader * const data_reader) {
  struct pb_trivial_data_reader *trivial_data_reader =
    (struct pb_trivial_data_reader*)data_reader;
  const struct pb_allocator *allocator =
    trivial_data_reader->data_reader.allocator;

  allocator->free(
    pb_alloc_type_struct,
    trivial_data_reader, sizeof(struct pb_trivial_data_reader),
    allocator);
}



/*******************************************************************************
 */
struct pb_trivial_line_reader {
  struct pb_line_reader line_reader;

  struct pb_list_iterator list_iterator;

  uint64_t list_data_revision;

  uint64_t list_offset;

  size_t page_offset;

  bool has_line;
  bool has_cr;
  bool is_terminated;
};

/*******************************************************************************
 */
struct pb_line_reader *pb_trivial_line_reader_create(
    struct pb_list *list) {
  return
    pb_trivial_line_reader_create_with_alloc(
      list, pb_get_trivial_allocator());
}

struct pb_line_reader *pb_trivial_line_reader_create_with_alloc(
    struct pb_list *list,
    const struct pb_allocator *allocator) {
  struct pb_trivial_line_reader *trivial_line_reader =
    allocator->alloc(
      pb_alloc_type_struct, sizeof(struct pb_trivial_line_reader), allocator);
  if (!trivial_line_reader)
    return NULL;

  trivial_line_reader->line_reader.has_line = &pb_trivial_line_reader_has_line;
  trivial_line_reader->line_reader.get_line_len =
    &pb_trivial_line_reader_get_line_len;
  trivial_line_reader->line_reader.get_line_data =
    &pb_trivial_line_reader_get_line_data;
  trivial_line_reader->line_reader.seek_line =
    &pb_trivial_line_reader_seek_line;

  trivial_line_reader->line_reader.terminate_line =
    &pb_trivial_line_reader_terminate_line;

  trivial_line_reader->line_reader.destroy = &pb_trivial_line_reader_destroy;

  trivial_line_reader->line_reader.list = list;

  trivial_line_reader->line_reader.allocator = allocator;

  pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  return NULL;
}

/*******************************************************************************
 */
bool pb_trivial_line_reader_has_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_list *list = trivial_line_reader->line_reader.list;

  if (trivial_line_reader->list_data_revision != list->get_data_revision(list))
    pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  if (trivial_line_reader->has_line)
    return true;

  if (list->iterator_end(list, &trivial_line_reader->list_iterator))
    list->iterator_prev(list, &trivial_line_reader->list_iterator);

  while (!list->iterator_end(list, &trivial_line_reader->list_iterator)) {
    const struct pb_data_vec *data_vec =
      &trivial_line_reader->list_iterator.page->data_vec;

    while (trivial_line_reader->page_offset < data_vec->len) {
      if (data_vec->base[trivial_line_reader->page_offset] == '\n')
        return (trivial_line_reader->has_line = true);
      else if (data_vec->base[trivial_line_reader->page_offset] == '\r')
        trivial_line_reader->has_cr = true;
      else
        trivial_line_reader->has_cr = false;

      ++trivial_line_reader->list_offset;
      ++trivial_line_reader->page_offset;
    }

    list->iterator_next(list, &trivial_line_reader->list_iterator);

    trivial_line_reader->page_offset = 0;
  }

  if (list->iterator_end(list, &trivial_line_reader->list_iterator) &&
      trivial_line_reader->is_terminated)
    return (trivial_line_reader->has_line = true);

  return false;
}

/*******************************************************************************
 */
uint64_t pb_trivial_line_reader_get_line_len(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_list *list = trivial_line_reader->line_reader.list;

  if (trivial_line_reader->list_data_revision != list->get_data_revision(list))
    pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  if (!trivial_line_reader->has_cr)
    return trivial_line_reader->list_offset;

  return (trivial_line_reader->list_offset - 1);
}

/*******************************************************************************
 */
uint64_t pb_trivial_line_reader_get_line_data(
    struct pb_line_reader * const line_reader,
    uint8_t * const buf, uint64_t len) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_list *list = trivial_line_reader->line_reader.list;

  if (trivial_line_reader->list_data_revision != list->get_data_revision(list))
    pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  if (!trivial_line_reader->has_line)
    return 0;



  return 0;
}

/*******************************************************************************
 */
uint64_t pb_trivial_line_reader_seek_line(
    struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;
  struct pb_list *list = trivial_line_reader->line_reader.list;

  if (trivial_line_reader->list_data_revision != list->get_data_revision(list))
    pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  if (!trivial_line_reader->has_line)
    return 0;

  uint64_t to_seek = trivial_line_reader->list_offset;
  list->seek(list, to_seek);

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
  struct pb_list *list = trivial_line_reader->line_reader.list;

  list->get_iterator(list, &trivial_line_reader->list_iterator);

  trivial_line_reader->list_data_revision = list->get_data_revision(list);

  trivial_line_reader->list_offset = 0;

  trivial_line_reader->has_line = false;
  trivial_line_reader->has_cr = false;
  trivial_line_reader->is_terminated = false;
}

/*******************************************************************************
 */
void pb_trivial_line_reader_destroy(struct pb_line_reader * const line_reader) {
  struct pb_trivial_line_reader *trivial_line_reader =
    (struct pb_trivial_line_reader*)line_reader;

  pb_trivial_line_reader_reset(&trivial_line_reader->line_reader);

  const struct pb_allocator *allocator =
    trivial_line_reader->line_reader.allocator;

  allocator->free(
    pb_alloc_type_struct,
    trivial_line_reader, sizeof(struct pb_trivial_line_reader),
    allocator);
}



































#if 0

while (itr != end)
  {
  if (*itr =='\n')
    {
    // found a \n character
    m_has_line = true;

    if ((!m_has_cr) && (m_lf_pos > 0)) // provision for first char being linefeed
      {
      // last char was not a \r so we can increment end pos
      ++m_line_end_pos;
      }

    ++m_lf_pos;

    break;
    }

  if (*itr == '\r')
    {
    // found a \r character
    m_has_cr = true;
    }
  else
    {
    m_has_cr = false;
    }

  m_line_end_pos = m_lf_pos;
  ++m_lf_pos;

  ++itr;
  }

return m_has_line;
}

#endif




#if 0
/*******************************************************************************
 */
static void *pb_list_default_alloc_fn(
    struct pb_list_mem_ops *ops, size_t size) {
  return malloc(size);
}

static void pb_list_default_free_fn(
    struct pb_list_mem_ops *ops, void *ptr) {
  free(ptr);
}

static struct pb_list_mem_ops pb_list_default_mem_ops = {
  &pb_list_default_alloc_fn,
  &pb_list_default_free_fn,
};



/*******************************************************************************
 */
void pb_page_list_clear(struct pb_page_list *list) {
  struct pb_page *itr = list->head;

  while (itr) {
    struct pb_page *page = itr;

    itr = itr->next;

    pb_page_destroy(page);
  }

  list->head = NULL;
  list->tail = NULL;
}

uint64_t pb_page_list_get_size(const struct pb_page_list *list) {
  uint64_t size = 0;
  struct pb_page *itr = list->head;

  while (itr) {
    size += itr->len;

    itr = itr->next;
  }

  return size;
}

/*******************************************************************************
 */
bool pb_page_list_prepend_data(
    struct pb_page_list *list, struct pb_data *data) {
  struct pb_page *page = pb_page_create(data);
  if (!page)
    return false;

  pb_page_list_prepend_page(list, page);

  return true;
}

bool pb_page_list_append_data(
    struct pb_page_list *list, struct pb_data *data) {
  struct pb_page *page = pb_page_create(data);
  if (!page)
    return false;

  pb_page_list_append_page(list, page);

  return true;
}

void pb_page_list_prepend_page(
    struct pb_page_list *list, struct pb_page *page) {
  if (list->head == NULL) {
    list->head = page;
    list->tail = page;

    return;
  }

  page->prev = list->head->prev; // sneakily use this append as an insert
  page->next = list->head;
  if (list->head->prev)
    list->head->prev->next = page; // don't forget the prev before the head ;-)
  list->head->prev = page;
  list->head = page;
}

void pb_page_list_append_page(
    struct pb_page_list *list, struct pb_page *page) {
  if (list->head == NULL) {
    page->next = NULL;
    page->prev = NULL;
    list->head = page;
    list->tail = page;

    return;
  }

  page->next = list->tail->next; // sneakily use this append as an insert
  page->prev = list->tail;
  if (list->tail->next)
    list->tail->next->prev = page; // don't forget the next after the tail ;-)
  list->tail->next = page;
  list->tail = page;
}

bool pb_page_list_append_page_copy(
    struct pb_page_list *list, const struct pb_page *src_page) {
  struct pb_page *page = pb_page_copy(src_page);
  if (!page)
    return false;

  pb_page_list_append_page(list, page);

  return true;
}

bool pb_page_list_append_page_clone(
    struct pb_page_list *list, const struct pb_page *src_page) {
  struct pb_page *page = pb_page_clone(src_page);
  if (!page)
    return false;

  pb_page_list_append_page(list, page);

  return true;
}

/*******************************************************************************
 */
uint64_t pb_page_list_reserve(
    struct pb_page_list *list, uint64_t len, uint16_t max_page_len) {
  uint64_t reserved = 0;
  void *bytes;
  struct pb_data *root_data;

  bytes = (*list->mem_ops->alloc_fn)(list->mem_ops, len);
  if (!bytes)
    return reserved;

  root_data = pb_data_create(0, &pb_builtin_allocator);
  if (!root_data) {
    (*list->mem_ops->free_fn)(list->mem_ops, bytes);

    return reserved;
  }

  while (len > 0) {
    uint16_t to_reserve;
    struct pb_data *data;
    bool append_result;

    to_reserve = (len < UINT16_MAX) ? len : UINT16_MAX;
    to_reserve =
      ((max_page_len != 0) && (max_page_len < to_reserve)) ?
        max_page_len : to_reserve;

    data = pb_data_create_data_ref(root_data, reserved, to_reserve);
    if (!data)
      break;

    append_result = pb_page_list_append_data(list, data);
    pb_data_put(data);
    if (!append_result)
      break;

    reserved += to_reserve;
    len -= to_reserve;
  }

  pb_data_put(root_data);

  return reserved;
}

/*******************************************************************************
 */
uint64_t pb_page_list_write_data(
    struct pb_page_list *list, const void *buf, uint64_t len,
    uint16_t max_page_len) {
  uint64_t written = 0;
  uint64_t reserved = 0;
  struct pb_page_list temp_list;
  struct pb_page *itr;

  temp_list.head = NULL;
  temp_list.tail = NULL;
  temp_list.mem_ops = list->mem_ops;

  reserved = pb_page_list_reserve(&temp_list, len, max_page_len);
  itr = temp_list.head;

  while ((len > 0) && (itr)) {
    uint16_t to_write = (len < itr->len) ? len : itr->len;

    memcpy(itr->base, (char*)buf + written, to_write);

    written += to_write;
    len -= to_write;

    itr = itr->next;
  }

  return pb_page_list_push_page_list(list, &temp_list, reserved);
}

uint64_t pb_page_list_write_data_ref(
    struct pb_page_list *list, const void *buf, uint64_t len) {
  uint64_t written = 0;

  while (len > 0) {
    uint16_t to_write = (len < UINT16_MAX) ? len : UINT16_MAX;
    struct pb_data *data;
    bool append_result;

    data = pb_data_create_buf_ref((char*)buf + written, to_write);
    if (!data)
      break;

    append_result = pb_page_list_append_data(list, data);
    pb_data_put(data);
    if (!append_result)
      break;

    written += to_write;
    len -= to_write;
  }

  return written;
}

uint64_t pb_page_list_write_page_list(struct pb_page_list *list,
    const struct pb_page_list *src_list, uint64_t len) {
  uint64_t written = 0;
  struct pb_page *itr = src_list->head;

  while ((len > 0) && (itr)) {
    uint16_t to_write = (len < itr->len) ? len : itr->len;
    struct pb_page *itr_next = itr->next;

    if (!pb_page_list_append_page_copy(list, itr))
      break;

    list->tail->len = to_write;

    written += to_write;
    len -= to_write;

    itr = itr_next;
  }

  return written;
}

/*******************************************************************************
 */
uint64_t pb_page_list_push_page_list(struct pb_page_list *list,
    struct pb_page_list *src_list, uint64_t len) {
  uint64_t pushed = 0;
  struct pb_page *itr = src_list->head;

  while ((len > 0) && (itr)) {
    if (len >= itr->len) {
      // advance the src_list head first because pb_page_list_append changes itr
      src_list->head = itr->next;
      if (src_list->head)
        src_list->head->prev = NULL;
      else
        src_list->tail = NULL;

      pb_page_list_append_page(list, itr);

      pushed += itr->len;
      len -= itr->len;

      itr = src_list->head;
    } else {
      if (!pb_page_list_append_page_copy(list, itr))
        break;

      list->tail->len = len;

      itr->base = (char*)itr->base + len;
      itr->len -= len;

      pushed += len;
      len = 0;
    }
  }

  return pushed;
}

/*******************************************************************************
 */
uint64_t pb_page_list_seek(struct pb_page_list *list, uint64_t len) {
  uint64_t seeked = 0;
  struct pb_page *itr = list->head;

  while ((len > 0) && (itr)) {
    uint16_t to_seek = (len < itr->len) ? len : itr->len;

    itr->base = (char*)itr->base + to_seek;
    itr->len -= to_seek;

    if (itr->len == 0) {
      list->head = itr->next;
      if (list->head)
        list->head->prev = NULL;
      else
        list->tail = NULL;

      pb_page_destroy(itr);
    }

    seeked += to_seek;
    len -= to_seek;

    itr = list->head;
  }

  return seeked;
}

uint64_t pb_page_list_trim(struct pb_page_list *list, uint64_t len) {
  uint64_t trimmed = 0;
  struct pb_page *itr = list->tail;

  while ((len > 0) && (itr)) {
    uint16_t to_trim = (len < itr->len) ? len : itr->len;

    itr->len -= to_trim;

    if (itr->len == 0) {
      list->tail = itr->prev;
      if (list->tail)
        list->tail->next = NULL;
      else
        list->head = NULL;

      pb_page_destroy(itr);
    }

    trimmed += to_trim;
    len -= to_trim;

    itr = list->tail;
  }

  return trimmed;
}

uint64_t pb_page_list_rewind(struct pb_page_list *list, uint64_t len) {
  uint64_t rewinded = 0;

  while (len > 0) {
    uint16_t to_rewind = (len < UINT16_MAX) ? len : UINT16_MAX;
    void *bytes;
    struct pb_data *data;
    bool prepend_result;

    bytes = (*list->mem_ops->alloc_fn)(list->mem_ops, to_rewind);
    if (!bytes)
      break;

    data = pb_data_create(bytes, to_rewind);
    if (!data) {
      (*list->mem_ops->free_fn)(list->mem_ops, bytes);

      break;
    }

    prepend_result = pb_page_list_prepend_data(list, data);
    pb_data_put(data);
    if (!prepend_result)
      break;

    rewinded += to_rewind;
    len -= to_rewind;
  }

  return rewinded;
}

/*******************************************************************************
 */
uint64_t pb_page_list_read_data(
    struct pb_page_list *list, void *buf, uint64_t len) {
  uint64_t readed = 0;
  struct pb_page *itr = list->head;

  while ((len > 0) && (itr)) {
    uint16_t to_read = (len < itr->len) ? len : itr->len;

    memcpy((char*)buf + readed, itr->base, to_read);

    readed += to_read;
    len -= to_read;

    itr = itr->next;
  }

  return readed;
}

/*******************************************************************************
 */
bool pb_page_list_dup(
    struct pb_page_list *list, const struct pb_page_list *src_list,
    uint64_t off, uint64_t len) {
  struct pb_page *itr = src_list->head;

  while ((off > 0) && (itr)) {
    if (off < itr->len)
      break;

    off -= itr->len;

    itr = itr->next;
  }

  while ((len > 0) && (itr)) {
    struct pb_page *itr_next = itr->next;

    if (!pb_page_list_append_page_clone(list, itr))
      return false;

    if (off > 0) {
      list->tail->base = (char*)list->tail->base + off;
      list->tail->len -= off;

      off = 0;
    }

    uint16_t to_retain = (len < list->tail->len) ? len : list->tail->len;

    list->tail->len = to_retain;

    len -= to_retain;

    itr = itr_next;
  }

  return true;
}


uint64_t pb_page_list_insert_page_list(
    struct pb_page_list *list, uint64_t off,
    struct pb_page_list *src_list, uint64_t src_off,
    uint64_t len) {
  uint64_t inserted = 0;
  struct pb_page *itr;
  struct pb_page *new_page;
  struct pb_page *src_itr;
  struct pb_page temp_page;
  struct pb_page_list temp_list;

  itr = list->head;

  while ((off > 0) && (itr)) {
    if (off < itr->len)
      break;

    off -= itr->len;

    itr = itr->next;
  }

  if ((off == 0) && (!itr))
    // offset set beyond end of list, don't insert anything
    return inserted;

  temp_list.mem_ops = list->mem_ops;

  if ((off != 0) && (itr)) {
    // offset falls in the middle of a page, split it
    new_page = pb_page_copy(itr);
    if (!new_page)
      return inserted;

    itr->len = off;

    new_page->base = (char*)new_page->base + off;
    new_page->len -= off;

    temp_list.head = itr;
    temp_list.tail = itr;

    pb_page_list_append_page(&temp_list, new_page);

    // reset temp_list.tail back to itr for following insertion
    temp_list.tail = itr;
  } else if ((off == 0) && (itr)) {
    // insert directly in front of a page, use a temp page in case itr is first
    temp_page.next = itr;

    temp_list.head = &temp_page;
    temp_list.tail = &temp_page;
  } else {
    // offset was exactly to the end of the list, append
    temp_list.head = list->head;
    temp_list.tail = list->tail;
  }

  src_itr = src_list->head;

  while ((src_off > 0) && (src_itr)) {
    if (src_off < src_itr->len)
      break;

    src_off -= src_itr->len;

    src_itr = src_itr->next;
  }

  while ((len > 0) && (src_itr)) {
    struct pb_page *src_itr_next = src_itr->next;

    if (!pb_page_list_append_page_copy(&temp_list, src_itr))
      return inserted;

    if (src_off > 0) {
      temp_list.tail->base = (char*)temp_list.tail->base + src_off;
      temp_list.tail->len -= src_off;

      src_off = 0;
    }

    uint16_t to_insert = (len < temp_list.tail->len) ? len : temp_list.tail->len;

    temp_list.tail->len = to_insert;

    inserted += to_insert;
    len -= to_insert;

    src_itr = src_itr_next;
  }

  return inserted;
}



/*******************************************************************************
 */
void pb_iterator_init(
    const struct pb_page_list *list, struct pb_iterator *iterator,
    bool is_reverse) {
  iterator->vec.base = NULL;
  iterator->vec.len = 0;

  iterator->is_reverse = is_reverse;

  if (!iterator->is_reverse)
    iterator->page = list->head;
  else
    iterator->page = list->tail;
}

bool pb_iterator_is_reverse(const struct pb_iterator *iterator) {
  return iterator->is_reverse;
}

bool pb_iterator_is_valid(const struct pb_iterator *iterator) {
  return (iterator->page != NULL);
}

void pb_iterator_next(struct pb_iterator *iterator) {
  if (!iterator->is_reverse)
    iterator->page = iterator->page->next;
  else
    iterator->page = iterator->page->prev;
}

const struct pb_vec *pb_iterator_get_vec(struct pb_iterator *iterator) {
  if (!iterator->page) {
    iterator->vec.base = NULL;
    iterator->vec.len = 0;

    return &iterator->vec;
  }

  iterator->vec.base = iterator->page->base;
  iterator->vec.len = iterator->page->len;

  return &iterator->vec;
}



/*******************************************************************************
 */
struct pb_buffer *pb_buffer_create() {
  struct pb_buffer *buffer =
    (*pb_list_default_mem_ops.alloc_fn)(
      &pb_list_default_mem_ops, sizeof(struct pb_buffer));
  if (!buffer)
    return NULL;

  memset(buffer, 0, sizeof(struct pb_buffer));

  pb_buffer_clear(buffer);

  buffer->reserve_max_page_len = PB_BUFFER_DEFAULT_PAGE_SIZE;

  buffer->data_list.mem_ops = &pb_list_default_mem_ops;
  buffer->write_list.mem_ops = &pb_list_default_mem_ops;
  buffer->retain_list.mem_ops = &pb_list_default_mem_ops;

  return buffer;
}

/*******************************************************************************
 */
void pb_buffer_destroy(struct pb_buffer *buffer) {
  pb_buffer_clear(buffer);

  buffer->reserve_max_page_len = 0;

  (*buffer->data_list.mem_ops->free_fn)(buffer->data_list.mem_ops, buffer);
}

/*******************************************************************************
 */
void pb_buffer_optimise(struct pb_buffer *buffer) {
  pb_page_list_clear(&buffer->write_list);
  pb_page_list_clear(&buffer->retain_list);
}

void pb_buffer_clear(struct pb_buffer *buffer) {
  pb_buffer_optimise(buffer);

  pb_page_list_clear(&buffer->data_list);

  buffer->data_size = 0;

  buffer->is_data_dirty = false;
}

/*******************************************************************************
 */
uint64_t pb_buffer_get_data_size_ro(const struct pb_buffer *buffer) {
  if (!buffer->is_data_dirty)
    return buffer->data_size;

  return pb_page_list_get_size(&buffer->data_list);
}

uint64_t pb_buffer_get_data_size(struct pb_buffer *buffer) {
  if (!buffer->is_data_dirty)
    return buffer->data_size;

  buffer->data_size = pb_page_list_get_size(&buffer->data_list);
  buffer->is_data_dirty = false;

  return buffer->data_size;
}

bool pb_buffer_is_empty_ro(const struct pb_buffer *buffer) {
  return (pb_buffer_get_data_size_ro(buffer) == 0);
}

bool pb_buffer_is_empty(struct pb_buffer *buffer) {
  return (pb_buffer_get_data_size(buffer) == 0);
}

/*******************************************************************************
 */
uint64_t pb_buffer_reserve(struct pb_buffer *buffer, uint64_t len) {
  uint64_t capacity = 0;

  capacity = pb_page_list_get_size(&buffer->write_list);
  if (capacity >= len)
    return len;

  return
    capacity +
    pb_page_list_reserve(
      &buffer->write_list, len - capacity, buffer->reserve_max_page_len);
}

uint64_t pb_buffer_push(struct pb_buffer *buffer, uint64_t len) {
  uint64_t pushed =
    pb_page_list_push_page_list(&buffer->data_list, &buffer->write_list, len);

  buffer->is_data_dirty = true;

  return pushed;
}

/*******************************************************************************
 */
uint64_t pb_buffer_write_data(
    struct pb_buffer *buffer, const void *buf, uint64_t len) {
  uint64_t written =
    pb_page_list_write_data(
      &buffer->data_list, buf, len, buffer->reserve_max_page_len);

  buffer->is_data_dirty = true;

  return written;
}

uint64_t pb_buffer_write_data_ref(
    struct pb_buffer *buffer, const void *buf, uint64_t len) {
  uint64_t written = pb_page_list_write_data_ref(&buffer->data_list, buf, len);

  buffer->is_data_dirty = true;

  return written;
}

uint64_t pb_buffer_write_buf(
    struct pb_buffer *buffer, const struct pb_buffer *src_buffer, uint64_t len) {
  uint64_t written =
    pb_page_list_write_page_list(
      &buffer->data_list, &src_buffer->data_list, len);

  buffer->is_data_dirty = true;

  return written;
}

/*******************************************************************************
 */
void pb_buffer_get_write_iterator(
    struct pb_buffer *buffer, struct pb_iterator *iterator) {
  pb_iterator_init(&buffer->write_list, iterator, false);
}

/*******************************************************************************
 */
uint64_t pb_buffer_seek(struct pb_buffer *buffer, uint64_t len) {
  uint64_t seeked = pb_page_list_seek(&buffer->data_list, len);

  buffer->is_data_dirty = true;

  return seeked;
}

uint64_t pb_buffer_trim(struct pb_buffer *buffer, uint64_t len) {
  uint64_t trimmed = pb_page_list_trim(&buffer->data_list, len);

  buffer->is_data_dirty = true;

  return trimmed;
}

uint64_t pb_buffer_rewind(struct pb_buffer *buffer, uint64_t len) {
  return 0;
}

/*******************************************************************************
 */
uint64_t pb_buffer_read_data(
    struct pb_buffer *buffer, void *buf, uint64_t len) {
  return pb_page_list_read_data(&buffer->data_list, buf, len);
}

/*******************************************************************************
 */
void pb_buffer_get_data_iterator(
    struct pb_buffer *buffer, struct pb_iterator *iterator) {
  pb_iterator_init(&buffer->data_list, iterator, false);
}

/*******************************************************************************
 */
struct pb_buffer *pb_buffer_dup(struct pb_buffer *src_buffer) {
  struct pb_buffer *buffer = pb_buffer_create();
  if (!buffer)
    return NULL;

  if (!pb_page_list_dup(&buffer->data_list, &src_buffer->data_list, 0, 0)) {
    pb_buffer_destroy(buffer);

    return NULL;
  }

  return buffer;
}
struct pb_buffer *pb_buffer_dup_seek(
    struct pb_buffer *src_buffer, uint64_t off) {
  struct pb_buffer *buffer = pb_buffer_create();
  if (!buffer)
    return NULL;

  if (!pb_page_list_dup(&buffer->data_list, &src_buffer->data_list, off, 0)) {
    pb_buffer_destroy(buffer);

    return NULL;
  }

  return buffer;
}
struct pb_buffer *pb_buffer_dup_trim(
    struct pb_buffer *src_buffer, uint64_t len) {
  struct pb_buffer *buffer = pb_buffer_create();
  if (!buffer)
    return NULL;

  if (!pb_page_list_dup(&buffer->data_list, &src_buffer->data_list, 0, len)) {
    pb_buffer_destroy(buffer);

    return NULL;
  }

  return buffer;
}
struct pb_buffer *pb_buffer_dup_sub(
    struct pb_buffer *src_buffer, uint64_t off, uint64_t len) {
  struct pb_buffer *buffer = pb_buffer_create();
  if (!buffer)
    return NULL;

  if (!pb_page_list_dup(&buffer->data_list, &src_buffer->data_list, off, len)) {
    pb_buffer_destroy(buffer);

    return NULL;
  }

  return buffer;
}

/*******************************************************************************
 */
uint64_t pb_buffer_insert_buf(
    struct pb_buffer *buffer, uint64_t off,
    struct pb_buffer *src_buffer, uint64_t src_off,
    uint64_t len) {
  return pb_page_list_insert_page_list(
    &buffer->data_list, off,
    &src_buffer->data_list, src_off,
    len);
}

#endif
