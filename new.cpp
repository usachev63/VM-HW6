#include <cstring>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <ucontext.h>

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

class MyPool {
public:
  MyPool() {}

  void init(size_t size);

  // address is 8-aligned
  void *alloc(size_t size);

  void free();

  size_t pool_size;
  void *base_ptr;
  char *free_ptr;
} pool;

static const char poolOverflowMessage[] = "pool exhausted\n";

static void pool_sigsegv_handler(int, siginfo_t *info, void *ucontext) {
  if (info->si_addr == pool.free_ptr) {
    write(STDOUT_FILENO, poolOverflowMessage,
          std::char_traits<char>::length(poolOverflowMessage));
    exit(1);
  }
}

static constexpr size_t PAGE_SIZE = 4096;
static constexpr size_t round_up_to_whole_pages(size_t size) {
  return (size + (PAGE_SIZE - 1)) & -PAGE_SIZE;
}

void MyPool::init(size_t size) {
  pool_size = round_up_to_whole_pages(size);
  base_ptr = mmap(nullptr, pool_size, PROT_READ | PROT_WRITE | PROT_GROWSDOWN,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
  if (base_ptr == MAP_FAILED) {
    std::cerr << "mmap failed: " << strerror(errno) << std::endl;
    std::exit(1);
  }
  std::cerr << "mmap ok " << base_ptr << std::endl;
  free_ptr = (char *)base_ptr + pool_size;

  struct sigaction act = {0};
  act.sa_sigaction = pool_sigsegv_handler;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &act, nullptr);
}

void *MyPool::alloc(size_t size) { return free_ptr -= size; }

void MyPool::free() {
  if (munmap(base_ptr, pool_size) != 0) {
    std::cerr << "munmap failed" << std::endl;
    std::exit(1);
  }
  base_ptr = nullptr;
  free_ptr = nullptr;
}

static inline Node *create_list(unsigned n) {
  pool.init(n * sizeof(Node));
  Node *list = nullptr;
  for (unsigned i = 0; i < n; i++) {
    Node *newList = (Node *)pool.alloc(sizeof(Node));
    newList->next = list;
    newList->node_id = i;
    list = newList;
  }
  return list;
}

static inline void delete_list(Node *list) { pool.free(); }

static inline void test(unsigned n) {
  struct rusage start, finish;
  get_usage(start);
  Node *list = create_list(n);
  get_usage(finish);
  delete_list(list);

  struct timeval diff;
  timersub(&finish.ru_utime, &start.ru_utime, &diff);
  double time_used = diff.tv_sec + diff.tv_usec / 1000000.0;
  cout << "Time used: " << time_used << " s\n";

  double mem_used = (finish.ru_maxrss) / 1024.0;
  cout << "Memory used: " << mem_used << " MB\n";
  auto mem_required = n * sizeof(Node) / 1024.0 / 1024.0;
  cout << "Memory required: " << mem_required << " MB\n";

  auto overhead = (mem_used - mem_required) * double(100) / mem_used;
  cout << "Overhead: " << std::fixed << std::setw(4) << std::setprecision(1)
       << overhead << "%\n";
}

int main(const int argc, const char *argv[]) {
  test(10'000'000);
  return EXIT_SUCCESS;
}
