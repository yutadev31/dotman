#include "platform.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

int detect_linux_distro(char *distro, size_t size) {
  if (distro == NULL || size == 0)
    return -1;

  /* Initialize the output buffer as an empty string. */
  distro[0] = '\0';

  FILE *fp = fopen("/etc/os-release", "r");

  if (fp == NULL)
    return -1;

  char line[256];

  while (fgets(line, sizeof(line), fp) != NULL) {
    /* Look for the distribution identifier. */
    if (strncmp(line, "ID=", 3) != 0)
      continue;

    char *id = line + 3;

    /* Remove the trailing newline. */
    id[strcspn(id, "\r\n")] = '\0';

    /* Remove optional quotes. */
    if (id[0] == '"') {
      size_t len = strlen(id);

      if (len >= 2 && id[len - 1] == '"') {
        id[len - 1] = '\0';
        id++;
      }
    }

    /* Make sure the destination buffer is large enough. */
    if (strlen(id) >= size) {
      fclose(fp);
      return -1;
    }

    /* Copy the distribution identifier to the output buffer. */
    strcpy(distro, id);

    fclose(fp);
    return 0;
  }

  fclose(fp);
  return -1;
}

int detect_platform(struct platform *platform) {
  if (platform == NULL) {
    return -1;
  }

  platform->os = OS_UNKNOWN;
  platform->distro[0] = '\0';

  struct utsname uts;

  /* Detect the operating system. */
  if (uname(&uts) == -1) {
    return -1;
  }

  if (strcmp(uts.sysname, "Linux") == 0) {
    platform->os = OS_LINUX;
    if (detect_linux_distro(platform->distro, sizeof(platform->distro)) == -1)
      return -1;
    return 0;
  }

  if (strcmp(uts.sysname, "FreeBSD") == 0) {
    platform->os = OS_FREEBSD;
    return 0;
  }

  if (strcmp(uts.sysname, "OpenBSD") == 0) {
    platform->os = OS_OPENBSD;
    return 0;
  }

  if (strcmp(uts.sysname, "NetBSD") == 0) {
    platform->os = OS_NETBSD;
    return 0;
  }

  if (strcmp(uts.sysname, "DragonFly") == 0) {
    platform->os = OS_DRAGONFLY;
    return 0;
  }

  if (strcmp(uts.sysname, "SunOS") == 0) {
    platform->os = OS_ILLUMOS;
    return 0;
  }

  if (strcmp(uts.sysname, "Darwin") == 0) {
    platform->os = OS_MACOS;
    return 0;
  }

  return 0;
}

bool is_nixos(struct platform *platform) {
  return platform->os == OS_LINUX && strcmp(platform->distro, "nixos") == 0;
}
