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

#include "pagebuf.hpp"

#include <cassert>
#include <cstring>



namespace pb
{

namespace priv
{

/*******************************************************************************
 */
template <
    typename T,
    void *(T::*AllocFunc)(enum pb_allocator_type type, size_t size)>
static void *pb_allocator_alloc_thunk(
    const struct pb_allocator *allocator,
    enum pb_allocator_type type, size_t size) {
  return
    (const_cast<T*>(reinterpret_cast<const T*>(allocator))->*AllocFunc)(
      type, size);
}

template <
    typename T,
    void (T::*FreeFunc)(enum pb_allocator_type type, void *obj, size_t size)>
static void pb_allocator_free_thunk(
    const struct pb_allocator *allocator,
    enum pb_allocator_type type, void *obj, size_t size) {
  (const_cast<T*>(reinterpret_cast<const T*>(allocator))->*FreeFunc)(
    type, obj, size);
}



/*******************************************************************************
 */
void calculate_allocation_size(
    size_t obj_size,
    const block_allocator::profile_set& block_profiles,
    size_t &block_size, size_t block_count) {
  block_size = 0;
  block_count = 1;

  if (block_profiles.empty()) {
    return;
  }

  for (block_allocator::profile_set::iterator itr = block_profiles.begin();
       itr != block_profiles.end();
       ++itr) {
    block_size = itr->block_size;

    if (itr->block_size >= obj_size)
      return;
  }

  block_count =
      (obj_size / block_size) +
    (((obj_size % block_size) != 0) ? 1 : 0);
}

}; /* namespace priv */



/*******************************************************************************
 */
block_allocator::block_profile::block_profile(size_t size, size_t count) :
  block_size(size),
  block_count(count) {
}

block_allocator::block_profile::block_profile(
    const block_allocator::block_profile& rvalue) :
  block_size(0),
  block_count(0) {
  *this = rvalue;
}

block_allocator::block_profile::~block_profile() {
}

/*******************************************************************************
 */
block_allocator::block_profile& block_allocator::block_profile::operator=(
    const block_allocator::block_profile& rvalue) {
  block_size = rvalue.block_size;
  block_count = rvalue.block_count;

  return *this;
}

/*******************************************************************************
 */
bool block_allocator::block_profile::operator<(
    const block_allocator::block_profile& rvalue) const {
  return (block_size < rvalue.block_size);
}



/*******************************************************************************
 */
block_allocator::block_allocator() {
  profile_set block_profiles;
  block_profiles.insert(block_profile(32, 1024));
  block_profiles.insert(block_profile(64, 1024));
  block_profiles.insert(block_profile(128, 1024));
  block_profiles.insert(block_profile(256, 1024));
  block_profiles.insert(block_profile(4096, 4096));

  initialise(block_profiles);
}

block_allocator::block_allocator(
    const block_allocator::profile_set& block_profiles) {
  initialise(block_profiles);
}

block_allocator::block_allocator(const block_allocator& rvalue) {
  assert(0);
}

block_allocator::~block_allocator() {
}

/*******************************************************************************
 */
block_allocator& block_allocator::operator=(const block_allocator& rvalue) {
  assert(0);

  return *this;
}

/*******************************************************************************
 */
const struct pb_allocator *block_allocator::get_allocator()
  {
  return &allocator_;
  }

/*******************************************************************************
 */
void block_allocator::initialise(
    const block_allocator::profile_set& block_profiles) {

  allocator_.alloc =
    priv::pb_allocator_alloc_thunk<block_allocator, &block_allocator::alloc>;
  allocator_.free =
    priv::pb_allocator_free_thunk<block_allocator, &block_allocator::free>;
}

/*******************************************************************************
 */
void *block_allocator::alloc(enum pb_allocator_type type, size_t size) {
  return NULL;
}

void block_allocator::free(enum pb_allocator_type type, void *obj, size_t size) {

}

}; /* namespace pagebuf */
