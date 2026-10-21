#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static int failures;

static void
fatal(char *message)
{
  fprintf(2, "dutest: %s\n", message);
  exit(1);
}

static void
make_file(char *path, char *contents)
{
  int fd = open(path, O_CREATE | O_WRONLY | O_TRUNC);
  int len = strlen(contents);

  if (fd < 0)
    fatal("cannot create test file");

  if (write(fd, contents, len) != len)
    fatal("cannot write test file");

  close(fd);
}

static uint64
file_size(char *path)
{
  struct stat st;
  int fd = open(path, O_RDONLY);

  if (fd < 0)
    fatal("cannot open test path");

  if (fstat(fd, &st) < 0)
    fatal("fstat failed");

  close(fd);
  return st.size;
}

/* Append one expected output line without needing snprintf(). */
static void
append_line(char *output, uint64 size, char *path)
{
  char digits[32];
  int count = 0;
  int pos = strlen(output);

  do {
    digits[count++] = '0' + size % 10;
    size /= 10;
  } while (size != 0);

  while (count > 0)
    output[pos++] = digits[--count];

  output[pos++] = '\t';

  while (*path)
    output[pos++] = *path++;

  output[pos++] = '\n';
  output[pos] = '\0';
}

static void
check(char *label, char *path, char *expected)
{
  int p[2];
  int status;
  int used = 0;
  int overflow = 0;
  int n;
  char actual[2048];
  char chunk[128];
  char *args[] = {"du", path, 0};

  if (pipe(p) < 0)
    fatal("pipe failed");

  int pid = fork();

  if (pid < 0)
    fatal("fork failed");

  if (pid == 0) {
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

  /* Keep draining even if the output exceeds our buffer. */
  while ((n = read(p[0], chunk, sizeof(chunk))) > 0) {
    for (int i = 0; i < n; i++) {
      if (used < sizeof(actual) - 1)
        actual[used++] = chunk[i];
      else
        overflow = 1;
    }
  }

  close(p[0]);
  actual[used] = '\0';
  wait(&status);

  if (status == 0 && n == 0 && !overflow &&
      strcmp(actual, expected) == 0) {
    printf("PASS: %s\n", label);
    return;
  }

  failures++;
  printf("FAIL: %s\n", label);
  printf("Expected:\n%s", expected);
  printf("Actual:\n%s", actual);

  if (used == 0 || actual[used - 1] != '\n')
    printf("\n");

  if (overflow)
    printf("(output exceeded capture buffer)\n");

  if (status != 0)
    printf("(du exited with status %d)\n", status);
}

static void
cleanup(void)
{
  /* Remove children before their parent directories. */
  unlink("dutmp/sub/deep/c.txt");
  unlink("dutmp/sub/deep");
  unlink("dutmp/sub/b.txt");
  unlink("dutmp/sub");
  unlink("dutmp/empty");
  unlink("dutmp/abcdefghijklmn");
  unlink("dutmp/a.txt");
  unlink("dutmp");
}

int
main(void)
{
  char expected[1024];
  uint64 deep_size;
  uint64 sub_size;
  uint64 empty_size;
  uint64 root_size;

  /*
   * Reserve this directory for the tester. Refuse to overwrite an
   * existing directory or file.
   */
  if (mkdir("dutmp") < 0)
    fatal("dutmp already exists or cannot be created; remove it first");

  if (mkdir("dutmp/sub") < 0 ||
      mkdir("dutmp/sub/deep") < 0 ||
      mkdir("dutmp/empty") < 0)
    fatal("cannot create test directories");

  make_file("dutmp/a.txt", "1234\n");
  make_file("dutmp/sub/b.txt", "abcdef\n");
  make_file("dutmp/sub/deep/c.txt", "xyz\n");
  make_file("dutmp/abcdefghijklmn", "fourteen-name\n");

  /* Measure directory sizes only after creating all entries. */
  deep_size = file_size("dutmp/sub/deep") +
              file_size("dutmp/sub/deep/c.txt");

  sub_size = file_size("dutmp/sub") +
             file_size("dutmp/sub/b.txt") + deep_size;

  empty_size = file_size("dutmp/empty");

  root_size = file_size("dutmp") +
              file_size("dutmp/a.txt") +
              file_size("dutmp/abcdefghijklmn") +
              sub_size + empty_size;

  expected[0] = '\0';
  append_line(expected, 5, "dutmp/a.txt");
  check("regular file", "dutmp/a.txt", expected);

  expected[0] = '\0';
  append_line(expected, file_size("dutmp/abcdefghijklmn"),
              "dutmp/abcdefghijklmn");
  check("14-character filename", "dutmp/abcdefghijklmn", expected);

  expected[0] = '\0';
  append_line(expected, empty_size, "dutmp/empty");
  check("empty directory", "dutmp/empty", expected);

  expected[0] = '\0';
  append_line(expected, deep_size, "dutmp/sub/deep");
  append_line(expected, sub_size, "dutmp/sub");
  check("nested directory", "dutmp/sub", expected);

  /*
   * Test sibling directories independently of their traversal order.
   * Either sibling ordering is permitted, but parents must come last.
   */
  if (unlink("dutmp/empty") < 0)
    fatal("cannot remove empty test directory");

  root_size = file_size("dutmp") +
              file_size("dutmp/a.txt") +
              file_size("dutmp/abcdefghijklmn") + sub_size;

  expected[0] = '\0';
  append_line(expected, deep_size, "dutmp/sub/deep");
  append_line(expected, sub_size, "dutmp/sub");
  append_line(expected, root_size, "dutmp");
  check("full recursive total and output order", "dutmp", expected);

  cleanup();

  if (failures == 0)
    printf("All 5 tests passed.\n");
  else
    printf("%d of 5 tests failed.\n", failures);

  exit(failures != 0);
}
