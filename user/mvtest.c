#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static void
fail(char *message)
{
  fprintf(2, "mvtest: FAIL: %s\n", message);
  exit(1);
}

static void
test_move(char *source, char *destination, char *contents)
{
  struct stat before, after;
  char buffer[128];
  int length = strlen(contents);
  int fd, pid, status, n;

  // Remove leftovers from previous test runs.
  unlink(source);
  unlink(destination);

  fd = open(source, O_CREATE | O_WRONLY);
  if (fd < 0)
    fail("could not create source file");

  if (write(fd, contents, length) != length)
    fail("could not write source file");

  if (fstat(fd, &before) < 0)
    fail("could not stat source file");

  close(fd);

  pid = fork();
  if (pid < 0)
    fail("fork failed");

  if (pid == 0) {
    char *args[] = {"mv", source, destination, 0};
    exec("mv", args);
    fprintf(2, "mvtest: could not execute mv\n");
    exit(1);
  }

  if (wait(&status) != pid)
    fail("wait failed");

  if (status != 0)
    fail("mv exited with a non-zero status");

  if (stat(source, &after) == 0)
    fail("original filename still exists");

  fd = open(destination, O_RDONLY);
  if (fd < 0)
    fail("new filename does not exist");

  if (fstat(fd, &after) < 0)
    fail("could not stat destination file");

  if (before.dev != after.dev || before.ino != after.ino)
    fail("destination does not refer to the original inode");

  if (before.nlink != after.nlink)
    fail("link count changed");

  if (after.size != (uint)length)
    fail("file size changed");

  n = read(fd, buffer, sizeof(buffer));
  if (n != length)
    fail("unexpected number of bytes read");

  if (memcmp(buffer, contents, length) != 0)
    fail("file contents changed");

  close(fd);

  if (unlink(destination) < 0)
    fail("could not clean up destination file");

  printf("mvtest: PASS: %s -> %s\n", source, destination);
}

int
main(int argc, char *argv[])
{
  test_move("mvt_old", "mvt_new", "Hello from xv6!\nRename test.\n");
  test_move("mvt_empty", "mvt_empty_new", "");

  printf("mvtest: all tests passed\n");
  exit(0);
}
