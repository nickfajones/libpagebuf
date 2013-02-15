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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

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
void generate_stream_source_buf(char *source_buf, size_t source_buf_size) {
  for (size_t counter = 0; counter < source_buf_size; ++counter) {
    source_buf[counter] = 'a' + (random() % 26);
  }
}


/*******************************************************************************
 */
void read_stream(
    char *source_buf, size_t source_buf_size, char *buf, size_t buf_size) {
  size_t start = random();
  for (size_t counter = 0; counter < buf_size; ++counter) {
    buf[counter] = source_buf[(start + counter) % source_buf_size];
  }
}



/*******************************************************************************
 */
struct test_case {
  struct pb_buffer *buffer;

  struct test_case *next;

  char *description;

  EVP_MD_CTX mdctx;

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
};

struct test_case *test_case_create(void) {
  struct test_case *test_case = malloc(sizeof(struct test_case));
  if (!test_case)
    return NULL;

  test_case->buffer = NULL;

  test_case->next = NULL;

  test_case->description = NULL;

  EVP_MD_CTX_init(&test_case->mdctx);
  EVP_DigestInit_ex(&test_case->mdctx, EVP_md5(), NULL);

  memset(test_case->digest, 0, EVP_MAX_MD_SIZE);
  test_case->digest_len = 0;

  return test_case;
}

void test_cases_destroy(struct test_case *test_cases) {
  while (test_cases) {
    struct test_case *test_case = test_cases;
    test_cases = test_cases->next;

    if (test_case->buffer) {
      pb_buffer_destroy(test_case->buffer);

      test_case->buffer = NULL;
    }

    test_case->next = NULL;

    if (test_case->description) {
      free(test_case->description);

      test_case->description = NULL;
    }

    EVP_MD_CTX_cleanup(&test_case->mdctx);

    memset(test_case->digest, 0, EVP_MAX_MD_SIZE);
    test_case->digest_len = 0;

    free(test_case);
  }
}

struct test_case *test_cases_init(void) {
  struct test_case *test_head;
  struct test_case **test_case = &test_head;

  (*test_case) = test_case_create();
  if (!*test_case)
    return NULL;

  (*test_case)->buffer = pb_buffer_create();
  (*test_case)->description = strdup("Standard heap sourced pb_buffer");

  test_case = &(*test_case)->next;

  return test_head;
}



/*******************************************************************************
 */
#define STREAM_BUF_SIZE                                   (1024 * 32)

int main(int argc, char **argv) {
  char opt;
  uint32_t seed = (UINT16_MAX + 1);

  static char *stream_source_buf;
  static char *stream_buf;
  static uint64_t stream_buf_size;

  uint32_t iterations_limit;
  uint32_t iterations = 0;

  uint64_t total_write_size = 0;
  uint64_t total_read_size = 0;

  struct timeval start_time;
  struct timeval end_time;

  struct test_case *test_cases = NULL;
  struct test_case *test_itr = NULL;
  size_t test_case_count = 0;

  EVP_MD_CTX control_mdctx;

  unsigned char control_digest[EVP_MAX_MD_SIZE];
  unsigned int control_digest_len = 0;

  int retval = 0;

  while ((opt = getopt(argc, argv, "s:")) != -1) {
    switch (opt) {
      case 's':
        seed = atoi(optarg);
        break;
      default:
        printf("Usage: %s [-s seed]\n", argv[0]);
        return -1;;
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

  stream_source_buf = malloc(STREAM_BUF_SIZE);
  stream_buf = malloc(STREAM_BUF_SIZE);
  stream_buf_size = STREAM_BUF_SIZE;

  generate_stream_source_buf(stream_source_buf, STREAM_BUF_SIZE);

  iterations_limit = 50000 + (random() % 50000);

  printf("Iterations to run: %d\n", iterations_limit);

  EVP_MD_CTX_init(&control_mdctx);
  EVP_DigestInit_ex(&control_mdctx, EVP_md5(), NULL);

  test_cases = test_cases_init();
  if (!test_cases) {
    printf("Error creating test cases\n");

    return -1;
  }

  test_itr = test_cases;
  while (test_itr) {
    ++test_case_count;
    test_itr = test_itr->next;
  }

  gettimeofday(&start_time, NULL);

  while (iterations < iterations_limit) {
    size_t write_size;
    size_t read_size;
    bool use_direct;

    printf("\riteration: '%d'          ", iterations + 1);

    write_size = 64 + (random() % (4 * 1024));
    if (write_size > stream_buf_size) {
      free(stream_buf);
      stream_buf = malloc(write_size);
      stream_buf_size = write_size;
    }

    read_stream(stream_source_buf, STREAM_BUF_SIZE, stream_buf, write_size);

    EVP_DigestUpdate(&control_mdctx, stream_buf, write_size);

    use_direct = (random() & 0x1);

    test_itr = test_cases;
    while (test_itr) {
      uint64_t current_size;
      uint64_t written = 0;

      current_size = pb_buffer_get_data_size(test_itr->buffer);
      assert(current_size == (total_write_size - total_read_size));

      if (!use_direct) {
        written =
          pb_buffer_write_data(test_itr->buffer, stream_buf, write_size);

        assert(written == write_size);
      } else {
        uint64_t len;
        struct pb_iterator iterator;

        len = pb_buffer_reserve(test_itr->buffer, write_size);
        assert(len == write_size);

        pb_buffer_get_write_iterator(test_itr->buffer, &iterator);

        while ((len > 0) && pb_iterator_is_valid(&iterator)) {
          const struct pb_vec *write_vec = pb_iterator_get_vec(&iterator);
          size_t to_write = (len < write_vec->len) ? len : write_vec->len;

          memcpy(write_vec->base, stream_buf + written, to_write);

          written += to_write;
          len -= to_write;

          pb_iterator_next(&iterator);
        }

        assert(written == write_size);

        assert(pb_buffer_push(test_itr->buffer, write_size) == write_size);
      }

      current_size = pb_buffer_get_data_size(test_itr->buffer);
      assert(current_size == ((total_write_size + write_size) - total_read_size));

      test_itr = test_itr->next;
    }

    total_write_size += write_size;

    read_size = (random() % (total_write_size - total_read_size));
    if (read_size > stream_buf_size) {
      free(stream_buf);
      stream_buf = malloc(read_size);
      stream_buf_size = read_size;
    }

    use_direct = (random() & 0x1);

    test_itr = test_cases;
    while (test_itr) {
      uint64_t current_size;
      uint64_t readed = 0;

      current_size = pb_buffer_get_data_size(test_itr->buffer);
      assert(current_size == (total_write_size - total_read_size));

      if (!use_direct) {
        readed =
          pb_buffer_read_data(test_itr->buffer, stream_buf, read_size);

        assert(readed == read_size);

        EVP_DigestUpdate(&test_itr->mdctx, stream_buf, read_size);
      } else {
        uint64_t len = read_size;
        struct pb_iterator iterator;

        pb_buffer_get_data_iterator(test_itr->buffer, &iterator);

        while ((len > 0) && pb_iterator_is_valid(&iterator)) {
          const struct pb_vec *read_vec = pb_iterator_get_vec(&iterator);
          size_t to_read = (len < read_vec->len) ? len : read_vec->len;

          EVP_DigestUpdate(&test_itr->mdctx, read_vec->base, to_read);

          readed += to_read;
          len -= to_read;

          pb_iterator_next(&iterator);
        }

        assert(readed == read_size);
      }

      assert(pb_buffer_seek(test_itr->buffer, read_size) == read_size);

      current_size = pb_buffer_get_data_size(test_itr->buffer);
      assert(current_size == (total_write_size - (total_read_size + read_size)));

      test_itr = test_itr->next;
    }

    total_read_size += read_size;

    ++iterations;
  }

  test_itr = test_cases;
  while (test_itr) {
    uint64_t current_size;
    uint64_t readed = 0;
    struct pb_iterator iterator;

    current_size = pb_buffer_get_data_size(test_itr->buffer);
    assert(current_size == (total_write_size - total_read_size));

    pb_buffer_get_data_iterator(test_itr->buffer, &iterator);

    while (pb_iterator_is_valid(&iterator)) {
      const struct pb_vec *read_vec = pb_iterator_get_vec(&iterator);

      EVP_DigestUpdate(&test_itr->mdctx, read_vec->base, read_vec->len);

      readed += read_vec->len;

      pb_iterator_next(&iterator);
    }

    EVP_DigestFinal_ex(
      &test_itr->mdctx, test_itr->digest, &test_itr->digest_len);

    assert(readed == (total_write_size - total_read_size));

    assert(
      pb_buffer_seek(test_itr->buffer, readed) ==
        (total_write_size - total_read_size));

    current_size = pb_buffer_get_data_size(test_itr->buffer);
    assert(current_size == 0);

    test_itr = test_itr->next;
  }

  total_read_size = total_write_size;

  gettimeofday(&end_time, NULL);
  end_time.tv_sec -= start_time.tv_sec;
  if (end_time.tv_sec == 0)
    end_time.tv_sec = 1;

  EVP_DigestFinal_ex(&control_mdctx, control_digest, &control_digest_len);

  printf("\nControl digest: ");
  for (unsigned int i = 0; i < control_digest_len; i++)
    {
    printf("%02x", control_digest[i]);
    }
  printf("\n");

  test_itr = test_cases;
  while (test_itr) {
    bool digest_match =
      (memcmp(control_digest, test_itr->digest, control_digest_len) == 0);

    printf("Test digest: '%s': ", test_itr->description);
    for (unsigned int i = 0; i < test_itr->digest_len; i++)
      {
      printf("%02x", test_itr->digest[i]);
      }
    printf(" ...%s\n", (digest_match) ? "OK" : "ERROR");

    if (!digest_match)
      retval = -1;

    test_itr = test_itr->next;
  }

  printf("Total bytes transferred: %ld Bytes (%ldb/s)\n",
    (total_read_size * test_case_count),
    (total_read_size * test_case_count * 8) / end_time.tv_sec);

  test_cases_destroy(test_cases);
  test_cases = NULL;

  EVP_MD_CTX_cleanup(&control_mdctx);

  free(stream_buf);
  stream_buf = NULL;
  stream_buf_size = 0;

  free(stream_source_buf);
  stream_source_buf = NULL;

  return retval;
}

