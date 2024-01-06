#include <sstream>
#include <stdio.h>
#include <string>
#include <strings.h>
#include <time.h>
#include <vector>

using namespace std;

vector<string> split (const string &s, char delim) {
    vector<string> result;
    stringstream ss (s);
    string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}

char *buffer;

int main(int argc, char **argv) {
  const char *namebuf = "/proc/spl/kstat/zfs/dbgmsg";
  uint64_t last_ts = 0, next_last_ts;
  buffer = (char*)malloc(16*1024*1024);
  while (true) {
    FILE *hnd = fopen(namebuf, "r");
    size_t size = fread(buffer, 1, (16*1024*1024)-1, hnd);
    buffer[size] = 0;
    fclose(hnd);
    vector<string> lines = split(buffer, '\n');
    lines.erase(lines.begin());
    for (string line : lines) {
      uint64_t ts;
      char msg[1024];
      bzero(msg, 1024);
      sscanf(line.c_str(), "%ld %1000c", &ts, &msg[0]);
      if (ts > last_ts) {
        time_t ts2 = ts;
        char time[20];
        strftime(time, 20, "%Y-%m-%d %H:%M:%S", localtime(&ts2));
        printf("%s %s\n", time, msg);
        next_last_ts = ts;
      }
    }
    last_ts = next_last_ts;
    struct timespec delay;
    delay.tv_sec = 1;
    delay.tv_nsec = 0;
    nanosleep(&delay, NULL);
  }
  return 0;
}
