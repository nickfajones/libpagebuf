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

  page->data_vec.base = src_page->data_vec.base + src_off;
  page->data_vec.len = len;
  page->data = src_page->data;
  page->prev = NULL;
  page->next = NULL;

  return page;
}

#if 0
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
#endif

void pb_page_destroy(struct pb_page *page,
    const struct pb_allocator *allocator) {
  pb_data_put(page->data);

  page->data_vec.base = NULL;
  page->data_vec.len = 0;
  page->data = NULL;
  page->prev = NULL;
  page->next = NULL;

  allocator->free(
    pb_alloc_type_struct, page, sizeof(struct pb_page), allocator);
}



/*******************************************************************************
 */
static struct pb_list_strategy pb_trivial_list_strategy = {
  .page_size = PB_TRIVIAL_LIST_DEFAULT_PAGE_SIZE,
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

struct pb_list *pb_trivial_list_create_with_strategy(
    const struct pb_list_strategy *strategy) {
  return
    pb_trivial_list_create_with_strategy_with_alloc(
      strategy, pb_get_trivial_allocator());
}

struct pb_list *pb_trivial_list_create_with_alloc(
    const struct pb_allocator *allocator) {
  return
    pb_trivial_list_create_with_strategy_with_alloc(
      pb_get_trivial_list_strategy(), allocator);
}

struct pb_list *pb_trivial_list_create_with_strategy_with_alloc(
    const struct pb_list_strategy *strategy,
    const struct pb_allocator *allocator) {
  struct pb_trivial_list *trivial_list =
    allocator->alloc(
      pb_alloc_type_struct, sizeof(struct pb_trivial_list), allocator);
  if (!trivial_list)
    return NULL;

  trivial_list->list.strategy.page_size = strategy->page_size;
  trivial_list->list.strategy.clone_on_write = strategy->clone_on_write;
  trivial_list->list.strategy.fragment_as_target = strategy->fragment_as_target;

  trivial_list->list.get_data_size = &pb_trivial_list_get_data_size;
  trivial_list->list.get_data_revision = &pb_trivial_list_get_data_revision;

  trivial_list->list.insert = &pb_trivial_list_insert;
  trivial_list->list.reserve = &pb_trivial_list_reserve;

  trivial_list->list.seek = &pb_trivial_list_seek;
  trivial_list->list.rewind = &pb_trivial_list_rewind;

  trivial_list->list.get_iterator = &pb_trivial_list_get_iterator;
  trivial_list->list.get_iterator_end = &pb_trivial_list_get_iterator_end;
  trivial_list->list.is_iterator_end = &pb_trivial_list_is_iterator_end;

  trivial_list->list.iterator_next = &pb_trivial_list_iterator_next;
  trivial_list->list.iterator_prev = &pb_trivial_list_iterator_prev;

  trivial_list->list.write_data = &pb_trivial_list_write_data;
  trivial_list->list.write_list = &pb_trivial_list_write_list;

  trivial_list->list.overwrite_data = &pb_trivial_list_overwrite_data;

  trivial_list->list.read_data = &pb_trivial_list_read_data;

  trivial_list->list.clear = &pb_trivial_list_clear;
  trivial_list->list.destroy = &pb_trivial_list_destroy;

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
uint64_t pb_trivial_list_insert(struct pb_list * const list,
    struct pb_page * const page,
    struct pb_list_iterator * const list_iterator,
    size_t offset) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  while ((offset > 0) &&
         (offset >= list_iterator->page->data_vec.len)) {
    trivial_list->list.iterator_next(&trivial_list->list, list_iterator);

    if (!trivial_list->list.is_iterator_end(&trivial_list->list, list_iterator))
      offset -= list_iterator->page->data_vec.len;
    else
      offset = 0;
  }

  struct pb_page *page1;
  struct pb_page *page2;

  if (offset != 0) {
    page1 = list_iterator->page;
    page2 =
      pb_page_transfer(
        page1, page1->data_vec.len, 0,
        trivial_list->list.allocator);
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
    page1 = list_iterator->page->prev;
    page2 = list_iterator->page;
  }

  page->prev = page1;
  page->next = page2;

  page1->next = page;
  page2->prev = page;

  trivial_list->data_size += page->data_vec.len;

  return page->data_vec.len;
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_reserve(struct pb_list * const list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  uint64_t reserved = 0;

  while (len > 0) {
    uint64_t reserve_len =
      (trivial_list->list.strategy.page_size != 0) ?
      (trivial_list->list.strategy.page_size < len) ?
       trivial_list->list.strategy.page_size : len :
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

    struct pb_list_iterator end_itr;
    trivial_list->list.get_iterator_end(&trivial_list->list, &end_itr);

    len -= reserve_len;
    reserved +=
      trivial_list->list.insert(
        &trivial_list->list, page, &end_itr, 0);
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
         (!trivial_list->list.is_iterator_end(&trivial_list->list, &itr))) {
    uint64_t seek_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    itr.page->data_vec.base += seek_len;
    itr.page->data_vec.len -= seek_len;

    if (itr.page->data_vec.len == 0) {
      struct pb_page *page = itr.page;

      trivial_list->list.iterator_next(&trivial_list->list, &itr);

      page->prev->next = itr.page;
      itr.page->prev = page->prev;

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
      (trivial_list->list.strategy.page_size != 0) ?
      (trivial_list->list.strategy.page_size < len) ?
       trivial_list->list.strategy.page_size : len :
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

    struct pb_list_iterator itr;
    trivial_list->list.get_iterator_end(&trivial_list->list, &itr);

    len -= reserve_len;
    rewinded +=
      trivial_list->list.insert(
        &trivial_list->list, page, &itr, 0);
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

void pb_trivial_list_get_iterator_end(struct pb_list * const list,
    struct pb_list_iterator * const iterator) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  iterator->page = &trivial_list->page_end;
}

bool pb_trivial_list_is_iterator_end(struct pb_list * const list,
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

void pb_trivial_list_iterator_prev(struct pb_list * const list,
    struct pb_list_iterator * const iterator) {
  iterator->page = iterator->page->prev;
}

/*******************************************************************************
 * fragment_as_target: false,true
 */
uint64_t pb_trivial_list_write_data(struct pb_list * const list,
    const uint8_t *buf,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator_end(&trivial_list->list, &itr);
  trivial_list->list.iterator_prev(&trivial_list->list, &itr);

  len = trivial_list->list.reserve(&trivial_list->list, len);

  if (!trivial_list->list.is_iterator_end(&trivial_list->list, &itr)) {
    trivial_list->list.iterator_next(&trivial_list->list, &itr);
  } else {
    trivial_list->list.get_iterator(&trivial_list->list, &itr);
  }

  uint64_t written = 0;

  while (len > 0) {
    uint64_t write_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    len -= write_len;
    written += write_len;

    trivial_list->list.iterator_next(&trivial_list->list, &itr);
  }

  return written;
}

/*******************************************************************************
 * clone_on_write: false
 * fragment_as_target: false
 */
static
#ifdef NDEBUG
inline
#endif
uint64_t pb_trivial_list_write_list1(struct pb_list * const list,

    struct pb_list * const src_list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator_end(&trivial_list->list, &itr);

  struct pb_list_iterator src_itr;
  src_list->get_iterator(src_list, &src_itr);

  uint64_t written = 0;

  while ((len > 0) &&
         (!src_list->is_iterator_end(src_list, &src_itr))) {
    uint64_t write_len =
      (src_itr.page->data_vec.len < len) ?
       src_itr.page->data_vec.len : len;

    struct pb_page *page =
      pb_page_transfer(
        src_itr.page, write_len, 0, trivial_list->list.allocator);
    if (!page)
      return written;

    trivial_list->list.insert(&trivial_list->list, page, &itr, 0);

    len -= write_len;
    written += write_len;

    src_list->iterator_next(src_list, &src_itr);
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
uint64_t pb_trivial_list_write_list2(struct pb_list * const list,
    struct pb_list * const src_list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator_end(&trivial_list->list, &itr);

  struct pb_list_iterator src_itr;
  src_list->get_iterator(src_list, &src_itr);

  uint64_t written = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!src_list->is_iterator_end(src_list, &src_itr))) {
    uint64_t write_len =
      (trivial_list->list.strategy.page_size != 0) ?
      (trivial_list->list.strategy.page_size < len) ?
       trivial_list->list.strategy.page_size : len :
       len;

    write_len =
      (src_itr.page->data_vec.len - src_offset < write_len) ?
       src_itr.page->data_vec.len - src_offset : write_len;

    struct pb_page *page =
      pb_page_transfer(
        src_itr.page, write_len, src_offset, trivial_list->list.allocator);
    if (!page)
      return written;

    trivial_list->list.insert(&trivial_list->list, page, &itr, 0);

    len -= write_len;
    written += write_len;
    src_offset += write_len;

    if (src_offset == src_itr.page->data_vec.len) {
      src_list->iterator_next(src_list, &src_itr);

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
uint64_t pb_trivial_list_write_list3(struct pb_list * const list,
    struct pb_list * const src_list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator_end(&trivial_list->list, &itr);
  trivial_list->list.iterator_prev(&trivial_list->list, &itr);

  struct pb_list_iterator src_itr;
  src_list->get_iterator(src_list, &src_itr);

  uint64_t written = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!src_list->is_iterator_end(src_list, &src_itr))) {
    uint64_t write_len =
      (trivial_list->list.strategy.page_size != 0) ?
      (trivial_list->list.strategy.page_size < len) ?
       trivial_list->list.strategy.page_size : len :
       len;

    write_len =
      (src_itr.page->data_vec.len - src_offset < write_len) ?
       src_itr.page->data_vec.len - src_offset : write_len;

    write_len = trivial_list->list.reserve(&trivial_list->list, write_len);

    if (!trivial_list->list.is_iterator_end(&trivial_list->list, &itr)) {
      trivial_list->list.iterator_next(&trivial_list->list, &itr);
    } else {
      trivial_list->list.get_iterator(&trivial_list->list, &itr);
    }

    memcpy(
      itr.page->data_vec.base,
      src_itr.page->data_vec.base + src_offset,
      write_len);

    len -= write_len;
    written += write_len;
    src_offset += write_len;

    if (src_offset == src_itr.page->data_vec.len) {
      src_list->iterator_next(src_list, &src_itr);

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
uint64_t pb_trivial_list_write_list4(struct pb_list * const list,
    struct pb_list * const src_list,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator_end(&trivial_list->list, &itr);
  trivial_list->list.iterator_prev(&trivial_list->list, &itr);

  struct pb_list_iterator src_itr;
  src_list->get_iterator(src_list, &src_itr);

  len = trivial_list->list.reserve(&trivial_list->list, len);

  if (!trivial_list->list.is_iterator_end(&trivial_list->list, &itr)) {
    trivial_list->list.iterator_next(&trivial_list->list, &itr);
  } else {
    trivial_list->list.get_iterator(&trivial_list->list, &itr);
  }

  uint64_t written = 0;
  size_t offset = 0;
  size_t src_offset = 0;

  while ((len > 0) &&
         (!trivial_list->list.is_iterator_end(&trivial_list->list, &itr)) &&
         (!src_list->is_iterator_end(src_list, &src_itr))) {
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
      trivial_list->list.iterator_next(&trivial_list->list, &itr);

      offset = 0;
    }

    if (src_offset == src_itr.page->data_vec.len) {
      src_list->iterator_next(src_list, &src_itr);

      src_offset = 0;
    }
  }

  return written;
}

uint64_t pb_trivial_list_write_list(struct pb_list * const list,
    struct pb_list * const src_list,
    uint64_t len) {
  if ((!list->strategy.clone_on_write) &&
      (!list->strategy.fragment_as_target)) {
    return pb_trivial_list_write_list1(list, src_list, len);
  } else if ((!list->strategy.clone_on_write) &&
             (list->strategy.fragment_as_target)) {
    return pb_trivial_list_write_list2(list, src_list, len);
  } else if ((list->strategy.clone_on_write) &&
             (!list->strategy.fragment_as_target)) {
    return pb_trivial_list_write_list3(list, src_list, len);
  }
  /*else if ((list->strategy->clone_on_write) &&
             (list->strategy->fragment_as_target)) {*/
  return pb_trivial_list_write_list4(list, src_list, len);
}

/*******************************************************************************
 */
uint64_t pb_trivial_list_overwrite_data(struct pb_list * const list,
    const uint8_t *buf,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  ++trivial_list->data_revision;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator(&trivial_list->list, &itr);

  uint64_t written = 0;

  while ((len > 0) &&
         (!trivial_list->list.is_iterator_end(&trivial_list->list, &itr))) {
    uint64_t write_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(itr.page->data_vec.base, buf + written, write_len);

    len -= write_len;
    written += write_len;

    trivial_list->list.iterator_next(&trivial_list->list, &itr);
  }

  return written;
}

uint64_t pb_trivial_list_read_data(struct pb_list * const list,
    uint8_t * const buf,
    uint64_t len) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  struct pb_list_iterator itr;
  trivial_list->list.get_iterator(&trivial_list->list, &itr);

  uint64_t readed = 0;

  while ((len > 0) &&
         (!trivial_list->list.is_iterator_end(&trivial_list->list, &itr))) {
    size_t read_len =
      (itr.page->data_vec.len < len) ?
       itr.page->data_vec.len : len;

    memcpy(buf + readed, itr.page->data_vec.base, read_len);\

    len -= read_len;
    readed += read_len;

    trivial_list->list.iterator_next(&trivial_list->list, &itr);
  }

  return readed;
}

/*******************************************************************************
 */
void pb_trivial_list_clear(struct pb_list * const list) {
  struct pb_trivial_list *trivial_list = (struct pb_trivial_list*)list;

  uint64_t data_size = trivial_list->list.get_data_size(&trivial_list->list);

  trivial_list->list.seek(&trivial_list->list, data_size);
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
         (!list->is_iterator_end(list, itr))) {
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

  if (list->is_iterator_end(list, itr)) {
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

  if (list->is_iterator_end(list, &trivial_line_reader->list_iterator))
    list->iterator_prev(list, &trivial_line_reader->list_iterator);

  while (!list->is_iterator_end(list, &trivial_line_reader->list_iterator)) {
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

  if (list->is_iterator_end(list, &trivial_line_reader->list_iterator) &&
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

  uint64_t getted = 0;

  uint64_t line_len =
    trivial_line_reader->line_reader.get_line_len(
      &trivial_line_reader->line_reader);

  struct pb_list_iterator itr;
  list->get_iterator(list, &itr);

  while ((len > 0) &&
         (line_len > 0) &&
         (!list->is_iterator_end(list, &itr))) {
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

    list->iterator_next(list, &itr);
  }

  return getted;
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
