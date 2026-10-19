#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char path[512];

int collect_total_size(int first){
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
      printf("Error: Unable to open file");
      return -1;
    }

    struct stat file_meta;

    if (fstat(fd, &file_meta) < 0) {
       printf("Error: Unable to excute fstat") ;
       close(fd);
       return -1;
    }

    long int total_size = file_meta.size;
    int start = strlen(path);
    char name[DIRSIZ + 1];

    if (file_meta.type != T_DIR) {
        if (first) {
          printf("%ld\t%s\n", total_size, path);
        }
        close(fd);
        return total_size;
    } else {
        struct dirent de;
        while(read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0){
                continue;
            }

            memmove(name, de.name, DIRSIZ);
            name[DIRSIZ] = '\0';

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            int pos = start;

            if (pos > 0 && path[pos - 1] != '/') {
                pos++;
            }
            
            if (pos + strlen(name) + 1 > sizeof(path)) {
              printf("Error: file path too long");
              close(fd);
              return -1; 
            }

            if (pos != start) {
                path[start] = '/';
            }

            strcpy(path + pos, name); 
            long int child_size = collect_total_size(0);

            path[start] = '\0';
            if (child_size < 0) {
                close(fd);
                return -1;
            }

            total_size += child_size;

        } 
        
    }

    printf("%ld\t%s\n", total_size, path);

    close(fd);
    return total_size;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
      printf("Error: Please use the format du [PATH]");
      return -1;
    }
    if (strlen(argv[1]) >= sizeof(path)) {
      printf("Error: file name is too long\n");
      return -1;
    }
    strcpy(path, argv[1]);
    if (collect_total_size(1) < 0) {
      return -1;
    }

    return 0;
}
