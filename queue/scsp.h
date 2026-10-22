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

struct ScspQueue {

  ScspQueue();
  int write(const char *newel, size_t sz);
  int read(char *input_buffer);

  info metadata[METADATA_SIZE];

  std::mutex m;

private:
  char *data;
  size_t write_idx = 0;
  size_t read_idx = 0;
  bool firstime = 1;
};

// smallest message is 2 kb => 2 * 11 bytes => 2 ^ 23 / 2 ^ 11 = 2 ^ 12
ScspQueue::ScspQueue() { // i want to hold at least 8 2mb messages = 8 * 2 * 2 ^
                         // 10 kb = 2 ^ 4 * 2^ 10 kb = 2 ^ 4 * 2 ^ 20 bytes

  data = (char *)malloc(MAX_SIZE);

  if (!data) {
    std::cout << "ERROR";
  }
}

int ScspQueue::write(const char *newel, size_t sz) {

  std::lock_guard<std::mutex> lock(m);
  assert(sz <= 2 * 1024 * 1024);
  size_t acc_write_idx = (write_idx) % METADATA_SIZE;
  size_t acc_read_idx = read_idx % METADATA_SIZE;

  if (write_idx > (read_idx + 100))
    return 0;
  bool is_new_meta_loop =
      firstime ? false
               : (write_idx / METADATA_SIZE) > (read_idx / METADATA_SIZE);

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
    //    std::cout << "Trying to loop loop. isfirstime: " << firstime << " \n";
  }

  size_t max_idx;

  if (firstime) {
    if (loopy == 0) // we cant wrap around if we still havent read  yet
      max_idx = MAX_SIZE;
    else {
      //      std::cout << "CANT LOOP\n";
      return 0;
    }
  } else {
    if (metadata[acc_read_idx].loop == loopy)
      max_idx = MAX_SIZE;
    else
      max_idx = metadata[acc_read_idx].offset;
  }

  if (offset >= max_idx) {
    //   std::cout << offset << " " << max_idx << " WRITING NO SPACE\n";
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

  std::lock_guard<std::mutex> lock(m);
  size_t tbr = firstime ? 0 : read_idx + 1;
  // write_idx is the next available spot to write (NOT WRITTEN YET)
  // read_idx is the last thing we read
  if (firstime && write_idx == 0) {
    return 0; // nothing to read yet
  }

  if (tbr > (write_idx - 1)) {

    return 0; // nothing to read yet
  }

  read_idx = tbr;

  size_t past_read_idx = (read_idx - 1) % METADATA_SIZE;
  size_t acc_read_idx = read_idx % METADATA_SIZE;

  size_t size;
  size_t start;
  if (read_idx == 0) {
    start = 0;
    size = metadata[0].offset;
  } else {
    start = metadata[past_read_idx].offset;

    if (metadata[acc_read_idx].loop >
        metadata[past_read_idx].loop) // if we go around, we know for sure that
    {
      start = 0; // this is the first element in our buffer
      size = metadata[acc_read_idx].offset;
    } else
      size = metadata[acc_read_idx].offset - metadata[past_read_idx].offset;
  }
  if (size > 2 * 1024 * 1024) {
    // std::cout << read_idx << " " << past_read_idx << " " << acc_read_idx <<
    // size
    // << "\n";
  }

  if (size > 2 * 1024 * 1024 || start >= MAX_SIZE || start + size > MAX_SIZE) {
    std::cerr << "BAD READ: start=" << start << " size=" << size
              << " MAX_SIZE=" << MAX_SIZE << " read_idx=" << read_idx
              << " past_read_idx=" << past_read_idx
              << " acc_read_idx=" << acc_read_idx << "\n";
    //    std::abort();
  }
  assert(size <= 2 * 1024 * 1024);
  // std::cout << "READING from " << start << " size: " << size << "\n";
  memcpy(input_buffer, data + start, size);
  // we want to read this data, so we need to memcpy x
  // 32 bytes -> 32
  firstime = false;
  // std::cout << "read\n";
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

    // 2097152 | 4194304 |
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
        if (!queue->write(chosen_p, sz)) {
          continue;
        }
        //     std::cout << "write!" << "  " << indx << " \n";
        indx++;
      } else {
        // READ

        if (!queue->read(buf)) {
          continue;
          //          std::cout << "WRITE EMPTY\n";
        }

        std::cout << last_read << "\n";
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

BENCHMARK(BM_StringCopy)->Threads(2)->Iterations(1);

BENCHMARK_MAIN();
