#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

uint64_t total = 0;

void recurse(DIR *hnd) {
  struct dirent *ent;
  int fd = dirfd(hnd);

  while (ent = readdir(hnd)) {
    if (strcmp(ent->d_name, ".") == 0) continue;
    if (strcmp(ent->d_name, "..") == 0) continue;
    if (ent->d_type & DT_DIR) {
      int dirfd = openat(fd, ent->d_name, O_CLOEXEC | O_DIRECTORY | O_NOCTTY | O_RDONLY);
      if ((dirfd == -1) && (errno == EACCES)) continue;
      assert(dirfd >= 0);
      DIR *hnd2 = fdopendir(dirfd);
      recurse(hnd2);
      closedir(hnd2);
    } else {
      struct stat statbuf;
      int ret = fstatat(fd, ent->d_name, &statbuf, AT_SYMLINK_NOFOLLOW);
      assert(ret == 0);
      if ((statbuf.st_blocks * 512) > statbuf.st_size) {
        //printf("inode %ld, sizes: %ld %ld\n", statbuf.st_ino, statbuf.st_size, statbuf.st_blocks * 512);
        uint64_t overhead = (statbuf.st_blocks * 512) - statbuf.st_size;
        printf("%ldMb %s\n", overhead/1024/1024, ent->d_name);
        total += overhead;
      }
    }
  }
}

int main(int argc, char** argv) {
  assert(argc == 2);
  const char *path = argv[1];
  int rootdir = open(path, O_CLOEXEC | O_DIRECTORY | O_NOCTTY | O_RDONLY);
  assert(rootdir >= 0);
  printf("fd %d\n", rootdir);
  DIR* hnd = fdopendir(rootdir);
  recurse(hnd);
  printf("%ldMb total\n", total/1024/1024);
  return 0;
}
