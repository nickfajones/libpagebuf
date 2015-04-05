/*******************************************************************************
 *  Copyright 2013 Nick Jones <nick.fa.jones@gmail.com>
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
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <cstdint>
#include <string>
#include <list>

#include <openssl/evp.h>

#include <pagebuf/pagebuf.h>


/*******************************************************************************
 */
uint32_t generate_seed(void) {
  uint8_t seed_buf[2];
  uint32_t seed;
  int fd;

  fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1) {
    printf("error opening random stream\n");

    return (UINT32_MAX + 1);
  }

  if (read(fd, seed_buf, 2) != 2) {
    printf("error reading from random stream\n");

    seed = (UINT32_MAX + 1);

    goto close_fd;
  }

  seed = ((uint32_t)seed_buf[1] << 8) + (uint32_t)seed_buf[0];

close_fd:
  close(fd);

  return seed;
}

/*******************************************************************************
 */
void generate_stream_source_buf(uint8_t *source_buf, size_t source_buf_size) {
  for (size_t counter = 0; counter < source_buf_size; ++counter) {
    source_buf[counter] = 'a' + (random() % 26);
  }
}


/*******************************************************************************
 */
void read_stream(
    uint8_t *source_buf, size_t source_buf_size, uint8_t *buf, size_t buf_size) {
  size_t start = random();
  for (size_t counter = 0; counter < buf_size; ++counter) {
    buf[counter] = source_buf[(start + counter) % source_buf_size];
  }
}



/*******************************************************************************
 */
class TestCase {
  public:
    TestCase(
        const std::string& description, const pb_list_strategy& strategy) :
      description(description),
      buffer1(pb_trivial_list_create_with_strategy(&strategy)),
      buffer2(pb_trivial_list_create_with_strategy(&strategy)),
      line_reader(pb_trivial_line_reader_create(buffer2)),
      md5ctx(new EVP_MD_CTX),
      digest(new unsigned char[EVP_MAX_MD_SIZE]),
      digest_len(0) {
      EVP_MD_CTX_init(md5ctx);
      EVP_DigestInit_ex(md5ctx, EVP_md5(), NULL);

      memset(digest, 0, EVP_MAX_MD_SIZE);
    }

    ~TestCase() {
      digest_len = 0;

      if (digest) {
        delete [] digest;
      }

      if (md5ctx) {
        EVP_MD_CTX_cleanup(md5ctx);

        delete md5ctx;
        md5ctx = NULL;
      }

      if (line_reader) {
        line_reader->destroy(line_reader);

        line_reader = NULL;
      }

      if (buffer2) {
        buffer2->destroy(buffer2);

        buffer2 = NULL;
      }

      if (buffer1) {
        buffer1->destroy(buffer1);

        buffer1 = NULL;
      }
    }

  public:
    std::string description;

    struct pb_list *buffer1;
    struct pb_list *buffer2;
    struct pb_line_reader *line_reader;

    EVP_MD_CTX *md5ctx;

    unsigned char *digest;
    unsigned int digest_len;
};



/*******************************************************************************
 */
class LineProfile {
  public:
    LineProfile(size_t len, bool has_cr) :
      len(len),
      full_len(len  + ((has_cr) ? 2 : 1)),
      has_cr(has_cr) {
    }

  public:
    ~LineProfile() {
      len = 0;
      full_len = 0;
      has_cr = false;
    }

  public:
    size_t len;
    size_t full_len;

    bool has_cr;
};



/*******************************************************************************
 */
void write_line(
    TestCase* test_case,
    uint8_t *stream_buf, uint64_t full_write_size,
    uint64_t total_write_size, uint64_t total_transfer_size) {
  uint64_t current_size = test_case->buffer1->get_data_size(test_case->buffer1);
  assert(current_size == (total_write_size - total_transfer_size));

  uint64_t written =
    test_case->buffer1->write_data(
      test_case->buffer1, stream_buf, full_write_size);
  assert(written == full_write_size);

  current_size = test_case->buffer1->get_data_size(test_case->buffer1);
  assert(current_size == (total_write_size + full_write_size - total_transfer_size));
}

void transfer_data(
    TestCase* test_case,
    uint64_t transfer_size,
    uint64_t total_transfer_size, uint64_t total_read_size) {
  uint64_t current_size = test_case->buffer2->get_data_size(test_case->buffer2);
  assert(current_size == (total_transfer_size - total_read_size));

  uint64_t transferred =
    test_case->buffer2->write_list(
      test_case->buffer2, test_case->buffer1, transfer_size);
  assert(transferred == transfer_size);

  current_size = test_case->buffer2->get_data_size(test_case->buffer2);
  assert(current_size == (total_transfer_size + transfer_size - total_read_size));

  uint64_t seeked =
    test_case->buffer1->seek(test_case->buffer1, transfer_size);
  assert(seeked == transfer_size);
}

uint64_t read_lines(
    TestCase* test_case,
    std::list<LineProfile*>& line_profiles,
    uint8_t *read_buf, uint64_t read_buf_size,
    uint64_t total_transfer_size, uint64_t total_read_size) {
  uint64_t seek_size = 0;

  uint64_t current_size = test_case->buffer2->get_data_size(test_case->buffer2);

  std::list<LineProfile*>::iterator profiles_itr = line_profiles.begin();

  while (test_case->line_reader->has_line(test_case->line_reader)) {
    LineProfile *line_profile = *profiles_itr;

    uint64_t line_len =
      test_case->line_reader->get_line_len(test_case->line_reader);
    assert(line_len == line_profile->len);

    assert(test_case->line_reader->is_crlf(test_case->line_reader) == line_profile->has_cr);

    assert(read_buf_size >= line_profile->len);

    test_case->line_reader->get_line_data(
      test_case->line_reader, read_buf, line_profile->len);
    EVP_DigestUpdate(test_case->md5ctx, read_buf, line_profile->len);

    uint64_t seeked =
      test_case->line_reader->seek_line(test_case->line_reader);
    assert(seeked == line_profile->full_len);

    current_size = test_case->buffer2->get_data_size(test_case->buffer2);
    assert(current_size == (total_transfer_size - total_read_size - seek_size - seeked));

    seek_size += seeked;

    ++profiles_itr;
  }

  return seek_size;
}



/*******************************************************************************
 */
#define STREAM_BUF_SIZE                                   (1024 * 32)

int main(int argc, char **argv) {
  int retval = 0;

  uint32_t seed = (UINT32_MAX + 1);

  uint32_t iterations = 0;
  uint32_t iterations_min = 50000;
  uint32_t iterations_range = 50000;

  char opt;
  while ((opt = getopt(argc, argv, "i:r:s:")) != -1) {
    long val;

    switch (opt) {
      case 'i':
        val = atol(optarg);
        if (val > 0)
          iterations_min = val;
        break;
      case 'r':
        val = atol(optarg);
        if (val > 0)
          iterations_range = val;
        break;
      case 's':
        seed = atol(optarg);
        break;
      default:
        printf("Usage: %s [-i iterations_min] [-r iterations_range] [-s seed]\n", argv[0]);
        return -1;
    }
  }

  if (seed == (UINT32_MAX + 1)) {
    seed = generate_seed();
    if (seed == (UINT16_MAX + 1))
      return -1;

    printf("Using generated prng seed: '%d'\n", seed);
  } else {
    seed %= UINT16_MAX;

    printf("Using prng seed: '%d'\n", seed);
  }

  srandom(seed);

  uint32_t iterations_limit = iterations_min + (random() % iterations_range);

  printf("Iterations to run: %d\n", iterations_limit);

  uint8_t *stream_source_buf = new uint8_t[STREAM_BUF_SIZE + 1];
  generate_stream_source_buf(stream_source_buf, STREAM_BUF_SIZE);

  uint8_t *stream_buf = NULL;
  uint64_t stream_buf_size = 0;

  uint64_t total_write_size = 0;
  uint64_t total_transfer_size = 0;
  uint64_t total_read_size = 0;

  std::list<TestCase*> test_cases;
  std::list<TestCase*>::iterator test_itr;
  TestCase *test_case;

  std::list<LineProfile*> line_profiles;

  struct pb_list_strategy strategy;

  strategy.page_size = PB_TRIVIAL_LIST_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = false;
  strategy.fragment_as_target = false;

  test_cases.push_back(
    new TestCase(
      "Standard heap sourced pb_buffer                                       ",
      strategy));

  strategy.page_size = PB_TRIVIAL_LIST_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = false;
  strategy.fragment_as_target = true;

  test_cases.push_back(
    new TestCase(
      "Standard heap sourced pb_buffer, fragment_as_target                   ",
      strategy));

  strategy.page_size = PB_TRIVIAL_LIST_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = true;
  strategy.fragment_as_target = false;

  test_cases.push_back(
    new TestCase(
      "Standard heap sourced pb_buffer, clone_on_Write                       ",
      strategy));

  strategy.page_size = PB_TRIVIAL_LIST_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = true;
  strategy.fragment_as_target = true;

  test_cases.push_back(
    new TestCase(
      "Standard heap sourced pb_buffer, clone_on_Write and fragment_on_target",
      strategy));

  EVP_MD_CTX control_mdctx;

  unsigned char control_digest[EVP_MAX_MD_SIZE];
  unsigned int control_digest_len = 0;

  EVP_MD_CTX_init(&control_mdctx);
  EVP_DigestInit_ex(&control_mdctx, EVP_md5(), NULL);

  struct timeval start_time;
  struct timeval end_time;

  gettimeofday(&start_time, NULL);

  while (iterations < iterations_limit) {
    line_profiles.push_back(
      new LineProfile((random() % 1024), ((random() % 2) == 1)));

    size_t write_size = line_profiles.back()->len;
    size_t full_write_size = line_profiles.back()->full_len;
    bool write_has_cr = line_profiles.back()->has_cr;

    if ((full_write_size) > stream_buf_size) {
      if (stream_buf)
        delete [] stream_buf;
      stream_buf = new uint8_t[full_write_size];
      stream_buf_size = full_write_size;
    }

    read_stream(stream_source_buf, STREAM_BUF_SIZE, stream_buf, write_size);

    EVP_DigestUpdate(&control_mdctx, stream_buf, write_size);

    if (write_has_cr) {
      stream_buf[write_size] = '\r';
      stream_buf[write_size + 1] = '\n';
    } else {
      stream_buf[write_size] = '\n';
    }

    uint64_t transfer_size =
      random() % (total_write_size + full_write_size - total_transfer_size);

    for (test_itr = test_cases.begin();
         test_itr != test_cases.end();
         ++test_itr) {
      test_case = *test_itr;

      write_line(
        test_case,
        stream_buf, full_write_size,
        total_write_size, total_transfer_size);
    }

    total_write_size += full_write_size;

    for (test_itr = test_cases.begin();
         test_itr != test_cases.end();
         ++test_itr) {
      test_case = *test_itr;

      transfer_data(
        test_case,
        transfer_size,
        total_transfer_size, total_read_size);
    }

    total_transfer_size += transfer_size;

    uint64_t read_size = 0;
    size_t complete_counter = 0;

    for (std::list<LineProfile*>::iterator profiles_itr = line_profiles.begin();
         profiles_itr != line_profiles.end();
         ++profiles_itr) {
      LineProfile *line_profile = *profiles_itr;

      if ((total_transfer_size - total_read_size) <
          (line_profile->full_len + read_size)) {
        break;
      }

      read_size += line_profile->full_len;

      ++complete_counter;
    }

    for (test_itr = test_cases.begin();
         test_itr != test_cases.end();
         ++test_itr) {
      test_case = *test_itr;

      uint64_t seek_size = read_lines(
        test_case,
        line_profiles,
        stream_buf, stream_buf_size,
        total_transfer_size, total_read_size);

      assert(seek_size == read_size);
    }

    total_read_size += read_size;

    while (complete_counter > 0) {
      LineProfile *line_profile = line_profiles.front();
      line_profiles.pop_front();

      delete line_profile;

      --complete_counter;
    }

    ++iterations;
  }

  while (total_transfer_size < total_write_size) {
    uint64_t transfer_size =
      random() % (total_write_size - total_transfer_size);
    if (transfer_size < 1024)
      transfer_size = (total_write_size - total_transfer_size);

    for (test_itr = test_cases.begin();
         test_itr != test_cases.end();
         ++test_itr) {
      test_case = *test_itr;

      transfer_data(
        test_case,
        transfer_size,
        total_transfer_size, total_read_size);
    }

    total_transfer_size += transfer_size;

    uint64_t read_size = 0;
    size_t complete_counter = 0;

    for (std::list<LineProfile*>::iterator profiles_itr = line_profiles.begin();
        profiles_itr != line_profiles.end();
         ++profiles_itr) {
      LineProfile *line_profile = *profiles_itr;

      if ((total_transfer_size - total_read_size) <
          (line_profile->full_len + read_size)) {
        break;
      }

      read_size += line_profile->full_len;

      ++complete_counter;
    }

    for (test_itr = test_cases.begin();
         test_itr != test_cases.end();
         ++test_itr) {
      test_case = *test_itr;

      uint64_t seek_size = read_lines(
        test_case,
        line_profiles,
        stream_buf, stream_buf_size,
        total_transfer_size, total_read_size);

      assert(seek_size == read_size);
    }

    while (complete_counter > 0) {
      LineProfile *line_profile = line_profiles.front();
      line_profiles.pop_front();

      delete line_profile;

      --complete_counter;
    }

    total_read_size += read_size;
  }

  assert(total_write_size == total_transfer_size);
  assert(total_transfer_size == total_read_size);

  EVP_DigestFinal_ex(&control_mdctx, control_digest, &control_digest_len);

  gettimeofday(&end_time, NULL);

  uint64_t millisecs =
    ((end_time.tv_sec - start_time.tv_sec) * 1000) +
    ((end_time.tv_usec + start_time.tv_usec) / 1000);
  if (millisecs == 0)
    {
    millisecs = 1;
    }

  printf("Done...\nControl digest: ");
  for (unsigned int i = 0; i < control_digest_len; i++)
    {
    printf("%02x", control_digest[i]);
    }
  printf("\n");

  for (test_itr = test_cases.begin();
       test_itr != test_cases.end();
       ++test_itr) {
    test_case = *test_itr;

    assert(test_case->buffer1->get_data_size(test_case->buffer1) == 0);
    assert(test_case->buffer2->get_data_size(test_case->buffer2) == 0);

    EVP_DigestFinal_ex(
      test_case->md5ctx, test_case->digest, &test_case->digest_len);

    assert(test_case->digest_len == control_digest_len);

    bool digest_match =
      (memcmp(control_digest, test_case->digest, control_digest_len) == 0);

    printf("Test digest: '%s': ", test_case->description.c_str());
    for (unsigned int i = 0; i < test_case->digest_len; i++)
      {
      printf("%02x", test_case->digest[i]);
      }
    printf(" ... %s\n", (digest_match) ? "OK" : "ERROR");

    if (!digest_match)
      retval = -1;
  }

  printf(
    "Total bytes transferred: %ld Bytes (%ld bps)\n",
      (total_read_size * test_cases.size()),
      (total_read_size * test_cases.size() * 8 * 1000) / millisecs);

  while (!test_cases.empty()) {
    delete test_cases.back();
    test_cases.back() = NULL;
    test_cases.pop_back();
  }

  EVP_MD_CTX_cleanup(&control_mdctx);

  delete [] stream_buf;
  stream_buf = NULL;
  stream_buf_size = 0;

  delete [] stream_source_buf;
  stream_source_buf = NULL;

  return retval;
}

