#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>

using namespace std;

static void get_usage(struct rusage &usage) {
  if (getrusage(RUSAGE_SELF, &usage)) {
    perror("Cannot get usage");
    exit(EXIT_SUCCESS);
  }
}

struct Node {
  Node *next;
  unsigned node_id;
};

static inline Node *create_list(unsigned n) {
  Node *list = nullptr;
  for (unsigned i = 0; i < n; i++)
    list = new Node({list, i});
  return list;
}

static inline void delete_list(Node *list) {
  while (list) {
    Node *node = list;
    list = list->next;
    delete node;
  }
}

static inline void test(unsigned n) {
  struct rusage start, finish;
  get_usage(start);
  delete_list(create_list(n));
  get_usage(finish);

  struct timeval diff;
  timersub(&finish.ru_utime, &start.ru_utime, &diff);
  double time_used = diff.tv_sec + diff.tv_usec / 1000000.0;
  cout << "Time used: " << time_used << " s\n";

  double mem_used = (finish.ru_maxrss - start.ru_maxrss) / 1024.0;
  cout << "Memory used: " << mem_used << " MB\n";

  auto mem_required = n * sizeof(Node) / 1024.0 / 1024.0;
  auto overhead = (mem_used - mem_required) * double(100) / mem_used;
  cout << "Overhead: " << std::fixed << std::setw(4) << std::setprecision(1)
       << overhead << "%\n";
}

int main(const int argc, const char *argv[]) {
  test(10'000'000);
  return EXIT_SUCCESS;
}
