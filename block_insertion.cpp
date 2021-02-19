#include "postgres_block_vault.hpp"
#include <boost/program_options.hpp>
#include <chrono>
#include <deque>
#include <random>
#include <vector>
#include <thread>

using namespace std::chrono;

std::vector<int> create_random_data(int numbytes) {
   std::mt19937     rng; // default constructed, seeded with fixed seed
   std::vector<int> data((numbytes + sizeof(int) - 1) / sizeof(int));
   std::generate(data.begin(), data.end(), std::ref(rng));
   return data;
}

void test_insert_proposed_block(eosio::blockvault::block_vault_interface& vault,
                                                uint64_t num_iterations, uint64_t block_size, std::chrono::milliseconds interval ) {

   // std::deque<uint64_t> insert_durations;
   std::vector<int>     data = create_random_data(block_size);
   const char*          v    = reinterpret_cast<const char*>(data.data());
   for (uint64_t i = 0; i < num_iterations; ++i) {
      auto start = high_resolution_clock::now();
      vault.append_proposed_block( {i + 1, i + 1},i + 1, std::string_view(v, block_size));
      auto stop = high_resolution_clock::now();
      // insert_durations.push_back(std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
      std::cout << std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() << std::endl;
      if (interval.count() > 0)
         std::this_thread::sleep_for( interval - (high_resolution_clock::now() - start));
   }
}

int main(int argc, const char** argv) {
   try {

      namespace po = boost::program_options;
      uint64_t num_iterations;
      uint64_t block_size;
      bool     create_table_only;
      uint64_t send_interval;

      po::options_description desc("Allowed options");
      desc.add_options()("help", "produce help message")(
          "iterations", po::value<uint64_t>(&num_iterations)->default_value(10000), "set number of iterations")(
          "block_size", po::value<uint64_t>(&block_size)->default_value(4096), "set block size")(
          "send_interval", po::value<uint64_t>(&send_interval)->default_value(500), "send interval in milliseconds");

      po::variables_map vm;
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 1;
      }

      postgres_block_vault vault;

      test_insert_proposed_block(vault, num_iterations, block_size, std::chrono::milliseconds(send_interval));

      // std::copy(result.begin(), result.end(), std::ostream_iterator<uint64_t>(std::cout, "\n"));

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      return 1;
   }
}
