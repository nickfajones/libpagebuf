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
#include <stdio.h>

#include <pagebuf/pagebuf.h>


/*******************************************************************************
 */
#define STREAM_BUF_SIZE                                   (1024 * 32)
static char stream_source_buf[STREAM_BUF_SIZE];
static char stream_buf[STREAM_BUF_SIZE];



/*******************************************************************************
 */
uint32_t generate_seed() {
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
void generate_stream_source_buf()
  {
  for (size_t counter = 0; counter < STREAM_BUF_SIZE; ++counter)
    {
    stream_source_buf[counter] = random();
    }
  }


/*******************************************************************************
 */
void read_stream(size_t len)
  {
  size_t skipper = random();
  for (size_t counter = 0; counter < len && counter < STREAM_BUF_SIZE; ++counter)
    {
    skipper = (skipper + random()) % STREAM_BUF_SIZE;

    stream_buf[counter] = stream_source_buf[skipper];
    }
  }

/*******************************************************************************
 */
int main(int argc, char **argv) {
  char opt;
  uint32_t seed = (UINT16_MAX + 1);
  uint64_t bytes_written = 0;
  uint64_t bytes_readed = 0;

  while ((opt = getopt(argc, argv, "s:")) != -1)
    {
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

  printf("Uusing prng seed: '%d'\n", seed);

  srandom(seed);

  generate_stream_source_buf();



  return 0;
}
