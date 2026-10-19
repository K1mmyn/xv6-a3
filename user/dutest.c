#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define ROOT "dutest_tmp"
#define D1 ROOT "/dir1"
#define D2 D1 "/dir2"
#define D3 D2 "/dir3"
#define EMPTY ROOT "/empty"

#define F0 ROOT "/file"
#define F1 D1 "/file1"
#define F2 D2 "/file2"
#define F3 D3 "/file3"

struct expected {
  char *path;
  int size;
};

static int failures;

static void
fatal(char *message)
{
  fprintf(2, "dutest: %s\n", message);
  exit(1);
}

static void
cleanup(void)
{
  unlink(F3);
  unlink(F2);
  unlink(F1);
  unlink(F0);
  unlink(D3);
  unlink(D2);
  unlink(D1);
  unlink(EMPTY);
  unlink(ROOT);
}

static void
make_file(char *path, int size)
{
  char buf[128];
  int fd;
  int n;

  memset(buf, 'x', sizeof(buf));

  fd = open(path, O_CREATE | O_WRONLY);
  if (fd < 0)
    fatal("cannot create test file");

  while (size > 0) {
    n = size < sizeof(buf) ? size : sizeof(buf);
    if (write(fd, buf, n) != n)
      fatal("cannot write test file");
    size -= n;
  }

  close(fd);
}

static int
file_size(char *path)
{
  struct stat st;
  int fd;

  fd = open(path, O_RDONLY);
  if (fd < 0)
    fatal("cannot open path for fstat");

  if (fstat(fd, &st) < 0)
    fatal("fstat failed");

  close(fd);
  return st.size;
}

/*
 * Read one output line:
 *
 *   decimal-size<TAB>path<NEWLINE>
 *
 * Advance *cursor past the line on success.
 */
static int
check_line(char **cursor, struct expected *want)
{
  char *p = *cursor;
  char *name = want->path;
  int size = 0;

  if (*p < '0' || *p > '9')
    return 0;

  while (*p >= '0' && *p <= '9') {
    size = size * 10 + (*p - '0');
    p++;
  }

  if (size != want->size || *p != '\t')
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

  *cursor = p + 1;
  return 1;
}

static void
run_test(char *label, char *path, struct expected *want, int count)
{
  char output[4096];
  char discard[128];
  char *cursor;
  int fds[2];
  int pid;
  int status;
  int used = 0;
  int overflow = 0;
  int n;
  int ok;

  if (pipe(fds) < 0)
    fatal("pipe failed");

  pid = fork();
  if (pid < 0)
    fatal("fork failed");

  if (pid == 0) {
    char *args[] = {"du", path, 0};

    close(fds[0]);
    close(1);

    if (dup(fds[1]) != 1)
      exit(1);

    close(fds[1]);
    exec("du", args);

    fprintf(2, "dutest: cannot exec du\n");
    exit(1);
  }

  close(fds[1]);

  while (used < sizeof(output) - 1) {
    n = read(fds[0], output + used, sizeof(output) - 1 - used);
    if (n < 0)
      fatal("read failed");
    if (n == 0)
      break;
    used += n;
  }

  /*
   * Drain excess output before waiting so a verbose child cannot
   * block forever on a full pipe.
   */
  while ((n = read(fds[0], discard, sizeof(discard))) > 0)
    overflow = 1;

  if (n < 0)
    fatal("read failed");

  output[used] = '\0';
  close(fds[0]);

  if (wait(&status) < 0)
    fatal("wait failed");

  cursor = output;
  ok = status == 0 && !overflow;

  for (int i = 0; i < count && ok; i++)
    ok = check_line(&cursor, &want[i]);

  if (ok && cursor != output + used)
    ok = 0;

  if (ok) {
    printf("PASS: %s\n", label);
    return;
  }

  failures++;
  printf("FAIL: %s\n", label);
  printf("  Exit status: %d\n", status);
  printf("  Expected output (size TAB path):\n");

  for (int i = 0; i < count; i++)
    printf("%d\t%s\n", want[i].size, want[i].path);

  printf("  Actual output:\n%s", output);

  if (used == 0 || output[used - 1] != '\n')
    printf("\n");

  if (overflow)
    printf("  [Output exceeded capture buffer]\n");
}

int
main(void)
{
  int s1;
  int s2;
  int s3;
  struct expected regular[1];
  struct expected empty[1];
  struct expected deepest[1];
  struct expected nested[3];

  /*
   * ROOT is reserved for this test. Remove leftovers from an
   * earlier run before creating the fixture.
   */
  cleanup();

  if (mkdir(ROOT) < 0 || mkdir(D1) < 0 || mkdir(D2) < 0 ||
      mkdir(D3) < 0 || mkdir(EMPTY) < 0)
    fatal("cannot create test directories");

  make_file(F0, 37);
  make_file(F1, 101);
  make_file(F2, 1025);
  make_file(F3, 4097);

  /*
   * Query directory sizes after creating every entry. Directory
   * sizes are filesystem-dependent and must not be hardcoded.
   */
  s3 = file_size(D3) + 4097;
  s2 = file_size(D2) + 1025 + s3;
  s1 = file_size(D1) + 101 + s2;

  regular[0].path = F0;
  regular[0].size = 37;

  empty[0].path = EMPTY;
  empty[0].size = file_size(EMPTY);

  deepest[0].path = D3;
  deepest[0].size = s3;

  nested[0].path = D3;
  nested[0].size = s3;
  nested[1].path = D2;
  nested[1].size = s2;
  nested[2].path = D1;
  nested[2].size = s1;

  run_test("regular file", F0, regular, 1);
  run_test("empty directory", EMPTY, empty, 1);
  run_test("directory containing a file", D3, deepest, 1);
  run_test("three nested directories", D1, nested, 3);

  cleanup();

  if (failures) {
    printf("dutest: %d test(s) failed\n", failures);
    exit(1);
  }

  printf("dutest: all 4 tests passed\n");
  exit(0);
}
