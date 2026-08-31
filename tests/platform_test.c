#define _POSIX_C_SOURCE 200809L

#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_os_release(FILE *fp, const char *id) {
  assert(fprintf(fp, "NAME=Test OS\nID=%s\n", id) > 0);
  assert(fclose(fp) == 0);
}

int main(void) {
  char path[] = "/tmp/dotman-os-release.XXXXXX";
  int fd = mkstemp(path);
  assert(fd != -1);

  FILE *fp = fdopen(fd, "w");
  assert(fp != NULL);
  write_os_release(fp, "'nixos'");

  char distro[64];
  assert(detect_linux_distro_file(path, distro, sizeof(distro)) == 0);
  assert(strcmp(distro, "nixos") == 0);

  fp = fopen(path, "w");
  assert(fp != NULL);
  assert(fputs("ID=", fp) >= 0);
  for (size_t i = 0; i < 300; i++) {
    assert(fputc('a', fp) != EOF);
  }
  assert(fputc('\n', fp) != EOF);
  assert(fclose(fp) == 0);

  assert(detect_linux_distro_file(path, distro, sizeof(distro)) == 0);
  assert(strlen(distro) == sizeof(distro) - 1);
  assert(!is_nixos(NULL));
  assert(unlink(path) == 0);
  return 0;
}
