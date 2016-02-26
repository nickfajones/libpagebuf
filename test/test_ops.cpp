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


#ifdef NDEBUG
#undef NDEBUG
#endif


#include <sys/types.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <list>

#include "pagebuf/pagebuf.hpp"
#include "pagebuf/pagebuf_mmap.hpp"

#include <stdio.h>


/*******************************************************************************
 */
#define TEST_OPS_MERGE(prefix,counter) prefix##counter
#define TEST_OPS_UNIQUE_VAR(counter) TEST_OPS_MERGE(cnd_,counter)
#define TEST_OPS_EVAL(condition) \
  bool TEST_OPS_UNIQUE_VAR(__LINE__) = (condition); \
  if (TEST_OPS_UNIQUE_VAR(__LINE__)) \
    fprintf( \
      stderr, \
      "Error Condition Found: Test: '%s', Line: '%d': Subject: '%s': '%s'\n", \
        __PRETTY_FUNCTION__, __LINE__, \
        subject.description.c_str(), \
        #condition); \
  if (TEST_OPS_UNIQUE_VAR(__LINE__))



/*******************************************************************************
 */
class test_subject {
  public:
    test_subject() :
      buffer(0) {
    }

    ~test_subject() {
      if (buffer) {
        delete buffer;
        buffer = 0;
      }
    }

  public:
    void init(
        const std::string &_description,
        pb::buffer *_buffer) {
      description = _description;
      buffer = _buffer;
    }

  public:
    std::string description;

    pb::buffer *buffer;
};



/*******************************************************************************
 */
class test_base {
  public:
    static int final_result;
};

int test_base::final_result = 0;



/*******************************************************************************
 */
template <typename T>
class test_case : public test_base {
  public:
    virtual int run_test(const test_subject& subject) = 0;

  public:
    static void run_test(const std::list<test_subject>& test_subjects) {
      T test_case;

      for (std::list<test_subject>::const_iterator itr = test_subjects.begin();
           itr != test_subjects.end();
           ++itr) {
        int result = test_case.run_test(*itr);
        final_result = ((final_result == 0) && (result == 0)) ? 0 : 1;
      }
    }
};



/*******************************************************************************
 */
class test_case_iterate1 : public test_case<test_case_iterate1> {
  public:
    static const char *input;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      size_t count_limit =
        ((subject.buffer->get_strategy().page_size * 10) / strlen(input)) + 1;

      for (size_t counter = 0; counter < count_limit; ++counter) {
        TEST_OPS_EVAL(subject.buffer->write(
              input, strlen(input)) != strlen(input))
          return 1;
      }

      TEST_OPS_EVAL(subject.buffer->get_data_size() !=
              (count_limit * strlen(input)))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < subject.buffer->get_data_size(); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i % strlen(output)])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_iterate1::input = "abcdefghijklmnopqrstuvwxyz";
const char *test_case_iterate1::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_iterate2 : public test_case<test_case_iterate2> {
  public:
    static const char *input;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      size_t count_limit =
        ((subject.buffer->get_strategy().page_size * 10) / strlen(input)) + 1;

      for (size_t counter = 0; counter < count_limit; ++counter) {
        TEST_OPS_EVAL(subject.buffer->write(
              input, strlen(input)) != strlen(input))
          return 1;
      }

      TEST_OPS_EVAL(subject.buffer->get_data_size() !=
              (count_limit * strlen(input)))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_end();
      --byte_itr;

      for (unsigned int i = 0; i < subject.buffer->get_data_size(); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i % strlen(output)])
          return 1;

        --byte_itr;
      }

      return 0;
    }
};

const char *test_case_iterate2::input = "abcdefghijklmnopqrstuvwxyz";
const char *test_case_iterate2::output = "zyxwvutsrqponmlkjihgfedcba";



/*******************************************************************************
 */
class test_case_insert1 : public test_case<test_case_insert1> {
  public:
    static const char *input1;
    static const char *input2;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      if (subject.buffer->get_strategy().rejects_insert)
        return 0;

      TEST_OPS_EVAL(subject.buffer->write(
            input1, strlen(input1)) != strlen(input1))
        return 1;

      pb::buffer::iterator buf_itr = subject.buffer->begin();

      TEST_OPS_EVAL(subject.buffer->insert(
            buf_itr, 5,
            input2, strlen(input2)) != strlen(input2))
        return 1;

      TEST_OPS_EVAL(subject.buffer->get_data_size() !=
          (strlen(input1) + strlen(input2)))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < strlen(output); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_insert1::input1 = "abcdejklmnopqrstuvwxyz";
const char *test_case_insert1::input2 = "fghi";
const char *test_case_insert1::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_insert2 : public test_case<test_case_insert2> {
  public:
    static const char *input1;
    static const char *input2;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      if (subject.buffer->get_strategy().rejects_insert)
        return 0;

      TEST_OPS_EVAL(subject.buffer->write(
            input1, strlen(input1)) != strlen(input1))
        return 1;

      pb::buffer::iterator buf_itr = subject.buffer->begin();

      TEST_OPS_EVAL(subject.buffer->insert_ref(
            buf_itr, 5,
            input2, strlen(input2)) != strlen(input2))
        return 1;

      TEST_OPS_EVAL(subject.buffer->get_data_size() !=
          (strlen(input1) + strlen(input2)))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < strlen(output); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_insert2::input1 = "abcdejklmnopqrstuvwxyz";
const char *test_case_insert2::input2 = "fghi";
const char *test_case_insert2::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_insert3 : public test_case<test_case_insert3> {
  public:
    static const char *input1;
    static const char *input2;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      if (subject.buffer->get_strategy().rejects_insert)
        return 0;

      TEST_OPS_EVAL(subject.buffer->write(
            input1, strlen(input1)) != strlen(input1))
        return 1;

      pb::buffer src_buffer;

      TEST_OPS_EVAL(src_buffer.write(
            input2, strlen(input2)) != strlen(input2))
        return 1;

      pb::buffer::iterator buf_itr = subject.buffer->begin();

      TEST_OPS_EVAL(subject.buffer->insert(
            buf_itr, 5,
            src_buffer, src_buffer.get_data_size()) !=
          src_buffer.get_data_size())
        return 1;

      src_buffer.clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() !=
          (strlen(input1) + strlen(input2)))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < strlen(output); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_insert3::input1 = "abcdejklmnopqrstuvwxyz";
const char *test_case_insert3::input2 = "fghi";
const char *test_case_insert3::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_overwrite1 : public test_case<test_case_overwrite1> {
  public:
    static const char *input1;
    static const char *input2;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      if (subject.buffer->get_strategy().rejects_overwrite)
        return 0;

      size_t input_size = subject.buffer->get_strategy().page_size + 10;
      size_t seek_size = input_size - 26;
      char *input_buf = new char[input_size];
      memset(input_buf, 0, input_size);
      memcpy(input_buf + seek_size, input1, strlen(input1));

      TEST_OPS_EVAL(subject.buffer->write(
            input_buf, input_size) != input_size) {
        delete [] input_buf;

        return 1;
      }

      delete [] input_buf;

      TEST_OPS_EVAL(subject.buffer->seek(seek_size) != seek_size)
        return 1;

      TEST_OPS_EVAL(subject.buffer->overwrite(
            input2, strlen(input2)) != strlen(input2))
        return 1;

      TEST_OPS_EVAL(subject.buffer->get_data_size() != strlen(input1))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < strlen(output); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_overwrite1::input1 = "----efghijklmnopqrstuvwxyz";
const char *test_case_overwrite1::input2 = "abcd";
const char *test_case_overwrite1::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_overwrite2 : public test_case<test_case_overwrite2> {
  public:
    static const char *input1;
    static const char *input2;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      if (subject.buffer->get_strategy().rejects_overwrite)
        return 0;

      size_t input_size = subject.buffer->get_strategy().page_size + 10;
      size_t seek_size = input_size - 26;
      char *input_buf = new char[input_size];
      memset(input_buf, 0, input_size);
      memcpy(input_buf + seek_size, input1, strlen(input1));

      TEST_OPS_EVAL(subject.buffer->write(
            input_buf, input_size) != input_size) {
        delete [] input_buf;

        return 1;
      }

      delete [] input_buf;

      TEST_OPS_EVAL(subject.buffer->seek(seek_size) != seek_size)
        return 1;

      pb::buffer src_buffer;

      TEST_OPS_EVAL(src_buffer.write(
            input2, strlen(input2)) != strlen(input2))
        return 1;

      TEST_OPS_EVAL(subject.buffer->overwrite(
            src_buffer, src_buffer.get_data_size()) !=
          src_buffer.get_data_size())
        return 1;

      src_buffer.clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != strlen(input1))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < strlen(output); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_overwrite2::input1 = "----efghijklmnopqrstuvwxyz";
const char *test_case_overwrite2::input2 = "abcd";
const char *test_case_overwrite2::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_rewind1 : public test_case<test_case_rewind1> {
  public:
    static const char *input1;
    static const char *input2;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      if (subject.buffer->get_strategy().rejects_rewind)
        return 0;

      TEST_OPS_EVAL(subject.buffer->write(
            input1, strlen(input1)) != strlen(input1))
        return 1;

      TEST_OPS_EVAL(subject.buffer->seek(
            strlen(input2)) != strlen(input2))
        return 1;

      TEST_OPS_EVAL(subject.buffer->rewind(
            strlen(input2)) != strlen(input2))
        return 1;

      TEST_OPS_EVAL(subject.buffer->overwrite(
            input2, strlen(input2)) != strlen(input2))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < strlen(output); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_rewind1::input1 = "----efghijklmnopqrstuvwxyz";
const char *test_case_rewind1::input2 = "abcd";
const char *test_case_rewind1::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_rewind2 : public test_case<test_case_rewind2> {
  public:
    static const char *input;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      if (subject.buffer->get_strategy().rejects_rewind ||
          subject.buffer->get_strategy().rejects_overwrite)
        return 0;

      size_t count_limit =
        ((subject.buffer->get_strategy().page_size * 5) / strlen(input)) + 1;

      for (size_t counter = 0; counter < count_limit; ++counter) {
        TEST_OPS_EVAL(subject.buffer->write(
              input, strlen(input)) != strlen(input))
          return 1;
      }

      TEST_OPS_EVAL(subject.buffer->get_data_size() !=
              (count_limit * strlen(input)))
        return 1;

      count_limit =
        ((subject.buffer->get_strategy().page_size * 3) / strlen(input)) + 1;

      TEST_OPS_EVAL(subject.buffer->seek(
            count_limit * strlen(input)) != (count_limit * strlen(input)))
        return 1;

      count_limit =
        ((subject.buffer->get_strategy().page_size * 2) / strlen(input));

      TEST_OPS_EVAL(subject.buffer->get_data_size() !=
              (count_limit * strlen(input)))
        return 1;

      for (size_t counter = 0; counter < count_limit; ++counter) {
        TEST_OPS_EVAL(subject.buffer->rewind(strlen(input)) != strlen(input))
          return 1;

        TEST_OPS_EVAL(subject.buffer->overwrite(
              input, strlen(input)) != strlen(input))
        return 1;
      }

      count_limit =
        ((subject.buffer->get_strategy().page_size * 4) / strlen(input));

      TEST_OPS_EVAL(subject.buffer->get_data_size() !=
              (count_limit * strlen(input)))
        return 1;

      TEST_OPS_EVAL(subject.buffer->rewind(10) != 10)
        return 1;

      TEST_OPS_EVAL(subject.buffer->overwrite(
            input + 16, strlen(input) - 16) != (strlen(input) - 16))
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < subject.buffer->get_data_size(); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[(i + 16) % strlen(output)])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_rewind2::input = "abcdefghijklmnopqrstuvwxyz";
const char *test_case_rewind2::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_trim1 : public test_case<test_case_trim1> {
  public:
    static const char *input;
    static const char *output;

  public:
    virtual int run_test(const test_subject& subject) {
      subject.buffer->clear();

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 0)
        return 1;

      if (subject.buffer->get_strategy().rejects_trim)
        return 0;

      TEST_OPS_EVAL(subject.buffer->write(
            input, strlen(input)) != strlen(input))
        return 1;

      TEST_OPS_EVAL(subject.buffer->trim(10) != 10)
        return 1;

      TEST_OPS_EVAL(subject.buffer->get_data_size() != 16)
        return 1;

      pb::buffer::byte_iterator byte_itr = subject.buffer->byte_begin();

      for (unsigned int i = 0; i < strlen(output); ++i) {
        TEST_OPS_EVAL(*byte_itr != output[i])
          return 1;

        ++byte_itr;
      }

      return 0;
    }
};

const char *test_case_trim1::input = "abcdefghijklmnopqrstuvwxyz";
const char *test_case_trim1::output = "abcdefghijklmnop";



/*******************************************************************************
 */
int main(int argc, char **argv) {
  std::list<test_subject> test_subjects;

  struct pb_buffer_strategy strategy;
  memset(&strategy, 0, sizeof(strategy));

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = false;
  strategy.fragment_as_target = false;

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer                                       ",
    new pb::buffer(&strategy));

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = false;
  strategy.fragment_as_target = true;

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, fragment_as_target                   ",
    new pb::buffer(&strategy));

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = true;
  strategy.fragment_as_target = false;

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, clone_on_Write                       ",
    new pb::buffer(&strategy));

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = true;
  strategy.fragment_as_target = true;

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, clone_on_Write and fragment_on_target",
    new pb::buffer(&strategy));

  char buffer_file_path[34];
  sprintf(buffer_file_path, "/tmp/pb_test_ops_buffer-%05d", getpid());

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "mmap file backed pb_buffer                                            ",
    new pb::mmap_buffer(
      buffer_file_path,
      pb::mmap_buffer::open_action_overwrite,
      pb::mmap_buffer::close_action_remove));

  test_case<test_case_iterate1>::run_test(test_subjects);
  test_case<test_case_iterate2>::run_test(test_subjects);
  test_case<test_case_insert1>::run_test(test_subjects);
  test_case<test_case_insert2>::run_test(test_subjects);
  test_case<test_case_insert3>::run_test(test_subjects);
  test_case<test_case_overwrite1>::run_test(test_subjects);
  test_case<test_case_overwrite2>::run_test(test_subjects);
  test_case<test_case_rewind1>::run_test(test_subjects);
  test_case<test_case_rewind2>::run_test(test_subjects);
  test_case<test_case_trim1>::run_test(test_subjects);

  test_subjects.clear();

  return test_base::final_result;
}
