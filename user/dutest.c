#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define ROOT "dutest_tmp"
#define SUB ROOT "/sub"
#define DEEP SUB "/deep"
#define FILE ROOT "/a.txt"
#define LONGFILE DEEP "/abcdefghijklmn"

struct expected {
  char *path;
  int size;
};

static int failures;

static void
cleanup(void)
{
  unlink(LONGFILE);
  unlink(FILE);
  unlink(DEEP);
  unlink(SUB);
  unlink(ROOT);
}

static void
fatal(char *message)
{
  fprintf(2, "dutest: %s\n", message);
  cleanup();
  exit(1);
}

static void
make_file(char *path, char *contents)
{
  int fd = open(path, O_CREATE | O_WRONLY);
  int length = strlen(contents);

  if (fd < 0)
    fatal("cannot create test file");

  if (write(fd, contents, length) != length) {
    close(fd);
    fatal("cannot write test file");
  }

  close(fd);
}

static int
file_size(char *path)
{
  struct stat st;
  int fd = open(path, O_RDONLY);

  if (fd < 0)
    fatal("cannot open fixture");

  if (fstat(fd, &st) < 0) {
    close(fd);
    fatal("fstat failed");
  }

  close(fd);
  return st.size;
}

/*
 * Capture stdout while du runs. Reading before wait prevents a full
 * pipe from blocking the child indefinitely.
 */
static int
capture_du(char *path, char *output, int capacity)
{
  int p[2];
  int pid;
  int status;
  int used = 0;
  int overflow = 0;
  int n;
  char chunk[128];

  if (pipe(p) < 0)
    fatal("pipe failed");

  pid = fork();
  if (pid < 0) {
    close(p[0]);
    close(p[1]);
    fatal("fork failed");
  }

  if (pid == 0) {
    char *args[] = {"du", path, 0};

    close(p[0]);
    close(1);

    if (dup(p[1]) != 1)
      exit(1);

    close(p[1]);
    exec("du", args);
    fprintf(2, "dutest: cannot exec du\n");
    exit(1);
  }

  close(p[1]);

  while ((n = read(p[0], chunk, sizeof(chunk))) > 0) {
    for (int i = 0; i < n; i++) {
      if (used < capacity - 1)
        output[used++] = chunk[i];
      else
        overflow = 1;
    }
  }

  close(p[0]);
  output[used] = '\0';

  if (wait(&status) < 0)
    return -1;

  if (n < 0 || overflow || status != 0)
    return -1;

  return 0;
}

/* Match exactly: decimal size, tab, path, newline. */
static int
matches(char *output, struct expected *expected, int count)
{
  char *p = output;

  for (int i = 0; i < count; i++) {
    int size = 0;
    char *name = expected[i].path;

    if (*p < '0' || *p > '9')
      return 0;

    while (*p >= '0' && *p <= '9') {
      size = size * 10 + (*p - '0');
      p++;
    }

    if (size != expected[i].size || *p != '\t')
      return 0;

    p++;

    while (*name) {
      if (*p != *name)
        return 0;
      p++;
      name++;
    }

    if (*p != '\n')
      return 0;

    p++;
  }

  return *p == '\0';
}

static void
check(char *label, char *path, struct expected *expected, int count)
{
  char output[2048];

  if (capture_du(path, output, sizeof(output)) < 0) {
    printf("FAIL: %s (du failed or output too large)\n", label);
    failures++;
    return;
  }

  if (!matches(output, expected, count)) {
    printf("FAIL: %s\n", label);
    printf("Expected:\n");

    for (int i = 0; i < count; i++)
      printf("%d\t%s\n", expected[i].size, expected[i].path);

    printf("Actual:\n%s", output);
    printf("\n");
    failures++;
    return;
  }

  printf("PASS: %s\n", label);
}

int
main(void)
{
  struct stat st;
  struct expected expected[3];
  int deep_total;
  int sub_total;
  int root_total;

  /*
   * Do not delete an existing directory that might contain user data.
   */
  if (stat(ROOT, &st) == 0) {
    fprintf(2, "dutest: %s already exists; remove it first\n", ROOT);
    exit(1);
  }

  if (mkdir(ROOT) < 0)
    fatal("cannot create root directory");

  expected[0].path = ROOT;
  expected[0].size = file_size(ROOT);
  check("empty directory", ROOT, expected, 1);

  if (mkdir(SUB) < 0 || mkdir(DEEP) < 0)
    fatal("cannot create nested directories");

  make_file(FILE, "1234\n");
  make_file(LONGFILE, "abcdefgh\n");

  expected[0].path = FILE;
  expected[0].size = file_size(FILE);
  check("regular file", FILE, expected, 1);

  expected[0].path = LONGFILE;
  expected[0].size = file_size(LONGFILE);
  check("14-character filename", LONGFILE, expected, 1);

  /*
   * Measure directory sizes after creating all directory entries.
   * Each subtree includes its own directory file.
   */
  deep_total = file_size(DEEP) + file_size(LONGFILE);
  sub_total = file_size(SUB) + deep_total;
  root_total = file_size(ROOT) + file_size(FILE) + sub_total;

  expected[0].path = DEEP;
  expected[0].size = deep_total;
  expected[1].path = SUB;
  expected[1].size = sub_total;
  expected[2].path = ROOT;
  expected[2].size = root_total;

  check("recursive totals and postorder output", ROOT, expected, 3);

  cleanup();

  if (failures) {
    printf("dutest: %d test(s) failed\n", failures);
    exit(1);
  }

  printf("dutest: all tests passed\n");
  exit(0);
}
