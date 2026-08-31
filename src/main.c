#include "platform.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct path_list {
  char **items;
  size_t len;
  size_t cap;
};

struct config {
  bool gui;
  bool vm;
};

struct options {
  const char *repo;
  bool dry_run;
};

static void free_paths(struct path_list *list) {
  for (size_t i = 0; i < list->len; i++)
    free(list->items[i]);
  free(list->items);
}

static int add_path(struct path_list *list, const char *path) {
  if (list->len == list->cap) {
    size_t cap = list->cap == 0 ? 16 : list->cap * 2;
    char **items = realloc(list->items, cap * sizeof(*items));
    if (items == NULL) {
      perror("realloc");
      return -1;
    }
    list->items = items;
    list->cap = cap;
  }
  list->items[list->len] = strdup(path);
  if (list->items[list->len] == NULL) {
    perror("strdup");
    return -1;
  }
  list->len++;
  return 0;
}

static bool valid_path(const char *path) {
  if (*path == '/' || *path == '\0')
    return false;
  const char *part = path;
  for (const char *p = path;; p++) {
    if (*p != '/' && *p != '\0')
      continue;
    size_t len = (size_t)(p - part);
    if (len == 0 || (len == 1 && part[0] == '.') ||
        (len == 2 && part[0] == '.' && part[1] == '.'))
      return false;
    if (*p == '\0')
      return true;
    part = p + 1;
  }
}

static bool paths_overlap(const char *a, const char *b) {
  size_t a_len = strlen(a), b_len = strlen(b);
  return strcmp(a, b) == 0 || (strncmp(a, b, a_len) == 0 && b[a_len] == '/') ||
         (strncmp(b, a, b_len) == 0 && a[b_len] == '/');
}

static int path_for(char *buffer, size_t size, const char *prefix,
                    const char *path) {
  if (snprintf(buffer, size, "%s/%s", prefix, path) >= (int)size) {
    fprintf(stderr, "Path too long: %s/%s\n", prefix, path);
    return -1;
  }
  return 0;
}

static int mkdir_parents(const char *path) {
  char buffer[PATH_MAX];
  if (snprintf(buffer, sizeof(buffer), "%s", path) >= (int)sizeof(buffer))
    return -1;
  char *slash = strrchr(buffer, '/');
  if (slash == NULL)
    return 0;
  *slash = '\0';
  for (char *p = buffer + 1; *p != '\0'; p++) {
    if (*p != '/')
      continue;
    *p = '\0';
    if (mkdir(buffer, 0700) == -1 && errno != EEXIST)
      goto fail;
    *p = '/';
  }
  if (mkdir(buffer, 0700) == -1 && errno != EEXIST)
    goto fail;
  return 0;
fail:
  fprintf(stderr, "mkdir: %s: %s\n", buffer, strerror(errno));
  return -1;
}

static bool is_managed_link(const char *source, const char *destination) {
  char resolved_source[PATH_MAX], resolved_destination[PATH_MAX];
  return realpath(source, resolved_source) != NULL &&
         realpath(destination, resolved_destination) != NULL &&
         strcmp(resolved_source, resolved_destination) == 0;
}

static char *trim(char *value) {
  while (*value == ' ' || *value == '\t')
    value++;
  char *end = value + strlen(value);
  while (end > value && (end[-1] == ' ' || end[-1] == '\t'))
    *--end = '\0';
  return value;
}

static int parse_bool(const char *name, const char *value, bool *result) {
  if (strcmp(value, "yes") == 0) {
    *result = true;
    return 0;
  }
  if (strcmp(value, "no") == 0) {
    *result = false;
    return 0;
  }
  fprintf(stderr, "Error: %s must be yes or no in dotconf.sh.\n", name);
  return -1;
}

static int load_configuration(const char *repo, struct config *config) {
  char path[PATH_MAX];
  if (path_for(path, sizeof(path), repo, "dotconf.sh") == -1)
    return -1;
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error: create %s before running dotman.\n", path);
    return -1;
  }
  config->gui = true;
  config->vm = false;
  char line[PATH_MAX];
  while (fgets(line, sizeof(line), fp) != NULL) {
    line[strcspn(line, "\r\n")] = '\0';
    char *key = trim(line);
    if (strncmp(key, "export ", 7) == 0)
      key = trim(key + 7);
    char *equals = strchr(key, '=');
    if (equals == NULL)
      continue;
    *equals = '\0';
    char *value = trim(equals + 1);
    size_t len = strlen(value);
    if (len >= 2 && ((value[0] == '\"' && value[len - 1] == '\"') ||
                     (value[0] == '\'' && value[len - 1] == '\''))) {
      value[len - 1] = '\0';
      value++;
    }
    if ((strcmp(key, "gui") == 0 &&
         parse_bool("gui", value, &config->gui) == -1) ||
        (strcmp(key, "vm") == 0 &&
         parse_bool("vm", value, &config->vm) == -1)) {
      fclose(fp);
      return -1;
    }
  }
  bool ok = !ferror(fp);
  fclose(fp);
  return ok ? 0 : -1;
}

static int load_dotlist(const char *repo, const struct config *config,
                        struct path_list *paths) {
  char list_path[PATH_MAX];
  if (path_for(list_path, sizeof(list_path), repo, "dotlist.txt") == -1)
    return -1;
  FILE *fp = fopen(list_path, "r");
  if (fp == NULL) {
    fprintf(stderr, "fopen: %s: %s\n", list_path, strerror(errno));
    return -1;
  }
  char line[PATH_MAX];
  while (fgets(line, sizeof(line), fp) != NULL) {
    line[strcspn(line, "\r\n")] = '\0';
    char *scope = trim(line);
    if (*scope == '\0' || *scope == '#')
      continue;
    char *separator = strpbrk(scope, " \t");
    if (separator == NULL)
      goto invalid;
    *separator++ = '\0';
    char *path = trim(separator);
    if ((strcmp(scope, "base") != 0 && strcmp(scope, "gui") != 0) ||
        !valid_path(path))
      goto invalid;
    if (strcmp(scope, "gui") == 0 && !config->gui)
      continue;
    if (add_path(paths, path) == -1) {
      fclose(fp);
      return -1;
    }
  }
  bool ok = !ferror(fp);
  fclose(fp);
  return ok ? 0 : -1;
invalid:
  fprintf(stderr, "Invalid dotlist entry: %s\n", line);
  fclose(fp);
  return -1;
}

static int preflight(const char *repo, const struct path_list *paths) {
  for (size_t i = 0; i < paths->len; i++) {
    char source[PATH_MAX];
    if (snprintf(source, sizeof(source), "%s/home/%s", repo, paths->items[i]) >=
        (int)sizeof(source))
      return -1;
    struct stat st;
    if (lstat(source, &st) == -1) {
      fprintf(stderr, "Error: managed source does not exist: %s\n", source);
      return -1;
    }
    for (size_t j = 0; j < i; j++)
      if (paths_overlap(paths->items[i], paths->items[j])) {
        fprintf(stderr, "Error: managed paths must not overlap: %s and %s\n",
                paths->items[j], paths->items[i]);
        return -1;
      }
  }
  return 0;
}

static int install_paths(const char *repo, const char *home,
                         const struct path_list *paths, bool dry_run) {
  char backup_root[PATH_MAX], backup_template[PATH_MAX];
  if (path_for(backup_root, sizeof(backup_root), home, ".dotfiles-backup") ==
          -1 ||
      snprintf(backup_template, sizeof(backup_template), "%s/install.XXXXXXXX",
               backup_root) >= (int)sizeof(backup_template))
    return -1;
  char *backup_dir = NULL;
  struct path_list moved = {0}, created = {0};
  int result = -1;
  for (size_t i = 0; i < paths->len; i++) {
    char source[PATH_MAX], destination[PATH_MAX], backup[PATH_MAX];
    if (snprintf(source, sizeof(source), "%s/home/%s", repo, paths->items[i]) >=
            (int)sizeof(source) ||
        path_for(destination, sizeof(destination), home, paths->items[i]) == -1)
      goto rollback;
    if (is_managed_link(source, destination))
      continue;
    struct stat st;
    bool exists = lstat(destination, &st) == 0;
    if (!exists && errno != ENOENT) {
      fprintf(stderr, "lstat: %s: %s\n", destination, strerror(errno));
      goto rollback;
    }
    if (dry_run) {
      if (exists)
        printf("Would move %s to a new backup directory\n", destination);
      printf("Would create %s\n", destination);
      continue;
    }
    if (exists) {
      if (backup_dir == NULL) {
        if (mkdir(backup_root, 0700) == -1 && errno != EEXIST) {
          fprintf(stderr, "mkdir: %s: %s\n", backup_root, strerror(errno));
          goto rollback;
        }
        backup_dir = mkdtemp(backup_template);
        if (backup_dir == NULL) {
          fprintf(stderr, "mkdtemp: %s\n", strerror(errno));
          goto rollback;
        }
      }
      if (path_for(backup, sizeof(backup), backup_dir, paths->items[i]) == -1 ||
          mkdir_parents(backup) == -1 || rename(destination, backup) == -1) {
        fprintf(stderr, "move: %s: %s\n", destination, strerror(errno));
        goto rollback;
      }
      if (add_path(&moved, paths->items[i]) == -1)
        goto rollback;
      printf("Move %s to %s\n", destination, backup);
    }
    if (mkdir_parents(destination) == -1 ||
        symlink(source, destination) == -1) {
      fprintf(stderr, "symlink: %s -> %s: %s\n", destination, source,
              strerror(errno));
      goto rollback;
    }
    if (add_path(&created, paths->items[i]) == -1)
      goto rollback;
    printf("Create %s\n", destination);
  }
  result = 0;
  if (backup_dir != NULL)
    printf("Backups are available in %s\n", backup_dir);
rollback:
  if (result == -1 && !dry_run) {
    fprintf(stderr, "Installation failed; restoring changed paths...\n");
    for (size_t i = created.len; i-- > 0;) {
      char destination[PATH_MAX];
      if (path_for(destination, sizeof(destination), home, created.items[i]) ==
          0)
        unlink(destination);
    }
    for (size_t i = moved.len; i-- > 0;) {
      char destination[PATH_MAX], backup[PATH_MAX];
      if (path_for(destination, sizeof(destination), home, moved.items[i]) ==
              0 &&
          path_for(backup, sizeof(backup), backup_dir, moved.items[i]) == 0) {
        mkdir_parents(destination);
        rename(backup, destination);
      }
    }
  }
  free_paths(&moved);
  free_paths(&created);
  return result;
}

static void usage(const char *program) {
  fprintf(stderr, "Usage: %s [--dry-run] [-r repository]\n", program);
}
static int parse_options(int argc, char **argv, struct options *options) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0)
      options->dry_run = true;
    else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      exit(EXIT_SUCCESS);
    } else if (strcmp(argv[i], "-r") == 0 && ++i < argc)
      options->repo = argv[i];
    else {
      usage(argv[0]);
      return -1;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  struct options options = {0};
  if (parse_options(argc, argv, &options) == -1)
    return EXIT_FAILURE;
  struct platform platform;
  if (detect_platform(&platform) != 0)
    return EXIT_FAILURE;
  if (is_nixos(&platform)) {
    fprintf(stderr, "Error: dotman cannot be run on NixOS.\n");
    return EXIT_FAILURE;
  }
  char cwd[PATH_MAX], repo[PATH_MAX];
  if (options.repo == NULL && getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("getcwd");
    return EXIT_FAILURE;
  }
  const char *requested_repo = options.repo == NULL ? cwd : options.repo;
  if (realpath(requested_repo, repo) == NULL) {
    fprintf(stderr, "realpath: %s: %s\n", requested_repo, strerror(errno));
    return EXIT_FAILURE;
  }
  const char *home = getenv("HOME");
  if (home == NULL || *home == '\0') {
    fprintf(stderr, "HOME is not set\n");
    return EXIT_FAILURE;
  }
  struct config config;
  struct path_list paths = {0};
  int result = load_configuration(repo, &config) == 0 &&
                       load_dotlist(repo, &config, &paths) == 0 &&
                       preflight(repo, &paths) == 0
                   ? install_paths(repo, home, &paths, options.dry_run)
                   : -1;
  free_paths(&paths);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
