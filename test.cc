#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;



int main()
{
    std::string shm_name("/test_shm");

    int shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0666);
    // CHECK_GE(shm_fd, 0) << "shm_open failed for " << shm_name
    //     << ", " << strerror(errno);

    struct stat sb;
    fstat(shm_fd, &sb);
    // CHECK_EQ(0, fstat(shm_fd, &sb)) << strerror(errno);
    auto total_shm_size = sb.st_size;

    void* base_ptr = mmap(0, total_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    // CHECK_NE(base_ptr, (void*) -1) << strerror(errno);
    std::cout << base_ptr;
    return 0;
}
