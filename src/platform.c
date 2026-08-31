#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "platform.h"
#include <string.h>

#ifndef _WIN32
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>

static char *decode_os_release_value(char *value) {
  char quote = '\0';
  if (*value == '\'' || *value == '\"') {
    quote = *value++;
  }

  char *source = value;
  char *destination = value;
  while (*source != '\0') {
    if (quote != '\0' && *source == quote) {
      if (source[1] != '\0') {
        return NULL;
      }
      *destination = '\0';
      return value;
    }
    if (*source == '\\' && source[1] != '\0') {
      source++;
    }
    *destination++ = *source++;
  }

  *destination = '\0';
  return quote == '\0' ? value : NULL;
}

int detect_linux_distro_file(const char *path, char *distro, size_t size) {
  if (path == NULL || distro == NULL || size == 0) {
    return -1;
  }

  distro[0] = '\0';
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    return -1;
  }

  char *line = NULL;
  size_t line_size = 0;
  int result = -1;

  while (getline(&line, &line_size, fp) != -1) {
    if (strncmp(line, "ID=", 3) != 0) {
      continue;
    }

    char *id = line + 3;
    id[strcspn(id, "\r\n")] = '\0';

    /* os-release values may be quoted and use backslash escapes. */
    id = decode_os_release_value(id);
    if (id == NULL) {
      continue;
    }

    /* Keep detection usable even when an unusual ID exceeds the output buffer.
     */
    size_t len = strlen(id);
    size_t copy_size = len < size - 1 ? len : size - 1;
    memcpy(distro, id, copy_size);
    distro[copy_size] = '\0';
    result = 0;
    break;
  }

  free(line);
  if (fclose(fp) == EOF || result != 0) {
    return -1;
  }
  return 0;
}

static int detect_linux_distro(char *distro, size_t size) {
  if (distro == NULL || size == 0)
    return -1;

  /* Initialize the output buffer as an empty string. */
  distro[0] = '\0';

  errno = 0;
  if (detect_linux_distro_file("/etc/os-release", distro, size) == 0) {
    return 0;
  }

  /* /usr/lib/os-release is the specified fallback when /etc is absent. */
  if (errno == ENOENT) {
    return detect_linux_distro_file("/usr/lib/os-release", distro, size);
  }
  return -1;
}

#else

int detect_linux_distro_file(const char *path, char *distro, size_t size) {
  (void)path;
  if (distro == NULL || size == 0) {
    return -1;
  }
  distro[0] = '\0';
  return -1;
}

#endif

int detect_platform(struct platform *platform) {
  if (platform == NULL) {
    return -1;
  }

  platform->os = OS_UNKNOWN;
  platform->distro[0] = '\0';

#ifdef _WIN32
  platform->os = OS_WINDOWS;

#elif defined(__linux__)
  platform->os = OS_LINUX;
  if (detect_linux_distro(platform->distro, sizeof(platform->distro)) != 0) {
    goto fail;
  }

#elif defined(__FreeBSD__)
  platform->os = OS_FREEBSD;

#elif defined(__OpenBSD__)
  platform->os = OS_OPENBSD;

#elif defined(__NetBSD__)
  platform->os = OS_NETBSD;

#elif defined(__DragonFly__)
  platform->os = OS_DRAGONFLY;

#elif defined(__sun)
  platform->os = OS_SUNOS;

#elif defined(__APPLE__) && defined(__MACH__)
  platform->os = OS_MACOS;

#else
  goto fail;
#endif

  return 0;

fail:
  platform->os = OS_UNKNOWN;
  platform->distro[0] = '\0';
  return -1;
}

bool is_nixos(const struct platform *platform) {
  return platform != NULL && platform->os == OS_LINUX &&
         strcmp(platform->distro, "nixos") == 0;
}
