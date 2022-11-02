#include <stdint.h>
#include <list>
#include <string>
#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

using namespace std;

// TODO, store a seperate copy of these for each pool
uint64_t dedup[64];
uint64_t elog[64];
uint64_t dedicated_log[64];
uint64_t normal[64];
uint64_t special[64];

static std::list<std::string> find_pools() {
  std::list<std::string> pools;
  DIR *dh = opendir("/proc/spl/kstat/zfs/");
  if (!dh) {
    exit(1);
  }
  while (struct dirent *dir = readdir(dh)) {
    if ((dir->d_type == 4) && (dir->d_name[0] != '.')) {
      pools.push_back(dir->d_name);
    }
  }
  closedir(dh);
  return pools;
}

static void scan_class(const string dir, const string pool, const string metaclass, uint64_t *histogram, FILE *out) {
  string path = dir + "fragmentation_" + metaclass;
  fstream fh(path, fh.in);
  if (!fh.is_open()) {
    exit(2);
  }
  while (!fh.eof()) {
    uint64_t bits, count;
    if (fh >> bits >> count) {
      if ((histogram[bits] != count) || count) {
        fprintf(out,"zfs_fragmentation_%s{pool=\"%s\",power=\"%ld\"} %ld\n", metaclass.c_str(), pool.c_str(), bits, count);
        histogram[bits] = count;
      }
    }
  }
}

static void do_scan(FILE *out) {
  auto pools = find_pools();
  for (auto &&pool : pools) {
    string dir = "/proc/spl/kstat/zfs/" + pool + "/";
    scan_class(dir, pool, "dedup", (uint64_t*)&dedup, out);
    scan_class(dir, pool, "embedded_log", (uint64_t*)&elog, out);
    scan_class(dir, pool, "log", (uint64_t*)&dedicated_log, out);
    scan_class(dir, pool, "normal", (uint64_t*)&normal, out);
    scan_class(dir, pool, "special", (uint64_t*)&special, out);
  }
}

void start_server() {
  int server = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(9103);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(server, (sockaddr*)&addr, sizeof(addr))) {
    exit(3);
  }
  listen(server, 5);
  while (true) {
    int client = accept(server, NULL, 0);
    FILE *client_fh = fdopen(client, "r+");
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, client_fh)) != -1) {
      //printf("Retrieved line of length %zu :\n", read);
      //printf("%s", line);
      string line2 = line;
      if (line2 == "\r\n") {
        fputs("HTTP/1.0 200\r\nContent-Type: text/plain\r\n\r\n",client_fh);
        do_scan(client_fh);
        fflush(client_fh);
        fclose(client_fh);
        break;
      }
    }
  }
}

int main(int argc, char **argv) {
  start_server();
  return 0;
}
