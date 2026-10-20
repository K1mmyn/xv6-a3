#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static char *dir = "ts_test";
static char *path = "ts_test/file";
static int failures;

static void
wait_ticks(uint ticks)
{
  uint start = uptime();

  while ((uint)(uptime() - start) < ticks) {
    /* Busy-wait to avoid depending on sleep() or pause(). */
  }
}

static void
check(int condition, char *message)
{
  if (condition) {
    printf("PASS: %s\n", message);
  } else {
    printf("FAIL: %s\n", message);
    failures++;
  }
}

static void
cleanup(void)
{
  unlink(path);
  unlink(dir);
}

static void
fatal(char *message)
{
  printf("ERROR: %s\n", message);
  cleanup();
  exit(1);
}

int
main(void)
{
  int fd;
  uint before, after;
  struct stat created, written, reopened;
  struct stat dir_before, dir_after;

  cleanup();

  if (mkdir(dir) < 0)
    fatal("cannot create test directory");

  if (stat(dir, &dir_before) < 0)
    fatal("cannot stat test directory");

  /* Ensure creation happens at a nonzero, later clock tick. */
  wait_ticks(5);

  before = uptime();
  fd = open(path, O_CREATE | O_RDWR);
  after = uptime();

  if (fd < 0)
    fatal("cannot create test file");

  if (fstat(fd, &created) < 0) {
    close(fd);
    fatal("fstat failed after creation");
  }

  printf("Creation: timestamp=%d, uptime range=%d..%d\n",
         created.timestamp, before, after);
  printf("DEBUG: timestamp=%d before=%d after=%d lower=%d upper=%d\n",
       created.timestamp, before, after,
       created.timestamp >= before,
       created.timestamp <= after);
  check(created.timestamp >= before && created.timestamp <= after,
        "new file timestamp matches creation time");

  if (stat(dir, &dir_after) < 0) {
    close(fd);
    fatal("cannot stat directory after file creation");
  }

  check(dir_after.timestamp > dir_before.timestamp,
        "adding a file updates its directory timestamp");

  /* Avoid creating and writing during the same clock tick. */
  wait_ticks(5);

  before = uptime();
  if (write(fd, "hello\n", 6) != 6) {
    close(fd);
    fatal("write failed");
  }
  after = uptime();

  if (fstat(fd, &written) < 0) {
    close(fd);
    fatal("fstat failed after write");
  }

  printf("Write: timestamp=%d, uptime range=%d..%d\n",
         written.timestamp, before, after);

  check(written.timestamp > created.timestamp,
        "writing advances the file timestamp");

  check(written.timestamp >= before && written.timestamp <= after,
        "updated timestamp matches write time");

  check(written.size == 6, "file size remains correct");

  close(fd);
  wait_ticks(5);

  fd = open(path, O_RDONLY);
  if (fd < 0)
    fatal("cannot reopen test file");

  if (fstat(fd, &reopened) < 0) {
    close(fd);
    fatal("fstat failed after reopening");
  }

  check(reopened.timestamp == written.timestamp,
        "closing and reopening preserves the timestamp");

  close(fd);

  printf("\nCheck ls formatting with: ls ts_test\n");
  printf("Expected file row: file %d %d %d %ld\n",
         written.type, written.ino, written.timestamp, written.size);

  if (failures == 0)
    printf("\nAll timestamp tests passed.\n");
  else
    printf("\n%d timestamp test(s) failed.\n", failures);

  /* Leave the file available for manual ls inspection. */
  exit(failures != 0);
}
