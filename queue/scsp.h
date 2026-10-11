#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#define MAX_SIZE (1 << 23)
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

private:
  char *data;
  info metadata[METADATA_SIZE];
  size_t write_idx = 0;
  size_t read_idx = -1;
};

// smalles message is 2 kb => 2 * 11 bytes => 2 ^ 23 / 2 ^ 11 = 2 ^ 12
ScspQueue::ScspQueue() { // i want to hold at least 8 2mb messages = 8 * 2 * 2 ^
                         // 10 kb = 2 ^ 4 * 2^ 10 kb = 2 ^ 4 * 2 ^ 20 bytes;j

  data = (char *)malloc(MAX_SIZE);

  if (!data) {
    std::cout << "ERROR";
  }
}

int ScspQueue::write(const char *newel, size_t sz) {
  size_t acc_write_idx = (write_idx) % METADATA_SIZE;
  size_t acc_read_idx = read_idx % METADATA_SIZE;

  bool is_new_meta_loop =
      (write_idx / METADATA_SIZE) > (read_idx / METADATA_SIZE);

  if (is_new_meta_loop && (acc_write_idx == acc_read_idx)) // we would override
  {

    std::cout << write_idx << " " << read_idx << " WRITING FAILED\n";
    return 0;
  }
  // we can write the metadata here (we know for sure we can keeep the data
  // here)

  int start_idx;
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

  int offset = start_idx + sz;
  if (offset >= MAX_SIZE) {
    std::cout << "BUFFER TOO BIG ?\n"; // TODO i would get stuck ehre
    start_idx = 0;
    offset = sz;
    loopy++;
  }

  int max_idx;

  if (read_idx == -1 || (metadata[acc_read_idx].loop == loopy)) {
    max_idx = MAX_SIZE;
  } else {
    max_idx = metadata[acc_read_idx].offset;
  }

  if (offset >= max_idx) {
    std::cout << offset << " " << max_idx << " WRITING NO SPACE\n";
    return 0;
  }

  metadata[acc_write_idx].loop = loopy;
  metadata[acc_write_idx].offset = start_idx + sz;
  memcpy(data + start_idx, newel, sz);

  write_idx++;
  std::cout << "writing " << newel[3] << "\n";
  return 1;
}

// we will assume the buffer is big enough
int ScspQueue::read(char *input_buffer) {

  // write_idx is the next available spot to write (NOT WRITTEN YET)
  // read_idx is the last thing we read

  // If what im abbout to read is not written yet
  if ((read_idx + 1) > (write_idx - 1)) {
    std::cout << "NOTHING YET\n";
    return 0; // nothing to read yet
  }

  read_idx++;

  size_t past_read_idx = (read_idx - 1) % METADATA_SIZE;
  size_t acc_read_idx = read_idx % METADATA_SIZE;

  int size;
  int start;
  if (read_idx == 0) {
    start = 0;
    size = metadata[0].offset;
  } else {
    start = metadata[past_read_idx].offset;
    size = metadata[acc_read_idx].offset - metadata[past_read_idx].offset;
  }

  memcpy(input_buffer, data + start, size);
  // we want to read this data, so we need to memcpy x
  // 32 bytes -> 32

  read_idx++;
  std::cout << "read\n";
  return 1;
}

int main() {
  ScspQueue queue;

  std::string max_str(2 * 1024 * 1024, 'b'); // 2 MB of 'A'
  char *p_max = max_str.data();

  std::string min_str(2 * 1024, 's'); // 2 KB of "B"
  char *p_min = min_str.data();

  srand(time(0));

  char *buf = (char *)malloc(2 * 1024 * 1024);

  // 2097152 | 4194304 |
  //  queue.write(p_max, 2 * 1024 * 1024);
  //  queue.write(p_max, 2 * 1024 * 1024);
  //  queue.read(buf);
  //  queue.write(p_max, 2 * 1024 * 1024);

  //  return 0;
  int indx = 0;
  for (int i = 0; i < 200; i++) {
    if (rand() % 2) // WRITE
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
      chosen_p[0] = '0' + indx / 10;
      chosen_p[1] = '0' + indx % 10;
      if (!queue.write(chosen_p, sz)) {

        continue;
      }
      indx++;
    } else {
      // READ
      if (!queue.read(buf)) {
        std::cout << "WRITE EMPTY\n";
        continue;
      }
      std::cout << buf[0] << buf[1] << buf[3] << "\n";
    }
  }
}
