#include <iostream>
#include <vector>
#include <cstring> 
#include <cstdlib>

#define MAX_SIZE (1 << 23)
#define MIN_MSG_SIZE (1 << 11)


struct ScspQueue
{

	ScspQueue();
  int write(const char* newel, size_t sz);
  int read(char *input_buffer);

private:
char *data;
int metadata[MAX_SIZE / MIN_MSG_SIZE]; 
int write_idx=0; 
int read_idx=0; 
};

// smalles message is 2 kb => 2 * 11 bytes => 2 ^ 23 / 2 ^ 11 = 2 ^ 12   
ScspQueue::ScspQueue() { // i want to hold at least 8 2mb messages = 8 * 2 * 2 ^ 10 kb = 2 ^ 4 * 2^ 10 kb = 2 ^ 4 * 2 ^ 20 bytes;j 


  data = (char*) malloc(MAX_SIZE);

if (!data) 
  {
    std::cout << "ERROR";}

}

int ScspQueue::write(const char* newel,
                    size_t sz)
{

  if (write_idx >= (MAX_SIZE / MIN_MSG_SIZE))
  {

    std::cout << "WRITING FAILED\n";
      return 0;
  }

  int start_idx;
  if (write_idx == 0) 
  {
    start_idx = 0;
    // we hold the end offset 
  }
  else 
  {
    start_idx = metadata[write_idx - 1]; 
  }

  if (start_idx + sz > MAX_SIZE)
  {
    return 0;
  }

  metadata[write_idx] = start_idx + sz; 
  memcpy(data + start_idx, newel, sz); 

  write_idx ++;

  return 1; 
}


// we will assume the buffer is big enough 
int ScspQueue::read(char *input_buffer)
{
  
  // write_idx is the next available spot to write (NOT WRITTEN YET) 
  // read_idx is the next spot to read (NOT READ YET) 
  
  // If what im abbout to read is not written yet 
  if (read_idx >= write_idx) 
  {
    return 0; // nothing to read yet
  }

  int size;
  int start = 0;
  if (read_idx == 0)
  {
    start = 0;
    size = metadata[0];
  }
  else 
  {
     start = metadata[read_idx - 1];
    size = metadata[read_idx] - metadata[read_idx - 1];
  }
  memcpy(input_buffer, data + start, size);
  // we want to read this data, so we need to memcpy x
  // 32 bytes -> 32 

  read_idx ++; 
  return 1; 
}

int main()
{
  ScspQueue queue; 

  std::string max_str(2 * 1024 * 1024, 'A');  // 2 MB of 'A'
  char *p_max = max_str.data();           

  std::string min_str(2 * 1024, 'B'); // 2 KB of "B" 
  char *p_min = min_str.data();           
  
  srand(time(0));

  
 char *buf = (char*) malloc(2 *1024 * 1024);


  int indx = 0;
  for (int i = 0; i < 200; i ++)
  {
      if (rand() % 2) // WRITE 
      {
        
        char* chosen_p;
        size_t sz; 

       if (rand() % 2) 
       {
         chosen_p = p_min; 
         sz = 2 *1024;
       }
       else 
       {
          chosen_p = p_max; 
          sz = 2 * 1024 * 1024; 
       }
        chosen_p[0] = '0' +  indx/ 10;
        chosen_p[1] = '0' + indx % 10; 
        if (!queue.write(chosen_p, sz))
       {

        std::cout << "WRITE FULL\n";
        continue;  
      }
      indx ++;
    }
       else 
  {
    // READ 
    if (!queue.read(buf))
      {
       std::cout << "WRITE EMPTY\n"; 
      continue; 
      }
    std::cout <<buf[0] << buf[1] <<"\n";
  }
  }


}


