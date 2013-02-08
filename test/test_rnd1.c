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
#define STREAM_BUF_SIZE                                   (1024 * 32)
static char stream_source_buf[STREAM_BUF_SIZE];
static char stream_buf[STREAM_BUF_SIZE];



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
void generate_stream_source_buf(void)
  {
  for (size_t counter = 0; counter < STREAM_BUF_SIZE; ++counter) {
    stream_source_buf[counter] = random();
    }
  }


/*******************************************************************************
 */
void read_stream(size_t len)
  {
  size_t skipper = random();
  for (size_t counter = 0;
       counter < len && counter < STREAM_BUF_SIZE;
       ++counter) {
    skipper = (skipper + random()) % STREAM_BUF_SIZE;

    stream_buf[counter] = stream_source_buf[skipper];
    }
  }

/*******************************************************************************
 */
int main(int argc, char **argv) {
  char opt;
  uint32_t seed = (UINT16_MAX + 1);

  struct pb_buffer *buffer;

  uint32_t iterations_limit;
  uint32_t iterations = 0;

  uint64_t bytes_written = 0;
  uint64_t bytes_readed = 0;

  EVP_MD_CTX mdctx;

  while ((opt = getopt(argc, argv, "s:")) != -1) {
    switch (opt)
      {
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

  generate_stream_source_buf();

  iterations_limit = 50000 + (random() % 50000);

  printf("Iterations to run: %d\n", iterations_limit);

  buffer = pb_buffer_create();

  EVP_MD_CTX_init(&mdctx);
  EVP_DigestInit_ex(&mdctx, EVP_md5(), NULL);

  while (iterations < iterations_limit) {
    size_t write_size;
    size_t read_size;

    printf("\riteration: '%d'          ", iterations + 1);

    write_size = 64 + (random() % (4 * 1024));
    if (write_size > STREAM_BUF_SIZE)
      write_size = STREAM_BUF_SIZE;

    read_stream(write_size);

    if (pb_buffer_write_data(buffer, stream_buf, write_size) != write_size) {
      assert(0);

      return -1;
      }

    bytes_written += write_size;

    read_size = (random() % (bytes_written - bytes_readed));
    if (read_size > STREAM_BUF_SIZE)
      read_size = STREAM_BUF_SIZE;

    if (pb_buffer_read_data(buffer, stream_buf, read_size) != read_size) {
      assert(0);

      return -1;
    }

    bytes_readed += read_size;

    ++iterations;
  }

  EVP_MD_CTX_cleanup(&mdctx);

  return 0;
}
