#include <algorithm>
#include <benchmark/benchmark.h>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <vector>

#define MAX_SIZE (1 << 26)
#define MIN_MSG_SIZE (1 << 11)
#define METADATA_SIZE (MAX_SIZE / MIN_MSG_SIZE)

struct info {
  size_t offset;
  size_t loop;
};

struct read_idx_info {
  size_t read_indx;
  bool first_time;
};

struct ScspQueue {

  ScspQueue();
  int write(const char *newel, size_t sz);
  int read(char *input_buffer);

  info metadata[METADATA_SIZE];

  std::mutex m;

private:
  char *data;
  size_t write_idx = 0;
  read_idx_info read_info;
};

// smallest message is 2 kb => 2 * 11 bytes => 2 ^ 23 / 2 ^ 11 = 2 ^ 12
ScspQueue::ScspQueue() { // i want to hold at least 8 2mb messages = 8 * 2 * 2 ^
                         // 10 kb = 2 ^ 4 * 2^ 10 kb = 2 ^ 4 * 2 ^ 20 bytes

  data = (char *)malloc(MAX_SIZE);
  read_info.read_indx = 0;
  read_info.first_time = 1;
  if (!data) {
    std::cout << "ERROR";
  }
}

int ScspQueue::write(const char *newel, size_t sz) {

  read_idx_info safe_read_info = read_info;

  assert(sz <= 2 * 1024 * 1024);
  size_t acc_write_idx = (write_idx) % METADATA_SIZE;
  size_t acc_read_idx = safe_read_info.read_indx % METADATA_SIZE;

  bool is_new_meta_loop = safe_read_info.first_time
                              ? false
                              : (write_idx / METADATA_SIZE) >
                                    (safe_read_info.read_indx / METADATA_SIZE);

  if (is_new_meta_loop && (acc_write_idx == acc_read_idx)) // we would override
  {
    return 0;
  }
  // we can write the metadata here (we know for sure we can keeep the data
  // here)

  size_t start_idx;
  int loopy;
  if (write_idx == 0) {
    loopy = 0;
    start_idx = 0; // simple case
  } else if (acc_write_idx == 0) {
    loopy = metadata[METADATA_SIZE - 1].loop;
    start_idx = metadata[METADATA_SIZE - 1].offset; // last element
  } else {
    loopy = metadata[acc_write_idx - 1].loop;
    start_idx = metadata[acc_write_idx - 1].offset;
  }

  bool canloop =
      loopy == metadata[acc_read_idx]
                   .loop; // i cant loop a second time, i would override myself
  size_t offset = start_idx + sz;
  if (offset >= MAX_SIZE && canloop) {
    start_idx = 0;
    offset = sz;
    loopy++;
  }

  size_t max_idx;

  if (safe_read_info.first_time) {
    if (loopy == 0) // we cant wrap around if we still havent read anything yet
      max_idx = MAX_SIZE;
    else {
      return 0;
    }
  } else {
    if (metadata[acc_read_idx].loop == loopy)
      max_idx = MAX_SIZE;
    else
      max_idx = metadata[acc_read_idx].offset;
  }

  if (offset >= max_idx) {
    return 0;
  }
  // std::cout << "WRITING FROM " << start_idx << " to " << start_idx + sz
  //       << " write_indx: " << loopy << "\n";
  metadata[acc_write_idx].loop = loopy;
  metadata[acc_write_idx].offset = start_idx + sz;
  memcpy(data + start_idx, newel, sz);

  write_idx++;
  return 1;
}

// we will assume the buffer is big enough
int ScspQueue::read(char *input_buffer) {

  size_t safe_write_idx = write_idx;
  size_t tbr = read_info.first_time ? 0 : read_info.read_indx + 1;
  // write_idx is the next available spot to write (NOT WRITTEN YET)
  // read_idx is the last thing we read
  if (read_info.first_time && safe_write_idx == 0) {
    return 0; // nothing to read yet
  }

  if (tbr > (safe_write_idx - 1)) {

    return 0; // nothing to read yet
  }
  size_t past_read_idx = (tbr - 1) % METADATA_SIZE;
  size_t acc_read_idx = tbr % METADATA_SIZE;

  size_t size;
  size_t start;
  if (tbr == 0) {
    start = 0;
    size = metadata[0].offset;
  } else {
    start = metadata[past_read_idx].offset;

    if (metadata[acc_read_idx].loop >
        metadata[past_read_idx].loop) // if we go around, we know for sure that
                                      // this is the first element in our buffer

    {
      start = 0;
      size = metadata[acc_read_idx].offset;
    } else
      size = metadata[acc_read_idx].offset - metadata[past_read_idx].offset;
  }
  if (size > 2 * 1024 * 1024 || start >= MAX_SIZE || start + size > MAX_SIZE) {
    std::cerr << "ERROR READ: start=" << start << " size=" << size
              << " MAX_SIZE=" << MAX_SIZE << " read_idx=" << read_info.read_indx
              << " past_read_idx=" << past_read_idx
              << " acc_read_idx=" << acc_read_idx << "\n";
    std::abort();
  }
  assert(size <= 2 * 1024 * 1024);
  // std::cout << "READING from " << start << " size: " << size << "\n";
  memcpy(input_buffer, data + start, size);
  read_idx_info next{tbr, false};

  read_info = next;

  return 1;
}

ScspQueue *queue;

// Define another benchmark
static void BM_StringCopy(benchmark::State &state) {

  std::string max_str(2 * 1024 * 1024, 'b'); // 2 MB of 'A'
  char *p_max = max_str.data();

  std::string min_str(2 * 1024, 's');
  ; // 2 KB of "B"
  char *p_min = min_str.data();

  srand(time(0));
  char *buf = (char *)malloc(2 * 1024 * 1024 + 1);

  if (state.thread_index() == 0) {
    queue = new ScspQueue;
    srand(time(0));
  }
  for (auto _ : state) {

    int indx = 0;
    int last_read = -1;
    for (int i = 0; i < 9999; i++) {
      if (state.thread_index() == 0) // WRITE
      {
        char *chosen_p;
        size_t sz;

        if (rand() % 2) {
          chosen_p = p_min;
          sz = 2 * 1024;
        } else {
          chosen_p = p_max;
          sz = 2 * 1024 * 1024;
        }

        chosen_p[0] = '0' + (indx / 100) % 10;
        chosen_p[1] = '0' + (indx / 10) % 10;
        chosen_p[2] = '0' + indx % 10;
        while (!queue->write(chosen_p, sz)) {
        }
        indx++;
      } else {
        // READ

        while (!queue->read(buf)) {
        }

        //        std::cout << "da: " << buf[0] << buf[1] << buf[2] << "\n";
        int expected = last_read + 1;
        assert(buf[0] == ('0' + (expected / 100) % 10));
        assert(buf[1] == ('0' + (expected / 10) % 10));
        assert(buf[2] == ('0' + (expected % 10)));
        last_read = expected;
      }
    }
  }
}

BENCHMARK(BM_StringCopy)->Threads(2)->Iterations(100);

// int main() { std::cout << sizeof(read_info); }
BENCHMARK_MAIN();
