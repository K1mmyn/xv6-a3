#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void format_path(char *buf, const char *path, const char *name) {
    char *p;
    
    // Copy the base path
    strcpy(buf, path);
    p = buf + strlen(buf);
    
    // Add trailing slash if missing (unless path is just root "/")
    if (p > buf && *(p - 1) != '/') {
        *p++ = '/';
    }
    
    // Append the file or directory name
    strcpy(p, name);
}

int collect_total_size(char* filename){
    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
      printf("Error: Unable to open file");
      return -1;
    }

    struct stat file_meta;

    if (fstat(fd, &file_meta) == -1) {
       printf("Error: Unable to excute fstat") ;
       close(fd);
       return -1;
    }

    int total_size = 0;

    if (file_meta.type == T_FILE) {
        close(fd);
        return file_meta.size;
    } else {
        struct dirent de;
        while(read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0){
                continue;
            }

            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            //TODO IDK HOW TO FIND NEW FILE PAth
            //
            char buf[512];

            format_path(buf, filename, de.name);

            total_size += collect_total_size(buf);
        } 
        printf("%d\t%s\n", total_size, filename);
    }

    close(fd);
    return total_size;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
      printf("Error: Please use the format du [PATH]");
      return -1;
    }
    int fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
      printf("Error: Unable to open file");
      return -1;
    }

    struct stat file_meta;

    if (fstat(fd, &file_meta) == -1) {
       printf("Error: Unable to excute fstat") ;
       close(fd);
       return -1;
    }

    if (file_meta.type == T_FILE) {
        printf("%ld\t%s\n", file_meta.size, argv[1]);
        return 0;
    }
    close(fd);
    collect_total_size(argv[1]);
    return 0;
}
