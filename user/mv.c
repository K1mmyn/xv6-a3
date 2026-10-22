#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main (int argc, char* argv[]) 
{

  if (argc != 3) {
    printf("Error: Please use the format mv [CUR NAME] [NEW NAME]\n");
    return -1;
  }

  char* old_file_name = argv[1];
  char* new_file_name = argv[2];

  int new_hardlink_status = link(old_file_name, new_file_name);

  if (new_hardlink_status < 0) {
    printf("Error: Unable to create new hardlink\n");
    return -1;
  }

  if (unlink(old_file_name) < 0) {
    printf("Error: Unable to unlink old filename\n"); 
    return -1;
  }

  return 0;

}
