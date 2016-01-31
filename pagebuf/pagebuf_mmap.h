/*******************************************************************************
 *  Copyright \2015 Nick Jones <nick.fa.jones@gmail.com>
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

#ifndef PAGEBUF_MMAP_H
#define PAGEBUF_MMAP_H


#include <pagebuf/pagebuf.h>


#ifdef __cplusplus
extern "C" {
#endif



/** Indicates which actions to take when opening and closing mmap'd files. */
enum pb_mmap_open_action {
  pb_mmap_open_action_append =                            1,
  pb_mmap_open_action_overwrite =                         2,
};

enum pb_mmap_close_action {
  pb_mmap_close_action_retain =                           1,
  pb_mmap_close_action_remove =                           2,
};



/** Factory functions for the mmap buffer implementation of pb_buffer.
 *
 * The mmap buffer has a specific strategy and customised operations that allow
 * it to use mmap'd memory regions backed by a file on a block storage device
 * as its data storage backend.
 *
 * The mmap buffer will make use of a supplied allocator for the purpose of
 * allocating structs, however data regions will be allocated using an
 * internal allocator.  If no allocator is supplied, the trivial heap based
 * allocator will be used for struct allocations.
 *
 * The mmap buffer requires some parameters for initialisation:
 * file_path: the full path and file name of the location of the file that the
 *           buffer is to use as storage.  It is up to the user of the mmap
 *           buffer to ensure the path both exists and is writable, and the
 *           file doesn't exist or is at least readable.
 *
 * open_action is the action to perform when the buffer is opened:
 *             append leaves the file unchanged and appends additional writes
 *             overwrite clears the file and writes start at the beginning
 * close_action is the action to perform when the buffer is closed:
 *              retain closes the file and leaves it as it is
 *              remove closes the file and deletes it
 *
 * Parameter validation errors during mmap buffer create will cause errno to be
 * set to EINVAL.
 * System errors during mmap buffer create will cause errno to be set to the
 * appropriate non zero value by the system call.
 */
struct pb_buffer *pb_mmap_buffer_create(const char *file_path,
  enum pb_mmap_open_action open_action,
  enum pb_mmap_close_action close_action);
struct pb_buffer *pb_mmap_buffer_create_with_alloc(const char *file_path,
  enum pb_mmap_open_action open_action,
  enum pb_mmap_close_action close_action,
  const struct pb_allocator *allocator);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAGEBUF_MMAP_H */
