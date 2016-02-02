/*******************************************************************************
 *  Copyright 2016 Nick Jones <nick.fa.jones@gmail.com>
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

#ifndef PAGEBUF_MMAP_HPP
#define PAGEBUF_MMAP_HPP


#include <string>

#include <pagebuf/pagebuf_mmap.h>

#include <pagebuf/pagebuf.hpp>


namespace pb
{

/** C++ wrapper araound pb_mmap_buffer */
class mmap_buffer : public buffer {
  public:
    enum open_action {
      open_action_append =                              pb_mmap_open_action_append,
      open_action_overwrite =                           pb_mmap_open_action_overwrite,
    };

    enum close_action {
      close_action_retain =                             pb_mmap_close_action_retain,
      close_action_remove =                             pb_mmap_close_action_remove,
    };

  public:
    mmap_buffer(const std::string& file_path,
                enum open_action open_action,
                enum close_action close_action) :
        buffer(
          pb_mmap_buffer_create(
            file_path.c_str(),
            pb_mmap_open_action(open_action),
            pb_mmap_close_action(close_action))),
        file_path_(file_path) {
    }

    mmap_buffer(const std::string& file_path,
                enum open_action open_action,
                enum close_action close_action,
                const struct pb_allocator *allocator) :
        buffer(
          pb_mmap_buffer_create_with_alloc(
            file_path.c_str(),
            pb_mmap_open_action(open_action),
            pb_mmap_close_action(close_action),
            allocator)),
        file_path_(file_path) {
    }

    mmap_buffer(mmap_buffer&& rvalue) :
        buffer(static_cast<buffer&&>(rvalue)),
        file_path_(rvalue.file_path_) {
      rvalue.buffer_ = 0;
    }

  private:
    mmap_buffer(const mmap_buffer& rvalue) {
    }

  public:
    ~mmap_buffer() {
    }

  private:
    mmap_buffer& operator=(const mmap_buffer&& rvalue) {
      return *this;
    }

  public:
    const std::string& get_file_path() const {
      return file_path_;
    }

  protected:
    std::string file_path_;
};

}; /* namespace pb */

#endif /* PAGEBUF_MMAP_HPP */