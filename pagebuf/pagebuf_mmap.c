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

#include "pagebuf_mmap.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>


/** Hashmap prepare and import
 */
#define pb_uthash_malloc(sz) \
  (pb_allocator_alloc( \
     mmap_allocator->struct_allocator, pb_alloc_type_struct, sz))
#define pb_uthash_free(ptr,sz) \
  (pb_allocator_free( \
     mmap_allocator->struct_allocator, pb_alloc_type_struct, ptr, sz))
#include "pagebuf_hash.h"



/** Pre declare mmap_allocator.
 */
struct pb_mmap_allocator;



/** The specialised data struct for use with the mmap_allocator
 */
struct pb_mmap_data {
  struct pb_data data;

  struct pb_mmap_allocator *mmap_allocator;

  uint64_t file_offset;

  bool obsolete;

  UT_hash_handle hh;
};



/** Pre declare the data operations factory for mmap_data. */
static const struct pb_data_operations *pb_get_mmap_data_operations(void);



/*******************************************************************************
 */
#define PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE                  4096



/** The specialised allocator that tracks regions backed by a block device file. */
struct pb_mmap_allocator {
  struct pb_allocator allocator;

  const struct pb_allocator *struct_allocator;

  /** Use count, maintained with atomic operations. */
  uint16_t use_count;

  char *file_path;

  int file_fd;

  uint64_t file_head_offset;

  struct pb_mmap_data *data_tree;

  enum pb_mmap_close_action close_action;
};



/*******************************************************************************
 */
static void *pb_mmap_allocator_alloc(const struct pb_allocator *allocator,
    enum pb_allocator_type type, size_t size) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)allocator;

  if (type == pb_alloc_type_struct)
    return pb_allocator_alloc(mmap_allocator->struct_allocator, type, size);

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
struct pb_allocator_operations pb_mmap_allocator_operations = {
  .alloc = &pb_mmap_allocator_alloc,
  .free =&pb_mmap_allocator_free,
};



/*******************************************************************************
 */
static void pb_mmap_allocator_get(struct pb_mmap_allocator *mmap_allocator);
static void pb_mmap_allocator_put(struct pb_mmap_allocator *mmap_allocator);



/*******************************************************************************
 */
static struct pb_mmap_allocator *pb_mmap_allocator_create(const char *file_path,
    enum pb_mmap_open_action open_action,
    enum pb_mmap_close_action close_action,
    const struct pb_allocator *allocator) {
  struct pb_mmap_allocator *mmap_allocator =
    pb_allocator_alloc(
      allocator,
      pb_alloc_type_struct, sizeof(struct pb_mmap_allocator));
  if (!mmap_allocator)
    return NULL;

  mmap_allocator->allocator.operations = &pb_mmap_allocator_operations;

  mmap_allocator->use_count = 1;

  mmap_allocator->struct_allocator = allocator;

  mmap_allocator->file_fd = -1;

  size_t file_path_len = strlen(file_path);

  mmap_allocator->file_path =
    pb_allocator_alloc(
      mmap_allocator->struct_allocator,
      pb_alloc_type_struct, (file_path_len + 1));
  if (!mmap_allocator->file_path) {
    int temp_errno = errno;

    pb_mmap_allocator_put(mmap_allocator);

    errno = temp_errno;

    return NULL;
  }
  memcpy(mmap_allocator->file_path, file_path, file_path_len);
  mmap_allocator->file_path[file_path_len] = '\0';

  int open_flags =
    (open_action == pb_mmap_open_action_append) ?
       O_RDWR|O_APPEND|O_CREAT|O_CLOEXEC :
       O_RDWR|O_APPEND|O_CREAT|O_TRUNC|O_CLOEXEC;

  mmap_allocator->file_fd =
    open(
      mmap_allocator->file_path, open_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
  if (mmap_allocator->file_fd == -1) {
    int temp_errno = errno;

    pb_mmap_allocator_put(mmap_allocator);

    errno = temp_errno;

    return NULL;
  }

  mmap_allocator->file_head_offset = 0;

  mmap_allocator->close_action = close_action;

  return mmap_allocator;
}

/***;****************************************************************************
 */
static uint64_t pb_mmap_allocator_get_file_size(
    struct pb_mmap_allocator * const mmap_allocator) {
  struct stat file_stat;
  memset(&file_stat, 0, sizeof(struct stat));

  if (fstat(mmap_allocator->file_fd, &file_stat) == -1)
    return 0;

  return file_stat.st_size;
  }

static uint64_t pb_mmap_allocator_get_data_size(
    struct pb_mmap_allocator *mmap_allocator) {
  uint64_t file_size = pb_mmap_allocator_get_file_size(mmap_allocator);

  if (file_size < mmap_allocator->file_head_offset) {
    assert(0);

    return 0;
  }

  return (file_size - mmap_allocator->file_head_offset);
}

/*******************************************************************************
 */
static struct pb_mmap_data *pb_mmap_allocator_data_create(
    struct pb_mmap_allocator * const mmap_allocator,
    uint64_t mmap_offset, size_t mmap_len) {
  void *mmap_base =
    mmap64(
      NULL, mmap_len,
      PROT_READ | PROT_WRITE, MAP_SHARED,
      mmap_allocator->file_fd, mmap_offset);
  if (mmap_base == MAP_FAILED)
    return NULL;

  struct pb_mmap_data *mmap_data =
    pb_allocator_alloc(
      mmap_allocator->struct_allocator,
      pb_alloc_type_struct, sizeof(struct pb_mmap_data));
  if (!mmap_data) {
    munmap(mmap_base, mmap_len);

    return NULL;
  }

  mmap_data->data.data_vec.base = mmap_base;
  mmap_data->data.data_vec.len = mmap_len;

  mmap_data->data.responsibility = pb_data_referenced;

  mmap_data->data.use_count = 1;

  mmap_data->data.operations = pb_get_mmap_data_operations();
  mmap_data->data.allocator = &mmap_allocator->allocator;

  mmap_data->mmap_allocator = mmap_allocator;

  mmap_data->file_offset = mmap_offset;

  pb_mmap_allocator_get(mmap_allocator);

  return mmap_data;
}

static void pb_mmap_allocator_data_destroy(
    struct pb_mmap_allocator * const mmap_allocator,
    struct pb_mmap_data * const mmap_data) {
  if (!mmap_data->obsolete)
    PB_HASH_DEL(mmap_allocator->data_tree, mmap_data);

  munmap(pb_data_get_base(&mmap_data->data), pb_data_get_len(&mmap_data->data));

  pb_allocator_free(
    mmap_allocator->struct_allocator,
    pb_alloc_type_struct, mmap_data, sizeof(struct pb_mmap_data));

  pb_mmap_allocator_put(mmap_allocator);
}

/*******************************************************************************
 */
static struct pb_page *pb_mmap_allocator_page_map_forward(
    struct pb_mmap_allocator * const mmap_allocator,
    const struct pb_buffer_iterator *buffer_iterator) {
  struct pb_page *page = buffer_iterator->page;
  struct pb_mmap_data *mmap_data =
    (page) ?
       (struct pb_mmap_data*)page->data :
       NULL;

  uint64_t file_size = pb_mmap_allocator_get_file_size(mmap_allocator);
  uint64_t file_offset =
    (mmap_data) ?
       mmap_data->file_offset +
         ((ptrdiff_t)pb_page_get_base(page) -
          (ptrdiff_t)pb_data_get_base(page->data)) +
         pb_page_get_len(page) :
       mmap_allocator->file_head_offset;
  uint64_t mmap_offset =
    (file_offset / PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE) *
    PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE;
  size_t mmap_len;
  size_t len = PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE;

  if (file_offset == file_size)
    return NULL;

  PB_HASH_FIND_UINT64(mmap_allocator->data_tree, &mmap_offset, mmap_data);
  if (mmap_data) {
    mmap_len = pb_data_get_len(&mmap_data->data);
    if ((mmap_offset + mmap_len) >= (file_offset + len)) {
      // mmap data is as big as it can be, temporarily hold it
      pb_data_get(&mmap_data->data);
    } else if ((mmap_len < PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE) &&
               ((mmap_offset + mmap_len) < file_size)) {
      // mmap data can be extended to meet new end of file
      mmap_len =
        (PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE < (file_size - mmap_offset)) ?
         PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE : (file_size - mmap_offset);

      struct pb_mmap_data *new_mmap_data =
        pb_mmap_allocator_data_create(mmap_allocator, mmap_offset, mmap_len);
      if (!new_mmap_data)
        return NULL;

      PB_HASH_DEL(mmap_allocator->data_tree, mmap_data);
      mmap_data->obsolete = true;

      mmap_data = new_mmap_data;

      PB_HASH_ADD_UINT64(mmap_allocator->data_tree, file_offset, mmap_data);
    }
  } else {
    // create a new mmap data
    mmap_len =
      (PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE < (file_size - mmap_offset)) ?
       PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE : (file_size - mmap_offset);

    mmap_data =
      pb_mmap_allocator_data_create(mmap_allocator, mmap_offset, mmap_len);
    if (!mmap_data)
      return NULL;

    PB_HASH_ADD_UINT64(mmap_allocator->data_tree, file_offset, mmap_data);
  }

  len = (mmap_offset + mmap_len) - file_offset;

  page = pb_page_create(&mmap_data->data, mmap_allocator->struct_allocator);
  if (!page) {
    pb_data_put(&mmap_data->data);

    return NULL;
  }

  // adjust the page data vec
  page->data_vec.base =
    pb_data_get_base_at(&mmap_data->data, (file_offset - mmap_offset));
  page->data_vec.len = len;

  pb_data_put(&mmap_data->data);

  return page;
}

/*******************************************************************************
 */
static struct pb_page *pb_mmap_allocator_page_map_backward(
    struct pb_mmap_allocator * const mmap_allocator,
    const struct pb_buffer_iterator *buffer_iterator) {
  struct pb_page *page = buffer_iterator->page;
  struct pb_mmap_data *mmap_data =
    (page) ?
       (struct pb_mmap_data*)page->data :
       NULL;

  uint64_t file_size = pb_mmap_allocator_get_file_size(mmap_allocator);
  uint64_t file_current_offset =
    (mmap_data) ?
       mmap_data->file_offset +
         ((ptrdiff_t)pb_page_get_base(page) -
          (ptrdiff_t)pb_data_get_base(page->data)) :
       file_size;
  uint64_t file_offset;
  uint64_t mmap_offset =
    (file_current_offset / PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE) *
    PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE;
  size_t mmap_len;
  size_t len = PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE;

  if (file_current_offset <= mmap_allocator->file_head_offset)
    return NULL;

  if (file_current_offset == mmap_offset)
    mmap_offset -= PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE;

  file_offset =
    (mmap_offset > mmap_allocator->file_head_offset) ?
     mmap_offset : mmap_allocator->file_head_offset;

  PB_HASH_FIND_UINT64(mmap_allocator->data_tree, &mmap_offset, mmap_data);
  if (mmap_data) {
    mmap_len = pb_data_get_len(&mmap_data->data);
    if ((mmap_offset + mmap_len) >= (file_offset + len)) {
      // mmap data is as big as it can be, temporarily hold it
      pb_data_get(&mmap_data->data);
    } else if ((mmap_len < PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE) &&
               ((mmap_offset + mmap_len) < file_size)) {
      // mmap data can be extended to meet new end of file
      mmap_len =
        (PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE < (file_size - mmap_offset)) ?
         PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE : (file_size - mmap_offset);

      struct pb_mmap_data *new_mmap_data =
        pb_mmap_allocator_data_create(mmap_allocator, mmap_offset, mmap_len);
      if (!new_mmap_data)
        return NULL;

      PB_HASH_DEL(mmap_allocator->data_tree, mmap_data);
      mmap_data->obsolete = true;

      mmap_data = new_mmap_data;

      PB_HASH_ADD_UINT64(mmap_allocator->data_tree, file_offset, mmap_data);
    }
  } else {
    // create a new mmap data
    mmap_len =
      (PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE < (file_size - mmap_offset)) ?
       PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE : (file_size - mmap_offset);

    mmap_data =
      pb_mmap_allocator_data_create(mmap_allocator, mmap_offset, mmap_len);
    if (!mmap_data)
      return NULL;

    PB_HASH_ADD_UINT64(mmap_allocator->data_tree, file_offset, mmap_data);
  }

  len = file_current_offset - file_offset;

  page = pb_page_create(&mmap_data->data, mmap_allocator->struct_allocator);
  if (!page) {
    pb_data_put(&mmap_data->data);

    return NULL;
  }

  // adjust the page data vec
  page->data_vec.base =
    pb_data_get_base_at(&mmap_data->data, (file_offset - mmap_offset));
  page->data_vec.len = file_current_offset - file_offset;

  pb_data_put(&mmap_data->data);

  return page;
}

/*******************************************************************************
 */
static uint64_t pb_mmap_allocator_extend(
    struct pb_mmap_allocator * const mmap_allocator,
    size_t len) {
  uint64_t file_size = pb_mmap_allocator_get_file_size(mmap_allocator);
  file_size += len;

  if (ftruncate64(mmap_allocator->file_fd, file_size) == -1)
    return 0;

  return len;
}

static uint64_t pb_mmap_allocator_rewind(
    struct pb_mmap_allocator * const mmap_allocator,
    size_t len) {
  uint64_t to_rewind =
    (len < mmap_allocator->file_head_offset) ?
     len : mmap_allocator->file_head_offset;

  mmap_allocator->file_head_offset -= to_rewind;

  return to_rewind;
}

static uint64_t pb_mmap_allocator_seek(
    struct pb_mmap_allocator * const mmap_allocator,
    size_t len) {
  uint64_t file_size = pb_mmap_allocator_get_file_size(mmap_allocator);

  if (len > (file_size - mmap_allocator->file_head_offset))
    len = (file_size - mmap_allocator->file_head_offset);

  mmap_allocator->file_head_offset += len;

  return len;
}

static uint64_t pb_mmap_allocator_trim(
    struct pb_mmap_allocator * const mmap_allocator,
    size_t len) {
  uint64_t file_size = pb_mmap_allocator_get_file_size(mmap_allocator);

  if (len > (file_size - mmap_allocator->file_head_offset))
    len = (file_size - mmap_allocator->file_head_offset);

  if (len == 0)
    return 0;

  uint64_t trimmed = 0;

  while (len > 0) {
    struct pb_mmap_data *mmap_data;

    uint64_t file_offset = file_size;
    uint64_t mmap_offset =
      (file_offset / PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE) *
         PB_MMAP_ALLOCATOR_BASE_MMAP_SIZE;

    size_t trim_len = 0;

    PB_HASH_FIND_UINT64(mmap_allocator->data_tree, &mmap_offset, mmap_data);
    if (mmap_data) {
      size_t mmap_len = pb_data_get_len(&mmap_data->data);

      trim_len = (mmap_len < len) ? mmap_len : len;

      if (trim_len < mmap_len) {
        mmap_len -= trim_len;

        struct pb_mmap_data *new_mmap_data =
          pb_mmap_allocator_data_create(mmap_allocator, mmap_offset, mmap_len);
        if (!new_mmap_data)
          break;

        PB_HASH_DEL(mmap_allocator->data_tree, mmap_data);
        mmap_data->obsolete = true;

        PB_HASH_ADD_UINT64(mmap_allocator->data_tree, file_offset, mmap_data);
        pb_data_get(&mmap_data->data);
      } else {
        PB_HASH_DEL(mmap_allocator->data_tree, mmap_data);
        mmap_data->obsolete = true;
      }

      pb_data_put(&mmap_data->data);
    } else {
      trim_len =
        ((file_offset - mmap_offset) < len) ?
         (file_offset - mmap_offset) : len;
    }

    len -= trim_len;
    trimmed += trim_len;
  }

  if (ftruncate64(mmap_allocator->file_fd, (file_size - trimmed)) == -1)
    trimmed = 0;

  return trimmed;
}

/*******************************************************************************
 */
static uint64_t pb_mmap_allocator_write_data(
    struct pb_mmap_allocator * const mmap_allocator,
    const void *buf, uint64_t len) {
  if (mmap_allocator->file_fd == -1)
    return 0;

  ssize_t written = write(mmap_allocator->file_fd, buf, len);
  if (written < 0)
    written = 0;

  return written;
}

static uint64_t pb_mmap_allocator_write_data_buffer(
    struct pb_mmap_allocator * const mmap_allocator,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  if (mmap_allocator->file_fd == -1)
    return 0;

  struct pb_buffer_iterator src_buffer_iterator;
  pb_buffer_get_iterator(src_buffer, &src_buffer_iterator);

  if (pb_buffer_iterator_is_end(src_buffer, &src_buffer_iterator))
    return 0;

  int iovpos = 0;
  int iovlim = 2;

  struct iovec *iov =
    pb_allocator_alloc(
      mmap_allocator->struct_allocator,
      pb_alloc_type_struct, sizeof(struct iovec) * iovlim);
  if (!iov)
    return 0;

  while ((len > 0) &&
         (!pb_buffer_iterator_is_end(src_buffer, &src_buffer_iterator))) {
    if (iovpos == iovlim) {
      if (iovlim == 1024)
        break;

      iovlim *= 2;

      struct iovec *iov2 =
        pb_allocator_alloc(
          mmap_allocator->struct_allocator,
          pb_alloc_type_struct, sizeof(struct iovec) * iovlim);
      if (!iov2)
        break;

      memcpy(iov2, iov, sizeof(struct iovec) * iovpos);

      pb_allocator_free(
        mmap_allocator->struct_allocator,
        pb_alloc_type_struct,
        iov, sizeof(struct iovec) * iovpos);

      iov = iov2;
    }

    uint64_t iov_len =
      (pb_buffer_iterator_get_len(&src_buffer_iterator) < len) ?
       pb_buffer_iterator_get_len(&src_buffer_iterator) : len;

    iov[iovpos].iov_base = pb_buffer_iterator_get_base(&src_buffer_iterator);
    iov[iovpos].iov_len = iov_len;

    len -= iov_len;
    ++iovpos;

    pb_buffer_iterator_next(src_buffer, &src_buffer_iterator);
  }

  ssize_t written = writev(mmap_allocator->file_fd, iov, iovpos);
  if (written < 0)
    written = 0;

  pb_allocator_free(
    mmap_allocator->struct_allocator,
    pb_alloc_type_struct,
    iov, sizeof(struct iovec) * iovlim);

  return written;
}

static uint64_t pb_mmap_allocator_overwrite_data(
    struct pb_mmap_allocator * const mmap_allocator,
    const void *buf, uint64_t len) {
  return 0;
}

/*******************************************************************************
 */
static void pb_mmap_allocator_clear(
    struct pb_mmap_allocator * const mmap_allocator) {
  uint64_t file_size = pb_mmap_allocator_get_file_size(mmap_allocator);

  mmap_allocator->file_head_offset = file_size;
}



/*******************************************************************************
 */
static void pb_mmap_allocator_get(
    struct pb_mmap_allocator * const mmap_allocator) {
  ++mmap_allocator->use_count;
}

static void pb_mmap_allocator_put(
    struct pb_mmap_allocator * const mmap_allocator) {
  const struct pb_allocator *struct_allocator =
    mmap_allocator->struct_allocator;

  if (--mmap_allocator->use_count != 0)
    return;

  PB_HASH_CLEAR(mmap_allocator->data_tree);

  if (mmap_allocator->file_fd >= 0) {
    if (mmap_allocator->close_action == pb_mmap_close_action_remove) {
      unlink(mmap_allocator->file_path);
    }

    close(mmap_allocator->file_fd);

    mmap_allocator->file_fd = -1;
  }

  if (mmap_allocator->file_path) {
    pb_allocator_free(
      struct_allocator,
      pb_alloc_type_struct,
      mmap_allocator->file_path, strlen(mmap_allocator->file_path) + 1);

    mmap_allocator->file_path = 0;
  }

  pb_allocator_free(
    struct_allocator,
    pb_alloc_type_struct, mmap_allocator, sizeof(struct pb_mmap_allocator));
}






/*******************************************************************************
 */
static void pb_mmap_data_get(struct pb_data * const data) {
  ++data->use_count;
}

static void pb_mmap_data_put(struct pb_data * const data) {
  struct pb_mmap_data *mmap_data = (struct pb_mmap_data*)data;
  struct pb_mmap_allocator *mmap_allocator = mmap_data->mmap_allocator;

  if (--data->use_count != 0)
    return;

  pb_mmap_allocator_data_destroy(mmap_allocator, mmap_data);
}



/*******************************************************************************
 */
static struct pb_data_operations pb_mmap_data_operations = {
  .get = &pb_mmap_data_get,
  .put = &pb_mmap_data_put,
};

static const struct pb_data_operations *pb_get_mmap_data_operations(void) {
  return &pb_mmap_data_operations;
}






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



/** Operations function overrides for mmap buffer. */
static uint64_t pb_mmap_buffer_get_data_size(struct pb_buffer * const buffer);


static void pb_mmap_buffer_get_iterator(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
static void pb_mmap_buffer_get_iterator_end(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
static bool pb_mmap_buffer_iterator_is_end(
                            struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *buffer_iterator);
static bool pb_mmap_buffer_iterator_cmp(struct pb_buffer * const buffer,
                            const struct pb_buffer_iterator *lvalue,
                            const struct pb_buffer_iterator *rvalue);
static void pb_mmap_buffer_iterator_next(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);
static void pb_mmap_buffer_iterator_prev(
                            struct pb_buffer * const buffer,
                            struct pb_buffer_iterator * const buffer_iterator);


static struct pb_page *pb_mmap_buffer_page_create(
                              struct pb_buffer * const buffer,
                              size_t len);

static struct pb_page *pb_mmap_buffer_page_create_ref(
                              struct pb_buffer * const buffer,
                              const uint8_t *buf, size_t len);


static uint64_t pb_mmap_buffer_insert(
                              struct pb_buffer * const buffer,
                              const struct pb_buffer_iterator *buffer_iterator,
                              size_t offset,
                              struct pb_page * const page);
static uint64_t pb_mmap_buffer_seek(struct pb_buffer * const buffer,
                              uint64_t len);
static uint64_t pb_mmap_buffer_extend(
                              struct pb_buffer * const buffer,
                              uint64_t len);
static uint64_t pb_mmap_buffer_rewind(
                              struct pb_buffer * const buffer,
                              uint64_t len);
static uint64_t pb_mmap_buffer_trim(
                              struct pb_buffer * const buffer,
                              uint64_t len);

uint64_t pb_mmap_buffer_write_data(struct pb_buffer * const buffer,
                                   const void *buf,
                                   uint64_t len);
uint64_t pb_mmap_buffer_write_data_ref(
                                   struct pb_buffer * const buffer,
                                   const void *buf,
                                   uint64_t len);
uint64_t pb_mmap_buffer_write_buffer(
                                   struct pb_buffer * const buffer,
                                   struct pb_buffer * const src_buffer,
                                   uint64_t len);
uint64_t pb_mmap_buffer_overwrite_data(
                                   struct pb_buffer * const buffer,
                                   const void *buf,
                                   uint64_t len);


static void pb_mmap_buffer_clear(struct pb_buffer * const buffer);
static void pb_mmap_buffer_destroy(
                                 struct pb_buffer * const buffer);



/*******************************************************************************
 */
static struct pb_buffer_operations pb_mmap_buffer_operations = {
  .get_data_revision = &pb_trivial_buffer_get_data_revision,
  .increment_data_revision = &pb_trivial_buffer_increment_data_revision,

  .get_data_size = &pb_mmap_buffer_get_data_size,

  .get_iterator = &pb_mmap_buffer_get_iterator,
  .get_iterator_end = &pb_mmap_buffer_get_iterator_end,
  .iterator_is_end = &pb_mmap_buffer_iterator_is_end,
  .iterator_cmp = &pb_mmap_buffer_iterator_cmp,
  .iterator_next = &pb_mmap_buffer_iterator_next,
  .iterator_prev = &pb_mmap_buffer_iterator_prev,

  .get_byte_iterator = &pb_trivial_buffer_get_byte_iterator,
  .get_byte_iterator_end = &pb_trivial_buffer_get_byte_iterator_end,
  .byte_iterator_is_end = &pb_trivial_buffer_byte_iterator_is_end,
  .byte_iterator_cmp = &pb_trivial_buffer_byte_iterator_cmp,
  .byte_iterator_next = &pb_trivial_buffer_byte_iterator_next,
  .byte_iterator_prev = &pb_trivial_buffer_byte_iterator_prev,

  .page_create = &pb_mmap_buffer_page_create,
  .page_create_ref = &pb_mmap_buffer_page_create_ref,

  .insert = &pb_mmap_buffer_insert,
  .extend = &pb_mmap_buffer_extend,
  .rewind = &pb_mmap_buffer_rewind,
  .seek = &pb_mmap_buffer_seek,
  .trim = &pb_mmap_buffer_trim,

  .insert_data = &pb_trivial_buffer_insert_data,
  .insert_data_ref = &pb_trivial_buffer_insert_data_ref,
  .insert_buffer = &pb_trivial_buffer_insert_buffer,

  .write_data = &pb_mmap_buffer_write_data,
  .write_data_ref = &pb_mmap_buffer_write_data_ref,
  .write_buffer = &pb_mmap_buffer_write_buffer,

  .overwrite_data = &pb_trivial_buffer_overwrite_data,
  .overwrite_buffer = &pb_trivial_buffer_overwrite_buffer,

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
  if (((open_action != pb_mmap_open_action_append) &&
       (open_action != pb_mmap_open_action_overwrite)) ||
      ((close_action != pb_mmap_close_action_retain) &&
       (close_action != pb_mmap_close_action_remove))) {
    errno = EINVAL;

    return NULL;
  }

  struct pb_mmap_allocator *mmap_allocator =
    pb_mmap_allocator_create(file_path, open_action, close_action, allocator);
  if (!mmap_allocator)
    return NULL;

  struct pb_mmap_buffer *mmap_buffer =
    pb_allocator_alloc(
      mmap_allocator->struct_allocator,
      pb_alloc_type_struct, sizeof(struct pb_mmap_buffer));
  if (!mmap_buffer) {
    pb_mmap_allocator_put(mmap_allocator);

    return NULL;
  }

  mmap_buffer->trivial_buffer.buffer.strategy = pb_get_mmap_buffer_strategy();

  mmap_buffer->trivial_buffer.buffer.operations =
    pb_get_mmap_buffer_operations();

  mmap_buffer->trivial_buffer.buffer.allocator = &mmap_allocator->allocator;

  mmap_buffer->trivial_buffer.page_end.prev = &mmap_buffer->trivial_buffer.page_end;
  mmap_buffer->trivial_buffer.page_end.next = &mmap_buffer->trivial_buffer.page_end;

  mmap_buffer->trivial_buffer.data_revision = 0;
  mmap_buffer->trivial_buffer.data_size = 0;

  return &mmap_buffer->trivial_buffer.buffer;
}



/*******************************************************************************
 */
static uint64_t pb_mmap_buffer_get_data_size(struct pb_buffer * const buffer) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  return pb_mmap_allocator_get_data_size(mmap_allocator);
}

/*******************************************************************************
 */
static struct pb_page *pb_mmap_buffer_page_create(
    struct pb_buffer * const buffer,
    size_t len) {
  assert(0);

  return NULL;
}

static struct pb_page *pb_mmap_buffer_page_create_ref(
    struct pb_buffer * const buffer,
    const uint8_t *buf, size_t len) {
  assert(0);

  return NULL;
}

/*******************************************************************************
 */
void pb_mmap_buffer_get_iterator(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  pb_trivial_buffer_get_iterator(buffer, buffer_iterator);
  if (!pb_trivial_buffer_iterator_is_end(buffer, buffer_iterator))
    return;

  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  struct pb_page *page =
    pb_mmap_allocator_page_map_forward(mmap_allocator, buffer_iterator);
  if (!page) {
    pb_trivial_buffer_get_iterator_end(buffer, buffer_iterator);

    return;
  }

  if (pb_trivial_buffer_insert(buffer, buffer_iterator, 0, page) == 0) {
    pb_trivial_buffer_get_iterator_end(buffer, buffer_iterator);

    return;
  }

  pb_trivial_buffer_get_iterator(buffer, buffer_iterator);
}

void pb_mmap_buffer_get_iterator_end(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  pb_trivial_buffer_get_iterator_end(buffer, buffer_iterator);
}

bool pb_mmap_buffer_iterator_is_end(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator) {
  return pb_trivial_buffer_iterator_is_end(buffer, buffer_iterator);
}

bool pb_mmap_buffer_iterator_cmp(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *lvalue,
    const struct pb_buffer_iterator *rvalue) {
  return pb_trivial_buffer_iterator_cmp(buffer, lvalue, rvalue);
}

void pb_mmap_buffer_iterator_next(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  pb_trivial_buffer_iterator_next(buffer, buffer_iterator);
  if (!pb_trivial_buffer_iterator_is_end(buffer, buffer_iterator))
    return;

  // reset the iterator to its previous position
  pb_trivial_buffer_iterator_prev(buffer, buffer_iterator);

  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  struct pb_page *page =
    pb_mmap_allocator_page_map_forward(mmap_allocator, buffer_iterator);
  if (!page) {
    pb_trivial_buffer_get_iterator_end(buffer, buffer_iterator);

    return;
  }

  struct pb_buffer_iterator end_buffer_iterator;
  pb_trivial_buffer_get_iterator_end(buffer, &end_buffer_iterator);

  // insert the new page at the end
  if (pb_trivial_buffer_insert(buffer, &end_buffer_iterator, 0, page) == 0) {
    pb_trivial_buffer_get_iterator_end(buffer, buffer_iterator);

    return;
  }

  // now advance the iterator
  pb_trivial_buffer_iterator_next(buffer, buffer_iterator);
}

void pb_mmap_buffer_iterator_prev(struct pb_buffer * const buffer,
    struct pb_buffer_iterator * const buffer_iterator) {
  pb_trivial_buffer_iterator_prev(buffer, buffer_iterator);
  if (!pb_trivial_buffer_iterator_is_end(buffer, buffer_iterator))
    return;

  // reset the iterator to its previous position
  pb_trivial_buffer_iterator_next(buffer, buffer_iterator);

  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  struct pb_page *page =
    pb_mmap_allocator_page_map_backward(mmap_allocator, buffer_iterator);
  if (!page) {
    pb_trivial_buffer_get_iterator_end(buffer, buffer_iterator);

    return;
  }

  struct pb_buffer_iterator head_buffer_iterator;
  pb_trivial_buffer_get_iterator(buffer, &head_buffer_iterator);

  // insert the new page at the head
  if (pb_trivial_buffer_insert(buffer, &head_buffer_iterator, 0, page) == 0) {
    pb_trivial_buffer_get_iterator_end(buffer, buffer_iterator);

    return;
  }

  // now rewind the iterator
  pb_trivial_buffer_iterator_prev(buffer, buffer_iterator);
}

/*******************************************************************************
 */
uint64_t pb_mmap_buffer_insert(struct pb_buffer * const buffer,
    const struct pb_buffer_iterator *buffer_iterator,
    size_t offset,
    struct pb_page * const page) {
  assert(0);
  return 0;
}

uint64_t pb_mmap_buffer_extend(struct pb_buffer * const buffer,
    uint64_t len) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  return pb_mmap_allocator_extend(mmap_allocator, len);;
}

uint64_t pb_mmap_buffer_rewind(struct pb_buffer * const buffer,
    uint64_t len) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  uint64_t rewinded = pb_mmap_allocator_rewind(mmap_allocator, len);

  pb_trivial_pure_buffer_clear(buffer);

  return rewinded;
}

uint64_t pb_mmap_buffer_seek(struct pb_buffer * const buffer,
    uint64_t len) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  uint64_t seeked = pb_mmap_allocator_seek(mmap_allocator, len);

  pb_trivial_pure_buffer_clear(buffer);

  return seeked;
}

uint64_t pb_mmap_buffer_trim(struct pb_buffer * const buffer,
    uint64_t len) {
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  uint64_t trimmed = pb_mmap_allocator_trim(mmap_allocator, len);

  pb_trivial_pure_buffer_clear(buffer);

  return trimmed;
}

/*******************************************************************************
 */
uint64_t pb_mmap_buffer_write_data(struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  return pb_mmap_allocator_write_data(mmap_allocator, buf, len);
}

uint64_t pb_mmap_buffer_write_data_ref(
    struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  return pb_mmap_allocator_write_data(mmap_allocator, buf, len);
}

uint64_t pb_mmap_buffer_write_buffer(
    struct pb_buffer * const buffer,
    struct pb_buffer * const src_buffer,
    uint64_t len) {
  if (pb_buffer_get_data_size(buffer) == 0)
    pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  return pb_mmap_allocator_write_data_buffer(mmap_allocator, src_buffer, len);
}

uint64_t pb_mmap_buffer_overwrite_data(
    struct pb_buffer * const buffer,
    const void *buf,
    uint64_t len) {
  pb_trivial_buffer_increment_data_revision(buffer);

  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  return pb_mmap_allocator_overwrite_data(mmap_allocator, buf, len);
}

/*******************************************************************************
 */
static void pb_mmap_buffer_clear(struct pb_buffer * const buffer) {
  pb_trivial_pure_buffer_clear(buffer);

  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  pb_mmap_allocator_clear(mmap_allocator);
}

static void pb_mmap_buffer_destroy(struct pb_buffer * const buffer) {
  pb_buffer_clear(buffer);

  struct pb_mmap_buffer *mmap_buffer =
    (struct pb_mmap_buffer*)buffer;
  struct pb_mmap_allocator *mmap_allocator =
    (struct pb_mmap_allocator*)buffer->allocator;

  pb_allocator_free(
    &mmap_allocator->allocator, pb_alloc_type_struct,
    mmap_buffer, sizeof(struct pb_mmap_buffer));

  pb_mmap_allocator_put(mmap_allocator);
}
