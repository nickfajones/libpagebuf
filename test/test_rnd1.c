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
    source_buf[counter] = random();
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

  bool use_direct;
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

  test_case->use_direct = false;
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
  (*test_case)->description = strdup("Standard pb_buffer, read/write interface");

  test_case = &(*test_case)->next;

  (*test_case) = test_case_create();
  if (!*test_case) {
    test_cases_destroy(test_head);

    return NULL;
  }

  (*test_case)->buffer = pb_buffer_create();
  (*test_case)->description = strdup("Standard pb_buffer, direct interface");
  (*test_case)->use_direct = true;

  test_case = &(*test_case)->next;

  return test_head;
}

bool test_case_write(struct test_case *test_case, void *buf, uint64_t len) {
  if (!test_case->use_direct) {
    assert(pb_buffer_write_data(test_case->buffer, buf, len) == len);
  } else {
    uint64_t written = 0;
    struct pb_iterator iterator;

    assert(pb_buffer_reserve(test_case->buffer, len) == len);

    pb_buffer_get_write_iterator(test_case->buffer, &iterator);

    while (pb_iterator_is_valid(&iterator)) {
      const struct pb_vec *write_vec = pb_iterator_get_vec(&iterator);

      memcpy(write_vec->base, buf + written, write_vec->len);

      written += write_vec->len;

      pb_iterator_next(&iterator);
    }

    assert(written == len);

    assert(pb_buffer_push(test_case->buffer, len) == len);
  }
}

bool test_case_read(struct test_case *test_case, void *buf, uint64_t len) {
  if (!test_case->use_direct) {
    assert(pb_buffer_read_data(test_case->buffer, buf, len) == len);
  } else {
    uint64_t readed = 0;
    struct pb_iterator iterator;

    pb_buffer_get_data_iterator(test_case->buffer, &iterator);

    while (pb_iterator_is_valid(&iterator)) {
      const struct pb_vec *read_vec = pb_iterator_get_vec(&iterator);

      memcpy(buf + readed, read_vec->base, read_vec->len);

      readed += read_vec->len;

      pb_iterator_next(&iterator);
    }

    assert(readed == len);

    assert(pb_buffer_seek(test_case->buffer, len) == len);
  }
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

  struct test_case *test_cases = NULL;
  struct test_case *test_itr = NULL;

  EVP_MD_CTX control_mdctx;

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
  }

  seed %= UINT16_MAX;

  printf("Using prng seed: '%d'\n", seed);

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

  while (iterations < iterations_limit) {
    size_t write_size;
    size_t read_size;

    printf("\riteration: '%d'          ", iterations + 1);

    write_size = 64 + (random() % (4 * 1024));
    if (write_size > stream_buf_size) {
      free(stream_buf);
      stream_buf = malloc(write_size);
      stream_buf_size = write_size;
    }

    read_stream(stream_source_buf, STREAM_BUF_SIZE, stream_buf, write_size);

    test_itr = test_cases;
    while (test_itr) {
      uint64_t current_size;
      
      current_size = pb_buffer_get_data_size(test_itr->buffer);
      assert(current_size != (total_write_size - total_read_size));

      test_case_write(test_itr, stream_buf, write_size);

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

    test_itr = test_cases;
    while (test_itr) {
      uint64_t current_size;

      current_size = pb_buffer_get_data_size(test_itr->buffer);
      assert(current_size == (total_write_size - total_read_size));

      test_case_read(test_itr, stream_buf, read_size);

      current_size = pb_buffer_get_data_size(test_itr->buffer);
      assert(current_size == (total_write_size - (total_read_size + read_size)));

      test_itr = test_itr->next;
    }

    total_read_size += read_size;

    ++iterations;
  }

  test_cases_destroy(test_cases);
  test_cases = NULL;

  EVP_MD_CTX_cleanup(&control_mdctx);

  free(stream_buf);
  stream_buf = NULL;
  stream_buf_size = 0;

  free(stream_source_buf);
  stream_source_buf = NULL;

  return 0;
}

