
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "otterylite.h"

int main(int argc, char **argv)
{
  unsigned char buf[1024];

  if (argc == 1 || strcmp(argv[1], "--yes-really"))
    {
      fprintf(stderr, "Hi! This program is designed to spew an infinite stream "
              "of random bytes to stdout, for testing. If you just called it "
              "wondering what it did... that's what. If you really want to blit "
              "randomness to stderr, say --yes-really\n");
      return 1;
    }

  while (1)
    {
      ottery_random_buf(buf, 1024);
      write(1, buf, sizeof(buf));
    }
  return 0;
}

