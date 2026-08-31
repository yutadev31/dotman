#pragma once

#include <stdbool.h>

enum os_type {
  OS_UNKNOWN,
  OS_LINUX,
  OS_FREEBSD,
  OS_OPENBSD,
  OS_NETBSD,
  OS_DRAGONFLY,
  OS_ILLUMOS,
  OS_MACOS,
};

struct platform {
  enum os_type os;
  char distro[64];
};

int detect_platform(struct platform *platform);
bool is_nixos(struct platform *platform);
