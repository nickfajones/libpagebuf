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
class TestCase {
  public:
    TestCase(const std::string& description) :
      description(description),
      buffer1(NULL),
      buffer2(NULL),
      md5ctx(NULL),
      digest(NULL),
      digest_len(0) {
      buffer1 = pb_trivial_list_create();
      buffer2 = pb_trivial_list_create();

      md5ctx = new EVP_MD_CTX;

      EVP_MD_CTX_init(md5ctx);
      EVP_DigestInit_ex(md5ctx, EVP_md5(), NULL);

      digest = new unsigned char[EVP_MAX_MD_SIZE];
      memset(digest, 0, EVP_MAX_MD_SIZE);

      digest_len = 0;
    }

  private:
    TestCase(const TestCase& rvalue) {
    }

  public:
    ~TestCase() {
      if (buffer1) {
        buffer1->destroy(buffer1);

        buffer1 = NULL;
      }

      if (buffer2) {
        buffer2->destroy(buffer2);

        buffer2 = NULL;
      }

      if (md5ctx) {
        EVP_MD_CTX_cleanup(md5ctx);

        delete md5ctx;
        md5ctx = NULL;
      }

      if (digest) {
        delete [] digest;
      }

      digest_len = 0;
    }

  private:
    TestCase& operator=(const TestCase& rvalue) {
      return *this;
    }

  public:
    std::string description;

    struct pb_list *buffer1;
    struct pb_list *buffer2;

    EVP_MD_CTX *md5ctx;

    unsigned char *digest;
    unsigned int digest_len;
};



/*******************************************************************************
 */
#define STREAM_BUF_SIZE                                   (1024 * 32)

int main(int argc, char **argv) {
  char opt;
  uint32_t seed = (UINT16_MAX + 1);

  uint8_t *stream_source_buf;
  uint8_t *stream_buf;
  uint64_t stream_buf_size;

  long val;
  uint32_t iterations_limit;
  uint32_t iterations_min = 50000;
  uint32_t iterations_range = 50000;
  uint32_t iterations = 0;

  uint64_t current_size;
  uint64_t transfer_size;
  uint64_t read_size;

  uint64_t total_write_size = 0;
  uint64_t total_transfer_size = 0;
  uint64_t total_read_size = 0;

  uint8_t *read_buf;

  struct timeval start_time;
  struct timeval end_time;
  uint64_t millisecs;

  std::list<TestCase*> test_cases;
  std::list<TestCase*>::iterator test_itr;

  TestCase *test_case;

  EVP_MD_CTX control_mdctx;

  unsigned char control_digest[EVP_MAX_MD_SIZE];
  unsigned int control_digest_len = 0;

  int retval = 0;

  while ((opt = getopt(argc, argv, "i:r:s:")) != -1) {
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
        printf("Usage: %s [-i iterations_min] [-r iterations_max] [-s seed]\n", argv[0]);
        return -1;
    }
  }

  if (seed == (UINT16_MAX + 1)) {
    seed = generate_seed();
    if (seed == (UINT16_MAX + 1))
      return -1;

    printf("Using generated prng seed: '%d'\n", seed);
  } else {
    seed %= UINT16_MAX;

    printf("Using prng seed: '%d'\n", seed);
  }

  srandom(seed);

  stream_source_buf = new uint8_t[STREAM_BUF_SIZE + 1];
  stream_buf = new uint8_t[STREAM_BUF_SIZE + 1];
  stream_buf_size = STREAM_BUF_SIZE;

  generate_stream_source_buf(stream_source_buf, STREAM_BUF_SIZE);

  iterations_limit = iterations_min + (random() % iterations_range);

  printf("Iterations to run: %d\n", iterations_limit);

  EVP_MD_CTX_init(&control_mdctx);
  EVP_DigestInit_ex(&control_mdctx, EVP_md5(), NULL);

  test_cases.push_back(new TestCase("Standard heap sourced pb_buffer"));

  gettimeofday(&start_time, NULL);

  while (iterations < iterations_limit) {
    size_t write_size = 64 + (random() % (4 * 1024));
    if (write_size > stream_buf_size) {
      delete [] stream_buf;
      stream_buf = new uint8_t[write_size];
      stream_buf_size = write_size;
    }

    read_stream(stream_source_buf, STREAM_BUF_SIZE, stream_buf, write_size);

    EVP_DigestUpdate(&control_mdctx, stream_buf, write_size);

    transfer_size = random() % (total_write_size + write_size - total_transfer_size);

    read_size = random() & (total_transfer_size + transfer_size- total_read_size);

    for (test_itr = test_cases.begin();
         test_itr != test_cases.end();
         ++test_itr) {
      test_case = *test_itr;

      current_size = test_case->buffer1->get_data_size(test_case->buffer1);
      assert(current_size == (total_write_size - total_transfer_size));

      uint64_t written =
        test_case->buffer1->write_data(
          test_case->buffer1, stream_buf, write_size);
      assert(written == write_size);

      current_size = test_case->buffer1->get_data_size(test_case->buffer1);
      assert(current_size == (total_write_size + write_size - total_read_size));
    }

    total_write_size += write_size;

    for (test_itr = test_cases.begin();
         test_itr != test_cases.end();
         ++test_itr) {
      test_case = *test_itr;

      current_size = test_case->buffer2->get_data_size(test_case->buffer2);
      assert(current_size == (total_transfer_size - total_read_size));

      uint64_t transferred =
        test_case->buffer2->write_list(
          test_case->buffer2, test_case->buffer1, transfer_size);
      assert(transferred == transfer_size);

      current_size = test_case->buffer2->get_data_size(test_case->buffer2);
      assert(current_size == (total_transfer_size + transfer_size - total_write_size));

      uint64_t seeked =
        test_case->buffer1->seek(test_case->buffer1, transfer_size);
      assert(seeked == transfer_size);
    }

    total_transfer_size += transfer_size;

    read_buf = new uint8_t[read_size];

    for (test_itr = test_cases.begin();
         test_itr != test_cases.end();
         ++test_itr) {
      test_case = *test_itr;

      current_size = test_case->buffer2->get_data_size(test_case->buffer2);
      assert(current_size >= read_size);

      uint64_t readed =
        test_case->buffer2->read_data(
          test_case->buffer2, read_buf, read_size);
      assert(readed == read_size);

      EVP_DigestUpdate(test_case->md5ctx, read_buf, read_size);

      uint64_t seeked =
        test_case->buffer2->seek(test_case->buffer2, read_size);
      assert(seeked == read_size);

      current_size = test_case->buffer2->get_data_size(test_case->buffer2);
      assert(current_size == (total_transfer_size - total_read_size - read_size));
    }

    total_read_size += read_size;

    delete [] read_buf;

    ++iterations;
  }

  transfer_size = (total_write_size - total_transfer_size);

  for (test_itr = test_cases.begin();
       test_itr != test_cases.end();
       ++test_itr) {
    test_case = *test_itr;

    current_size = test_case->buffer1->get_data_size(test_case->buffer1);
    assert(current_size == transfer_size);

    uint64_t transferred =
      test_case->buffer2->write_list(
        test_case->buffer2, test_case->buffer1, transfer_size);
    assert(transferred == transfer_size);

    current_size = test_case->buffer2->get_data_size(test_case->buffer2);
    assert(current_size == (transfer_size + transferred));

    uint64_t seeked =
      test_case->buffer1->seek(test_case->buffer1, transfer_size);
    assert(seeked == transfer_size);
  }

  read_size = (total_transfer_size - total_read_size);
  read_buf = new uint8_t[read_size];

  for (test_itr = test_cases.begin();
       test_itr != test_cases.end();
       ++test_itr) {
    test_case = *test_itr;

    current_size = test_case->buffer2->get_data_size(test_case->buffer2);
    assert(current_size >= read_size);

    uint64_t readed =
      test_case->buffer2->read_data(
        test_case->buffer2, read_buf, read_size);
    assert(readed == read_size);

    EVP_DigestUpdate(test_case->md5ctx, read_buf, read_size);

    EVP_DigestFinal_ex(
      test_case->md5ctx, test_case->digest, &test_case->digest_len);

    uint64_t seeked =
      test_case->buffer2->seek(test_case->buffer2, read_size);
    assert(seeked == read_size);

    current_size = test_case->buffer2->get_data_size(test_case->buffer2);
    assert(current_size == 0);
  }

  delete [] read_buf;
  read_buf = NULL;

  EVP_DigestFinal_ex(&control_mdctx, control_digest, &control_digest_len);

  gettimeofday(&end_time, NULL);
  millisecs =
    ((end_time.tv_sec - start_time.tv_sec) * 1000000) -
    start_time.tv_usec +
    end_time.tv_usec;

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

  printf("Total bytes transferred: %ld Bytes (%ld bps)\n",
    (total_read_size * test_cases.size()),
    (total_read_size * test_cases.size() * 8 * 1000000) / millisecs);

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

