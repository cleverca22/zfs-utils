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
#include <stdlib.h>
#include <math.h>

#define MIN(a,b) (a < b) ? a : b

uint64_t total = 0;
bool do_defrag = false;
int firstdev = 0;

uint64_t min_lost = 1 << 20;

void degang(int fd, uint64_t blksize, uint64_t size) {
  void *buffer = malloc(blksize);
  for (uint64_t i = 0; i < size; i += blksize) {
    printf("%ld/%ld, %f%%\r", i/blksize, size/blksize, (double)i/size*100);
    uint64_t remaining = size - i;
    uint64_t len = MIN(remaining, blksize);
    int res = pread(fd, buffer, len, i);
    assert(res == len);
    res = pwrite(fd, buffer, len, i);
    assert(res == len);
  }
  free(buffer);
}

void recurse(DIR *hnd, const char *name) {
  struct dirent *ent;
  int fd = dirfd(hnd);
  char buffer[1024];

  while (ent = readdir(hnd)) {
    if (strcmp(ent->d_name, ".") == 0) continue;
    if (strcmp(ent->d_name, "..") == 0) continue;

    snprintf(buffer, 1023, "%s/%s", name, ent->d_name);
    buffer[1023] = 0;

    struct stat statbuf;
    int ret = fstatat(fd, ent->d_name, &statbuf, AT_SYMLINK_NOFOLLOW);
    if (ret == -1) {
      if (errno == ENOENT) continue;
    }
    if (firstdev == 0) firstdev = statbuf.st_dev;
    if (statbuf.st_dev != firstdev) {
      printf("skipping %s\n", buffer);
      continue;
    }

    if (ent->d_type == DT_DIR) {
      int dirfd = openat(fd, ent->d_name, O_CLOEXEC | O_DIRECTORY | O_NOCTTY | O_RDONLY);
      if ((dirfd == -1) && (errno == EACCES)) continue;
      assert(dirfd >= 0);
      DIR *hnd2 = fdopendir(dirfd);
      recurse(hnd2, buffer);
      closedir(hnd2);
    } else {
      if ((statbuf.st_blocks * 512) > statbuf.st_size) {
        //printf("inode %ld, sizes: %ld %ld\n", statbuf.st_ino, statbuf.st_size, statbuf.st_blocks * 512);
        uint64_t overhead = (statbuf.st_blocks * 512) - statbuf.st_size;
        if (overhead > min_lost) {
          printf("%ldMb %ldKb 2^%d %s\n", overhead/1024/1024, statbuf.st_blksize>>10, (int)log2(statbuf.st_blksize), buffer);
          total += overhead;
          if (do_defrag) {
            int filefd = openat(fd, ent->d_name, O_RDWR);
            if (filefd == -1) {
              perror("cant open to defrag");
            } else {
              degang(filefd, statbuf.st_blksize, statbuf.st_size);
              close(filefd);
            }
          }
        }
      }
    }
  }
}

int main(int argc, char** argv) {
  int opt;
  const char *path = ".";

  while ((opt = getopt(argc, argv, "p:dm:")) != -1) {
    switch (opt) {
    case 'p':
      path = optarg;
      break;
    case 'd':
      do_defrag = true;
      break;
    case 'm':
      min_lost = atoi(optarg) << 20;
      break;
    }
  }
  assert(path);
  int rootdir = open(path, O_CLOEXEC | O_DIRECTORY | O_NOCTTY | O_RDONLY);
  assert(rootdir >= 0);
  DIR* hnd = fdopendir(rootdir);
  recurse(hnd, path);
  printf("%ldMb total\n", total/1024/1024);
  return 0;
}
