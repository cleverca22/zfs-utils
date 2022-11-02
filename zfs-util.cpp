#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string>
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

int main(int argc, char **argv) {
  char namebuf[1024];
  if (argc != 2) {
    printf("usage: %s <pool>\n", argv[0]);
    return -1;
  }
  snprintf(namebuf, 1023, "/proc/spl/kstat/zfs/%s/txgs", argv[1]);
  uint64_t last_txg = 0;
  int idle = 0;
  while (true) {
    idle++;
    char buffer[16 * 1024];
    FILE *hnd = fopen(namebuf, "r");
    size_t size = fread(buffer, 1, sizeof(buffer)-1, hnd);
    buffer[size] = 0;
    fclose(hnd);
    vector<string> lines = split(buffer, '\n');
    lines.erase(lines.begin());
    for (string line : lines) {
      //cout << line << endl;
      // birth/1000000000 == seconds of uptime when txg was born
      // otime nanoseconds between birth and open
      // qtime nanoseconds between open and quiessense
      // wtime nanoseconds between quiessense and waiting for sync
      // stime nanoseconds between waiting for sync, and sync completing
      uint64_t txg, birth, ndirty, nread, nwritten, reads, writes, otime, qtime, wtime, stime;
      char state;
      sscanf(line.c_str(), "%ld %ld %c %ld %ld %ld %ld %ld %ld %ld %ld %ld", &txg, &birth, &state, &ndirty, &nread, &nwritten, &reads, &writes, &otime, &qtime, &wtime, &stime);

      if (txg > last_txg) {
        if (state == 'C') {
          float fOtime = (float)otime/1000000000;
          float fQtime = (float)qtime/1000000000;
          float fWtime = (float)wtime/1000000000;
          float fStime = (float)stime/1000000000;
          if ((fOtime > 10) || (fQtime > 1) || (fWtime > 1) || (fStime > 1)) {
            printf("txg:%ld state:%c ndirty:%5ld kb n(read/written):%5ld/%5ld kb read/writes:%5ld/%5ld otime:%5f qtime:%5f wtime:%5f stime:%5f\n", txg, state, ndirty/1024, nread/1024, nwritten/1024, reads, writes, fOtime, fQtime, fWtime, fStime);
            idle = 0;
          }
          last_txg = txg;
        }
      }
    }
    if (idle == 10) {
      puts("");
    }
    struct timespec delay;
    delay.tv_sec = 1;
    delay.tv_nsec = 0;
    nanosleep(&delay, NULL);
  }
  return 0;
}
