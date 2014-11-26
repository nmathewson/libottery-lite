
#include <unistd.h>
#include <fcntl.h>
#include "otterylite.h"

int main(int c, char **v)
{
  unsigned char buf[1024];
  (void)c; (void)v;

  while (1) {
    ottery_random_bytes(buf, 1024);
    write(1, buf, sizeof(buf));
  }
  return 0;
}

