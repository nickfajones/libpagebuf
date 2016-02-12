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
#undef NDEBU
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

#include "pagebuf/pagebuf.h"
#include "pagebuf/pagebuf_mmap.h"


/*******************************************************************************
 */
class test_subject {
  public:
    test_subject(const std::string& _description, struct pb_buffer *_buffer) :
      description(_description),
      buffer(_buffer) {
    }

    ~test_subject() {
      if (buffer) {
        pb_buffer_destroy(buffer);

        buffer = NULL;
      }
    }

  public:
    std::string description;

    struct pb_buffer *buffer;
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
    virtual int run_test(struct pb_buffer *buffer) = 0;

  public:
    static void run_test(const std::list<test_subject*>& test_subjects) {
      T test_case;

      for (std::list<test_subject*>::const_iterator itr = test_subjects.begin();
           itr != test_subjects.end();
           ++itr) {
        int result = test_case.run_test((*itr)->buffer);
        final_result = ((final_result == 0) && (result == 0)) ? 0 : 1;
      }
    }
};



/*******************************************************************************
 */
class test_case_insert1 : public test_case<test_case_insert1> {
  public:
    static const char *input1;
    static const char *input2;
    static const char *output;

  public:
    virtual int run_test(struct pb_buffer *buffer) {
      pb_buffer_clear(buffer);

      if (buffer->strategy->rejects_insert)
        return 0;

      if (pb_buffer_write_data(
            buffer,
            input1, strlen(input1)) != strlen(input1))
        return 1;

      pb_buffer_iterator buf_itr;
      pb_buffer_get_iterator(buffer, &buf_itr);

      if (pb_buffer_insert_data(
            buffer,
            &buf_itr, 5,
            input2, strlen(input2)) != strlen(input2))
        return 1;

      if (pb_buffer_get_data_size(buffer) != (strlen(input1) + strlen(input2)))
        return 1;

      pb_buffer_byte_iterator byte_itr;
      pb_buffer_get_byte_iterator(buffer, &byte_itr);

      for (unsigned int i = 0; i < strlen(output); ++i) {
        if (*byte_itr.current_byte != output[i])
          return 1;

        pb_buffer_byte_iterator_next(buffer, &byte_itr);
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
    virtual int run_test(struct pb_buffer *buffer) {
      pb_buffer_clear(buffer);

      if (buffer->strategy->rejects_insert)
        return 0;

      if (pb_buffer_write_data(
            buffer,
            input1, strlen(input1)) != strlen(input1))
        return 1;

      pb_buffer_iterator buf_itr;
      pb_buffer_get_iterator(buffer, &buf_itr);

      if (pb_buffer_insert_data_ref(
            buffer,
            &buf_itr, 5,
            input2, strlen(input2)) != strlen(input2))
        return 1;

      if (pb_buffer_get_data_size(buffer) != (strlen(input1) + strlen(input2)))
        return 1;

      pb_buffer_byte_iterator byte_itr;
      pb_buffer_get_byte_iterator(buffer, &byte_itr);

      for (unsigned int i = 0; i < strlen(output); ++i) {
        if (*byte_itr.current_byte != output[i])
          return 1;

        pb_buffer_byte_iterator_next(buffer, &byte_itr);
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
    virtual int run_test(struct pb_buffer *buffer) {
      pb_buffer_clear(buffer);

      if (buffer->strategy->rejects_insert)
        return 0;

      if (pb_buffer_write_data(
            buffer,
            input1, strlen(input1)) != strlen(input1))
        return 1;

      pb_buffer_iterator buf_itr;
      pb_buffer_get_iterator(buffer, &buf_itr);

      struct pb_buffer *src_buffer = pb_trivial_buffer_create();
      if (pb_buffer_write_data(
            src_buffer,
            input2, strlen(input2)) != strlen(input2)) {
        pb_buffer_destroy(src_buffer);

        return 1;
      }

      if (pb_buffer_insert_buffer(
            buffer,
            &buf_itr, 5,
            src_buffer, pb_buffer_get_data_size(src_buffer)) !=
         pb_buffer_get_data_size(src_buffer)) {
        pb_buffer_destroy(src_buffer);

        return 1;
      }

      pb_buffer_destroy(src_buffer);

      if (pb_buffer_get_data_size(buffer) != (strlen(input1) + strlen(input2)))
        return 1;

      pb_buffer_byte_iterator byte_itr;
      pb_buffer_get_byte_iterator(buffer, &byte_itr);

      for (unsigned int i = 0; i < strlen(output); ++i) {
        if (*byte_itr.current_byte != output[i])
          return 1;

        pb_buffer_byte_iterator_next(buffer, &byte_itr);
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
    virtual int run_test(struct pb_buffer *buffer) {
      pb_buffer_clear(buffer);

      size_t input_size = buffer->strategy->page_size + 10;
      size_t seek_size = input_size - 26;
      char *input_buf = new char[input_size];
      memset(input_buf, 0, input_size);
      memcpy(input_buf + seek_size, input1, strlen(input1));

      if (pb_buffer_write_data(buffer, input_buf, input_size) != input_size) {
        delete [] input_buf;

        return 1;
      }

      if (pb_buffer_seek(buffer, seek_size) != seek_size) {
        delete [] input_buf;

        return 1;
      }

      if (pb_buffer_overwrite_data(
            buffer,
            input2, strlen(input2)) != strlen(input2)) {
        delete [] input_buf;

        return 1;
      }

      delete [] input_buf;

      if (pb_buffer_get_data_size(buffer) != strlen(input1))
        return 0;

      pb_buffer_byte_iterator byte_itr;
      pb_buffer_get_byte_iterator(buffer, &byte_itr);

      for (unsigned int i = 0; i < strlen(output); ++i) {
        if (*byte_itr.current_byte != output[i])
          return 1;

        pb_buffer_byte_iterator_next(buffer, &byte_itr);
      }

      return 0;
    }
};

const char *test_case_overwrite1::input1 = "----efghijklmnopqrstuvwxyz";
const char *test_case_overwrite1::input2 = "abcd";
const char *test_case_overwrite1::output = "abcdefghijklmnopqrstuvwxyz";



/*******************************************************************************
 */
class test_case_trim1 : public test_case<test_case_trim1> {
  public:
    static const char *input1;
    static const char *output;

  public:
    virtual int run_test(struct pb_buffer *buffer) {
      pb_buffer_clear(buffer);

      pb_buffer_write_data(
        buffer,
        input1, strlen(input1));

      if (pb_buffer_trim(buffer, 10) != 10)
        return 1;

      if (pb_buffer_get_data_size(buffer) != 16)
        return 1;

      pb_buffer_byte_iterator byte_itr;
      pb_buffer_get_byte_iterator(buffer, &byte_itr);

      for (unsigned int i = 0; i < strlen(output); ++i) {
        if (*byte_itr.current_byte != output[i])
          return 1;

        pb_buffer_byte_iterator_next(buffer, &byte_itr);
      }

      return 0;
    }
};

const char *test_case_trim1::input1 = "abcdefghijklmnopqrstuvwxyz";
const char *test_case_trim1::output = "abcdefghijklmnop";



/*******************************************************************************
 */
int main(int argc, char **argv) {
  std::list<test_subject*> test_subjects;
  std::list<test_subject*>::iterator test_itr;

  struct pb_buffer_strategy strategy;
  memset(&strategy, 0, sizeof(strategy));

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = false;
  strategy.fragment_as_target = false;

  test_subjects.push_back(
    new test_subject(
      "Standard heap sourced pb_buffer                                       ",
      pb_trivial_buffer_create_with_strategy(&strategy)));

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = false;
  strategy.fragment_as_target = true;

  test_subjects.push_back(
    new test_subject(
      "Standard heap sourced pb_buffer, fragment_as_target                   ",
      pb_trivial_buffer_create_with_strategy(&strategy)));

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = true;
  strategy.fragment_as_target = false;

  test_subjects.push_back(
    new test_subject(
      "Standard heap sourced pb_buffer, clone_on_Write                       ",
      pb_trivial_buffer_create_with_strategy(&strategy)));

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = true;
  strategy.fragment_as_target = true;

  test_subjects.push_back(
    new test_subject(
      "Standard heap sourced pb_buffer, clone_on_Write and fragment_on_target",
      pb_trivial_buffer_create_with_strategy(&strategy)));

  char buffer_name[34];
  sprintf(buffer_name, "/tmp/pb_test_ops_buffer-%05d", getpid());

  test_subjects.push_back(
    new test_subject(
      "mmap file backed pb_buffer                                            ",
      pb_mmap_buffer_to_buffer(
        pb_mmap_buffer_create(
          buffer_name,
          pb_mmap_open_action_overwrite, pb_mmap_close_action_remove))));

  test_case<test_case_insert1>::run_test(test_subjects);
  test_case<test_case_insert2>::run_test(test_subjects);
  test_case<test_case_insert3>::run_test(test_subjects);
  test_case<test_case_overwrite1>::run_test(test_subjects);
  test_case<test_case_trim1>::run_test(test_subjects);

  while (!test_subjects.empty()) {
    delete test_subjects.back();
    test_subjects.pop_back();
  }

  return test_base::final_result;
}
