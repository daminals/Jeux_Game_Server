#include "option_processing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int options = 0x0;
int PORT = 0;

int option_processor(int argc, char* argv[]) {
  long opt;
  char *ptr;
  while ((opt = getopt(argc, argv, "p")) != -1) {
    switch (opt) {
      case 'p':
        options |= PORT_OPTION;
        PORT = strtol(argv[optind], &ptr, 10);
        break;
      default:
        return 1;
    }
  }
  if (options & PORT_OPTION) {
    return 0;
  }
  return 1;
}