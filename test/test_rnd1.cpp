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
#include <sys/stat.h>
#include <sys/time.h>
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
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

#include "pagebuf/pagebuf.hpp"
#include "pagebuf/pagebuf_mmap.hpp"


/*******************************************************************************
 */
uint32_t generate_seed(void) {
  uint8_t seed_buf[2];
  uint32_t seed;
  int fd;

  fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1) {
    printf("error opening random stream\n");

    return (UINT16_MAX + 1);
  }

  if (read(fd, seed_buf, 2) != 2) {
    printf("error reading from random stream\n");

    seed = (UINT16_MAX + 1);

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
class test_subject {
  public:
    test_subject() :
      buffer1(0),
      buffer2(0),
      md5ctx(0),
      digest(0),
      write_ref(false) {
    }

  public:
    ~test_subject() {
      digest_len = 0;

      if (digest) {
        delete [] digest;
      }

      if (md5ctx) {
        EVP_MD_CTX_cleanup(md5ctx);

        delete md5ctx;
        md5ctx = 0;
      }

      if (buffer2) {
        delete buffer2;
        buffer2 = 0;
      }

      if (buffer1) {
        delete buffer1;
        buffer1 = 0;
      }
    }

  public:
    void init(
        const std::string& _description,
        pb::buffer *_buffer1,
        pb::buffer *_buffer2,
        bool _write_ref) {
      description = _description;
      buffer1 = _buffer1;
      buffer2 = _buffer2;
      write_ref = _write_ref;

      md5ctx = new EVP_MD_CTX;
      EVP_MD_CTX_init(md5ctx);
      EVP_DigestInit_ex(md5ctx, EVP_md5(), 0);

      digest = new unsigned char[EVP_MAX_MD_SIZE];
      memset(digest, 0, EVP_MAX_MD_SIZE);

      digest_len = 0;
    }

  public:
    std::string description;

    pb::buffer *buffer1;
    pb::buffer *buffer2;

    EVP_MD_CTX *md5ctx;

    unsigned char *digest;
    unsigned int digest_len;

    bool write_ref;
};



/*******************************************************************************
 */
class data_profile {
  public:
    data_profile() :
      data(0),
      len(0) {
    }

    ~data_profile() {
      if (data) {
        delete [] data;
        data = 0;
      }

      len = 0;
    }

  public:
    void init(size_t _len) {
      data = new uint8_t[_len];
      len = _len;
    }

  public:
    uint8_t *data;
    size_t len;
};



/*******************************************************************************
 */
void write_data(
    test_subject& subject,
    uint8_t *stream_buf, uint64_t full_write_size,
    uint64_t total_write_size, uint64_t total_transfer_size) {
  uint64_t current_size = subject.buffer1->get_data_size();
  assert(current_size == (total_write_size - total_transfer_size));

  uint64_t written =
    (!subject.write_ref) ?
       subject.buffer1->write(stream_buf, full_write_size) :
       subject.buffer1->write_ref(stream_buf, full_write_size);
  assert(written == full_write_size);

  current_size = subject.buffer1->get_data_size();
  assert(current_size == (total_write_size + full_write_size - total_transfer_size));
}

void transfer_data(
    test_subject& subject,
    uint64_t transfer_size,
    uint64_t total_transfer_size, uint64_t total_read_size) {
  uint64_t current_size = subject.buffer2->get_data_size();
  assert(current_size == (total_transfer_size - total_read_size));

  uint64_t transferred =
    subject.buffer2->write(*subject.buffer1, transfer_size);
  assert(transferred == transfer_size);

  current_size = subject.buffer2->get_data_size();
  assert(current_size == (total_transfer_size + transfer_size - total_read_size));

  uint64_t seeked = subject.buffer1->seek(transfer_size);
  assert(seeked == transfer_size);
}

uint64_t read_data(
    test_subject& subject,
    uint8_t *read_buf, uint64_t read_size,
    uint64_t total_transfer_size, uint64_t total_read_size) {
  uint64_t current_size = subject.buffer2->get_data_size();
  assert(current_size >= read_size);

  uint64_t readed = subject.buffer2->read(read_buf, read_size);
  assert(readed == read_size);

  EVP_DigestUpdate(subject.md5ctx, read_buf, read_size);

  uint64_t seeked = subject.buffer2->seek(read_size);
  assert(seeked == read_size);

  current_size = subject.buffer2->get_data_size();
  assert(current_size == (total_transfer_size - total_read_size - read_size));

  return seeked;
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

  uint64_t total_write_size = 0;
  uint64_t total_transfer_size = 0;
  uint64_t total_read_size = 0;

  std::list<test_subject> test_subjects;
  std::list<test_subject>::iterator test_itr;

  std::list<data_profile> data_profiles;

  struct pb_buffer_strategy strategy;
  memset(&strategy, 0, sizeof(strategy));

  strategy.rejects_insert = false;

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = false;
  strategy.fragment_as_target = false;

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer                                                   ",
    new pb::buffer(&strategy),
    new pb::buffer(&strategy),
    false);

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer (write ref)                                       ",
    new pb::buffer(&strategy),
    new pb::buffer(&strategy),
    true);

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = false;
  strategy.fragment_as_target = true;

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, fragment_as_target                               ",
    new pb::buffer(&strategy),
    new pb::buffer(&strategy),
    false);

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, fragment_as_target (write ref)                   ",
    new pb::buffer(&strategy),
    new pb::buffer(&strategy),
    true);

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = true;
  strategy.fragment_as_target = false;

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, clone_on_Write                                   ",
    new pb::buffer(&strategy),
    new pb::buffer(&strategy),
    false);

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, clone_on_Write (write ref)                       ",
    new pb::buffer(&strategy),
    new pb::buffer(&strategy),
    true);

  strategy.page_size = PB_BUFFER_DEFAULT_PAGE_SIZE;
  strategy.clone_on_write = true;
  strategy.fragment_as_target = true;

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, clone_on_Write and fragment_on_target            ",
    new pb::buffer(&strategy),
    new pb::buffer(&strategy),
    false);

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "Standard heap sourced pb_buffer, clone_on_Write and fragment_on_target (write ref)",
    new pb::buffer(&strategy),
    new pb::buffer(&strategy),
    true);

  char buffer1_name[34];
  char buffer2_name[34];

  sprintf(buffer1_name, "/tmp/pb_test_rnd1_buffer1-%05d-1", getpid());
  sprintf(buffer2_name, "/tmp/pb_test_rnd1_buffer1-%05d-2", getpid());

  test_subjects.push_back(test_subject());
  test_subjects.back().init(
    "mmap file backed pb_buffer                                                        ",
    new pb::mmap_buffer(
      buffer1_name,
      pb::mmap_buffer::open_action_overwrite,
      pb::mmap_buffer::close_action_remove),
    new pb::mmap_buffer(
      buffer2_name,
      pb::mmap_buffer::open_action_overwrite,
      pb::mmap_buffer::close_action_remove),
    false);

  EVP_MD_CTX control_mdctx;

  unsigned char control_digest[EVP_MAX_MD_SIZE];
  unsigned int control_digest_len = 0;

  EVP_MD_CTX_init(&control_mdctx);
  EVP_DigestInit_ex(&control_mdctx, EVP_md5(), 0);

  struct timeval start_time;
  struct timeval end_time;

  gettimeofday(&start_time, 0);

  while (iterations < iterations_limit) {
    data_profiles.push_back(data_profile());
    data_profiles.back().init(64 + (random() % (4 * 1024)));

    uint8_t *write_buf = data_profiles.back().data;
    size_t write_size = data_profiles.back().len;

    read_stream(stream_source_buf, STREAM_BUF_SIZE, write_buf, write_size);

    EVP_DigestUpdate(&control_mdctx, write_buf, write_size);

    uint64_t transfer_size =
      random() % (total_write_size + write_size - total_transfer_size);

    for (test_itr = test_subjects.begin();
         test_itr != test_subjects.end();
         ++test_itr) {
      write_data(
        *test_itr,
        write_buf, write_size,
        total_write_size, total_transfer_size);
    }

    total_write_size += write_size;

    for (test_itr = test_subjects.begin();
         test_itr != test_subjects.end();
         ++test_itr) {
      transfer_data(
        *test_itr,
        transfer_size,
        total_transfer_size, total_read_size);
    }

    total_transfer_size += transfer_size;

    uint64_t read_size = random() & (total_transfer_size  - total_read_size);
    uint8_t *read_buf = new uint8_t[read_size];

    for (test_itr = test_subjects.begin();
         test_itr != test_subjects.end();
         ++test_itr) {
      uint64_t seek_size = read_data(
        *test_itr,
        read_buf, read_size,
        total_transfer_size, total_read_size);

      assert(seek_size == read_size);
    }

    total_read_size += read_size;

    while (read_size > 0) {
      if (read_size >= data_profiles.front().len) {
        read_size -= data_profiles.front().len;

        data_profiles.pop_front();
      } else {
        data_profiles.front().len -= read_size;

        read_size = 0;
      }
    }

    delete [] read_buf;

    ++iterations;
  }

  while (total_transfer_size < total_write_size) {
    uint64_t transfer_size =
      random() % (total_write_size - total_transfer_size);

    uint64_t read_size =
      random() & (total_transfer_size + transfer_size- total_read_size);

    if (transfer_size < 1024) {
      transfer_size = (total_write_size - total_transfer_size);

      read_size = (total_transfer_size + transfer_size - total_read_size);
    }

    for (test_itr = test_subjects.begin();
         test_itr != test_subjects.end();
         ++test_itr) {
      transfer_data(
        *test_itr,
        transfer_size,
        total_transfer_size, total_read_size);
    }

    total_transfer_size += transfer_size;

    uint8_t *read_buf = new uint8_t[read_size];

    for (test_itr = test_subjects.begin();
         test_itr != test_subjects.end();
         ++test_itr) {
      uint64_t seek_size = read_data(
        *test_itr,
        read_buf, read_size,
        total_transfer_size, total_read_size);

      assert(seek_size == read_size);
    }

    total_read_size += read_size;

    while (read_size > 0) {
      if (read_size >= data_profiles.front().len) {
        read_size -= data_profiles.front().len;

        data_profiles.pop_front();
      } else {
        data_profiles.front().len -= read_size;

        read_size = 0;
      }
    }

    delete [] read_buf;
  }

  assert(total_write_size == total_transfer_size);
  assert(total_transfer_size == total_read_size);

  EVP_DigestFinal_ex(&control_mdctx, control_digest, &control_digest_len);

  gettimeofday(&end_time, 0);

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

  for (test_itr = test_subjects.begin();
       test_itr != test_subjects.end();
       ++test_itr) {
    test_subject &subject = *test_itr;

    assert(subject.buffer1->get_data_size() == 0);
    assert(subject.buffer2->get_data_size() == 0);

    EVP_DigestFinal_ex(
      subject.md5ctx, subject.digest, &subject.digest_len);

    assert(subject.digest_len == control_digest_len);

    bool digest_match =
      (memcmp(control_digest, subject.digest, control_digest_len) == 0);

    printf("Test digest: '%s': ", subject.description.c_str());
    for (unsigned int i = 0; i < subject.digest_len; i++)
      {
      printf("%02x", subject.digest[i]);
      }
    printf(" ... %s\n", (digest_match) ? "OK" : "ERROR");

    if (!digest_match)
      retval = -1;
  }

  printf(
    "Total bytes transferred: %" PRIu64 " Bytes (%" PRIu64 " bps)\n",
      (total_read_size * test_subjects.size()),
      (total_read_size * test_subjects.size() * 8 * 1000) / millisecs);

  test_subjects.clear();

  EVP_MD_CTX_cleanup(&control_mdctx);

  delete [] stream_source_buf;
  stream_source_buf = 0;

  return retval;
}

