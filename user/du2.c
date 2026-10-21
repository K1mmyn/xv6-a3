#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user/user.h"

#define PATH_SIZE 512

static char path[PATH_SIZE];

static int
collect_total_size(int print_file)
{
  struct stat st;
  struct dirent de;
  char name[DIRSIZ + 1];
  int fd;
  int total;
  int base;
  int pos;
  int child_size;

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(2, "du: cannot open %s\n", path);
    return -1;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "du: cannot stat %s\n", path);
    close(fd);
    return -1;
  }

  /* Include the file or directory's own size. */
  total = st.size;

  if (st.type != T_DIR) {
    if (print_file)
      printf("%d\t%s\n", total, path);

    close(fd);
    return total;
  }

  base = strlen(path);

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = '\0';

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    pos = base;
    if (pos > 0 && path[pos - 1] != '/')
      pos++;

    if (pos + strlen(name) + 1 > sizeof(path)) {
      fprintf(2, "du: path too long\n");
      close(fd);
      return -1;
    }

    if (pos != base)
      path[base] = '/';

    strcpy(path + pos, name);

    /* Descendants print directory totals, but not regular files. */
    child_size = collect_total_size(0);

    /* Restore this directory's path after visiting its child. */
    path[base] = '\0';

    if (child_size < 0) {
      close(fd);
      return -1;
    }

    total += child_size;
  }

  close(fd);

  /* Print parents after their subdirectories. */
  printf("%d\t%s\n", total, path);
  return total;
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(2, "usage: du PATH\n");
    exit(1);
  }

  if (strlen(argv[1]) >= sizeof(path)) {
    fprintf(2, "du: path too long\n");
    exit(1);
  }

  strcpy(path, argv[1]);

  if (collect_total_size(1) < 0)
    exit(1);

  exit(0);
}
