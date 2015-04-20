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


namespace pb
{

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
    const struct pb_buffer_strategy& get_strategy() const {
      return buf_->strategy;
    }

    const struct pb_allocator& get_allocator() const {
      return *buf_->allocator;
    }

    struct pb_buffer& get_implementation() const {
      return *buf_;
    }

  public:
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
