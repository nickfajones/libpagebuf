/*******************************************************************************
 *  Copyright 2015 Nick Jones <nick.fa.jones@gmail.com>
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
    test_subject(const std::string& description, struct pb_buffer *buffer) :
      description(description),
      buffer(buffer) {
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
template <typename T>
class test_case {
  public:
    static int result;

  public:
    virtual int run_test(struct pb_buffer *buffer) = 0;

  public:
    static void run_test(const std::list<test_subject*>& test_subjects) {
      T test_case;

      for (std::list<test_subject*>::const_iterator itr = test_subjects.begin();
           itr != test_subjects.end();
           ++itr) {
        result |= test_case.run_test((*itr)->buffer);
      }
    }
};

template <typename T>
int test_case<T>::result = 0;



/*******************************************************************************
 */
class test_case_insert1 : public test_case<test_case_insert1> {
  public:
    static constexpr const char *input1 = "abcdejklmnopqrstuvwxyz";
    static constexpr const char *input2 = "fghi";
    static constexpr const char *output = "abcdefghijklmnopqrstuvwxyz";

  public:
    virtual int run_test(struct pb_buffer *buffer) {
      pb_buffer_clear(buffer);

      pb_buffer_write_data(
        buffer,
        reinterpret_cast<const u_int8_t*>(input1), strlen(input1));

      pb_buffer_iterator buf_itr;
      pb_buffer_get_iterator(buffer, &buf_itr);

      pb_buffer_insert_data(
        buffer,
        &buf_itr, 5,
        reinterpret_cast<const uint8_t*>(input2), 4);

      if (pb_buffer_get_data_size(buffer) != 26)
        return 1;

      pb_buffer_byte_iterator byte_itr;
      pb_buffer_get_byte_iterator(buffer, &byte_itr);

      for (int i = 0; i < 26; ++i) {
        if (*byte_itr.current_byte != output[i])
          return 1;

        pb_buffer_byte_iterator_next(buffer, &byte_itr);
      }

      return 0;
    }
};



/*******************************************************************************
 */
int main(int argc, char **argv) {

  std::list<test_subject*> test_subjects;
  std::list<test_subject*>::iterator test_itr;

  struct pb_buffer_strategy strategy;
  memset(&strategy, 0, sizeof(strategy));

  strategy.rejects_insert = false;

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

  char buffer1_name[34];
  char buffer2_name[34];

  sprintf(buffer1_name, "/tmp/pb_test_rnd2_buffer1-%05d-1", getpid());
  sprintf(buffer2_name, "/tmp/pb_test_rnd2_buffer1-%05d-2", getpid());

  test_subjects.push_back(
    new test_subject(
      "mmap file backed pb_buffer                                            ",
      pb_mmap_buffer_create(
        buffer1_name,
        pb_mmap_open_action_overwrite, pb_mmap_close_action_remove)));

  test_case<test_case_insert1>::run_test(test_subjects);

  while (!test_subjects.empty()) {
    delete test_subjects.back();
    test_subjects.back() = NULL;
    test_subjects.pop_back();
  }

  return 0;
}