#include <iostream>
#include <boost/iostreams/device/mapped_file.hpp>

int main() { 
    boost::iostreams::mapped_file_source src("blocks.index");

    const uint64_t* data_start_ptr = reinterpret_cast<const uint64_t*>(src.data());
    const uint64_t* ptr = data_start_ptr;

    uint64_t pos = *ptr++;

    uint64_t max_size = 0;
    uint64_t min_size = __UINT64_MAX__;
    uint64_t count    = 0;
    uint64_t sum      = 0;

    while (ptr < reinterpret_cast<const uint64_t*>(src.data() + src.size())) {
       uint64_t new_pos = *ptr;
       uint64_t sz      = new_pos - pos;
       max_size         = std::max(sz, max_size);
       min_size         = std::min(sz, min_size);
       sum += sz;

       pos              = new_pos;
       ++ptr;
       ++count;
    }

    std::cout << "max: " << max_size << "\n";
    std::cout << "min: " << min_size << "\n";
    std::cout << "avg: " << sum / count << "\n";
    return 0;
}