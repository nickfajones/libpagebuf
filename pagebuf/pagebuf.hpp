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


#include <string>

#include <pagebuf/pagebuf.h>


namespace pb
{

/** Pre-declare some friends
 */
class data_reader;
class byte_reader;
class line_reader;



/** C++ wrapper around pb_buffer, using trivial buffer in the base class.
 */
class buffer {
  public:
    friend class data_reader;
    friend class byte_reader;
    friend class line_reader;

  public:
    /** C++ wrapper around pb_buffer_iterator.
     */
    class iterator {
      public:
        friend class buffer;

      public:
        iterator() :
          itr_({}),
          buf_(0) {
        }

      private:
        iterator(struct pb_buffer * const buffer, bool at_end) :
          itr_({}),
          buf_(buffer) {
          if (!at_end)
            pb_buffer_get_iterator(buf_, &itr_);
          else
            pb_buffer_get_iterator_end(buf_, &itr_);
        }

      public:
        iterator(const iterator& rvalue) :
          itr_({}),
          buf_(0) {
          *this = rvalue;
        }

        ~iterator() {
          itr_ = {};
          buf_ = 0;
        }

      public:
        iterator& operator=(const iterator& rvalue) {
          itr_ = rvalue.itr_;
          buf_ = rvalue.buf_;

          return *this;
        }

      public:
        bool operator==(const iterator& rvalue) {
          return
            ((buf_ == rvalue.buf_) &&
             (pb_buffer_iterator_cmp(buf_, &itr_, &rvalue.itr_)));
        }

      public:
        iterator& operator++() {
          pb_buffer_iterator_next(buf_, &itr_);

          return *this;
        }

        iterator operator++(int) {
           iterator temp(*this);
           ++(*this);
           return temp;
        }

      public:
        iterator& operator--() {
          pb_buffer_iterator_prev(buf_, &itr_);

          return *this;
        }

        iterator operator--(int) {
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

        struct pb_buffer *buf_;
    };

  public:
    class byte_iterator {
      public:
        friend class buffer;

      public:
        byte_iterator() :
          itr_({}),
          buf_(0) {
        }

      private:
        byte_iterator(struct pb_buffer * const buffer, bool at_end) :
          itr_({}),
          buf_(buffer) {
          if (!at_end)
            pb_buffer_get_byte_iterator(buf_, &itr_);
          else
            pb_buffer_get_byte_iterator_end(buf_, &itr_);
        }

      public:
        byte_iterator(const byte_iterator& rvalue) :
          itr_({}),
          buf_(0) {
          *this = rvalue;
        }

        ~byte_iterator() {
          itr_ = {};
          buf_ = 0;
        }

      public:
        byte_iterator& operator=(const byte_iterator& rvalue) {
          itr_ = rvalue.itr_;
          buf_ = rvalue.buf_;

          return *this;
        }

      public:
        bool operator==(const byte_iterator& rvalue) {
          return
            ((buf_ == rvalue.buf_) &&
             (pb_buffer_byte_iterator_cmp(buf_, &itr_, &rvalue.itr_)));
        }

      public:
        byte_iterator& operator++() {
          pb_buffer_byte_iterator_next(buf_, &itr_);

          return *this;
        }

        byte_iterator operator++(int) {
           byte_iterator temp(*this);
           ++(*this);
           return temp;
        }

      public:
        byte_iterator& operator--() {
          pb_buffer_byte_iterator_prev(buf_, &itr_);

          return *this;
        }

        byte_iterator operator--(int) {
          byte_iterator temp(*this);
          --(*this);
          return temp;
        }

      public:
        const char* operator*() {
          return itr_.current_byte;
        }

      private:
        struct pb_buffer_byte_iterator itr_;

        struct pb_buffer *buf_;
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
        pb_buffer_destroy(buf_);
        buf_ = 0;
      }
    }

  private:
    buffer& operator=(const buffer& rvalue) {
      return *this;
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
    uint64_t get_data_revision() {
      return pb_buffer_get_data_revision(buf_);
    }

    uint64_t get_data_size() {
      return pb_buffer_get_data_size(buf_);
    }

  public:
    uint64_t seek(uint64_t len) {
      return pb_buffer_seek(buf_, len);
    }

    uint64_t reserve(uint64_t len) {
      return pb_buffer_reserve(buf_, len);
    }

    uint64_t rewind(uint64_t len) {
      return pb_buffer_rewind(buf_, len);
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
      return pb_buffer_write_data(buf_, buf, len);
    }

    uint64_t write_ref(const uint8_t *buf, uint64_t len) {
      return pb_buffer_write_data_ref(buf_, buf, len);
    }

    uint64_t write(const buffer& src_buf, uint64_t len) {
      return pb_buffer_write_buffer(buf_, src_buf.buf_, len);
    }

    uint64_t overwrite(const uint8_t *buf, uint64_t len) {
      return pb_buffer_overwrite_data(buf_, buf, len);
    }

  public:
    uint64_t read(uint8_t * const buf, uint64_t len) {
      return pb_buffer_read_data(buf_, buf, len);
    }

  public:
    void clear() {
    pb_buffer_clear(buf_);
    }

  protected:
    struct pb_buffer *buf_;
};



/** C++ Wrapper around pb_line_reader
 */
class line_reader {
  public:
    line_reader() :
      reader_(0),
      has_line_(false),
      oversize_(false) {
    }

    explicit line_reader(buffer& buf) :
      reader_(pb_trivial_line_reader_create(buf.buf_)),
      has_line_(false),
      oversize_(false) {
    }

    line_reader(const byte_reader& rvalue) :
      reader_(0),
      has_line_(false),
      oversize_(false) {
      *this = rvalue;
    }

    line_reader(line_reader&& rvalue) :
      reader_(0),
      has_line_(false),
      oversize_(false) {
      *this = rvalue;
    }

    virtual ~line_reader() {
      destroy();
    }

  private:
    line_reader& operator=(const line_reader& rvalue) {
      destroy();

      reader_ = rvalue.reader_->clone(rvalue.reader_);

      return *this;
    }

  public:
    line_reader& operator=(line_reader&& rvalue) {
      destroy();

      reader_ = rvalue.reader_;
      rvalue.reader_ = 0;

      return *this;
    }

  public:
    virtual void reset() {
      if (reader_)
        reader_->reset(reader_);

      line_.clear();

      has_line_ = false;
      oversize_ = false;
    }

  protected:
    virtual void destroy() {
      reset();

      if (reader_) {
        reader_->destroy(reader_);
        reader_ = 0;
      }
    }

  public:
    bool has_line() {
      if (has_line_)
        return true;

      if (!reader_->has_line(reader_))
        return false;

      reset();

      has_line_ = true;

      size_t line_len = reader_->get_line_len(reader_);
      if (line_len > line_.max_size()) {
        line_len = line_.max_size();

        oversize_ = true;
      }

      line_.resize(line_len);
      reader_->get_line_data(
        reader_,
        reinterpret_cast<uint8_t*>(const_cast<char*>(line_.data())),
        line_len);

      return true;
    }

  public:
    size_t get_line_len() {
      if (!has_line_)
        return 0;

      return reader_->get_line_len(reader_);
    }

    const std::string& get_line() {
      return line_;
    }

  public:
    size_t seek_line() {
      if (!has_line_)
        return 0;

      size_t seeked;

      if (!oversize_)
        seeked = reader_->seek_line(reader_);
      else
        seeked = pb_buffer_seek(reader_->buffer, line_.size());

      reset();

      return seeked;
    }

  public:
    void terminate_line() {
      terminate_line(false);
    }

    void terminate_line(bool check_cr) {
      if (!check_cr)
        reader_->terminate_line(reader_);
      else
        reader_->terminate_line_check_cr(reader_);
    }

  public:
    bool is_line_crlf() {
      return reader_->is_crlf(reader_);
    }

    bool is_end() {
      return reader_->is_end(reader_);
    }

  protected:
    struct pb_line_reader *reader_;

  protected:
    std::string line_;

    bool has_line_;
    bool oversize_;
};

}; /* namespace pagebuf */

#endif /* PAGEBUF_HPP */
