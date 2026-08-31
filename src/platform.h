#pragma once

#include <stdbool.h>
#include <stddef.h>

enum os_type {
  OS_UNKNOWN,
  OS_LINUX,
  OS_FREEBSD,
  OS_OPENBSD,
  OS_NETBSD,
  OS_DRAGONFLY,
  OS_SUNOS,
  OS_ILLUMOS,
  OS_MACOS,
  OS_WINDOWS,
};

struct platform {
  enum os_type os;
  char distro[64];
};

int detect_platform(struct platform *platform);
int detect_linux_distro_file(const char *path, char *distro, size_t size);
bool is_nixos(const struct platform *platform);
