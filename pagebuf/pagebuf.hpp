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

#ifndef PAGEBUF_HPP
#define PAGEBUF_HPP


#include <pagebuf/pagebuf.h>

#include <cstddef>
#include <cstdint>
#include <cstdbool>

#include <set>


namespace pb
{

/** pb_allocator implementation in c++
 *
 * Slab style interface supporting memory block re-use, dynamic resizing and
 * fast free block lookup.
 */
class block_allocator {
  public:
    struct block_profile {
      block_profile(size_t size, size_t count);
      block_profile(const block_profile& rvalue);
      ~block_profile();

      block_profile& operator=(const block_profile& rvalue);
      bool operator<(const block_profile& rvalue) const;

      size_t block_size;
      size_t block_count;
    };

    typedef std::set<struct block_profile> profile_set;

  public:
    block_allocator();
    explicit block_allocator(const profile_set& block_profiles);

  private:
    block_allocator(const block_allocator& rvalue);

  public:
    ~block_allocator();

  private:
    block_allocator& operator=(const block_allocator& rvalue);

  public:
    const struct pb_allocator *get_allocator();

  private:
    void initialise(const profile_set& block_profiles);

  private:
    void *alloc(enum pb_allocator_type type, size_t size);
    void free(enum pb_allocator_type type, void *obj, size_t size);

  private:
    struct block {
      uint8_t *base;
      size_t size;
      size_t count;

      std::set<size_t> reserved;

      struct block *next;
    };

  private:
    struct pb_allocator allocator_;
  };



/** C++ wrapper around pb_buffer, using trivial buffer in the base class.
 */
class buffer {
  public:
    /** C++ wrapper around pb_buffer_iterator.
     */
    class iterator {
      public:
        friend class buffer;

      public:
        iterator() :
          itr_({}),
          buffer_(0) {
        }

      private:
        iterator(struct pb_buffer * const buffer, bool at_end) :
          itr_({}),
          buffer_(buffer) {
          if (!at_end)
            buffer_->get_iterator(buffer_, &itr_);
          else
            buffer_->get_iterator_end(buffer_, &itr_);
        }

      public:
        iterator(const iterator& rvalue) :
          itr_({}),
          buffer_(0) {
          *this = rvalue;
        }

        ~iterator() {
          itr_ = {};
          buffer_ = 0;
        }

      public:
        iterator& operator=(const iterator& rvalue) {
          itr_ = rvalue.itr_;
          buffer_ = rvalue.buffer_;

          return *this;
        }

      public:
        bool operator==(const iterator& rvalue) {
          return
            ((itr_.page == rvalue.itr_.page) && (buffer_ == rvalue.buffer_));
        }

      public:
        iterator& operator++() {
          buffer_->iterator_next(buffer_, &itr_);

          return *this;
        }

        iterator  operator++(int) {
           iterator temp(*this);
           ++(*this);
           return temp;
        }

      public:
        iterator& operator--() {
          buffer_->iterator_prev(buffer_, &itr_);

          return *this;
        }

        iterator  operator--(int) {
          iterator temp(*this);
          --(*this);
          return temp;
        }

      public:
        const struct pb_data_vec& operator*() {
          return itr_.page->data_vec;
        }

      private:
        struct pb_buffer_iterator itr_;

        struct pb_buffer *buffer_;
    };

  public:
    buffer() :
      buf_(pb_trivial_buffer_create()) {
    }

    explicit buffer(const struct pb_buffer_strategy *strategy) :
      buf_(pb_trivial_buffer_create_with_strategy(strategy)) {
    }

    explicit buffer(const struct pb_allocator *allocator) :
      buf_(pb_trivial_buffer_create_with_alloc(allocator)) {
    }

    buffer(
        const struct pb_buffer_strategy *strategy,
        const struct pb_allocator *allocator) :
      buf_(
        pb_trivial_buffer_create_with_strategy_with_alloc(
          strategy, allocator)) {
    }

  protected:
    explicit buffer(struct pb_buffer *buf) :
      buf_(buf) {
    }

  private:
    buffer(const buffer& rvalue) :
      buf_(0) {
    }

  public:
    buffer(buffer&& rvalue) :
      buf_(rvalue.buf_) {
      rvalue.buf_ = 0;
    }

    ~buffer() {
      if (buf_) {
        buf_->destroy(buf_);
        buf_ = 0;
      }
    }

  public:
    struct pb_buffer_strategy& get_strategy() const {
      return buf_->strategy;
    }

    uint64_t get_data_size() {
      return buf_->get_data_size(buf_);
    }

    uint64_t get_data_revision() {
      return buf_->get_data_revision(buf_);
    }

  public:
    uint64_t reserve(uint64_t len) {
      return buf_->reserve(buf_, len);
    }

    uint64_t rewind(uint64_t len) {
      return buf_->rewind(buf_, len);
    }

    uint64_t seek(uint64_t len) {
      return buf_->seek(buf_, len);
    }

  public:
    iterator begin() {
      return iterator(buf_, false);
    }

    iterator end() {
      return iterator(buf_, true);
    }

  public:
    uint64_t write(const uint8_t *buf, uint64_t len) {
      return buf_->write_data(buf_, buf, len);
    }

    uint64_t write(const buffer& src_buf, uint64_t len) {
      return buf_->write_buffer(buf_, src_buf.buf_, len);
    }

  public:
    uint64_t overwrite(const uint8_t *buf, uint64_t len) {
      return buf_->overwrite_data(buf_, buf, len);
    }

  public:
    uint64_t read(uint8_t * const buf, uint64_t len) {
      return buf_->read_data(buf_, buf, len);
    }

  public:
    void clear() {
      buf_->clear(buf_);
    }

  protected:
    struct pb_buffer *buf_;
};

}; /* namespace pagebuf */

#endif /* PAGEBUF_HPP */
