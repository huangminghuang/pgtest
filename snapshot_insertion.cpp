#include "postgres_block_vault.hpp"
#include <boost/program_options.hpp>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace std::chrono;

int main(int argc, const char** argv) {
   try {

      namespace po = boost::program_options;

      uint64_t                num_iterations;
      po::options_description desc("Allowed options");
      desc.add_options()("help", "produce help message")(
          "iterations", po::value<uint64_t>(&num_iterations)->default_value(10000), "set number of iterations")(
          "snapshot_file", po::value<std::string>()->default_value("snapshot.bin"),
          "snapshot file name")("sleep_time", po::value<uint64_t>()->default_value(3), "sleep time in sec");

      po::variables_map vm;
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 1;
      }

      postgres_block_vault vault;

      uint32_t    watermark         = 1;
      std::string snapshot_filename = vm["snapshot_file"].as<std::string>();
      auto        sleep_time        = std::chrono::seconds(vm["sleep_time"].as<uint64_t>());

      for (uint64_t i = 0; i < num_iterations; ++i) {
         auto start = high_resolution_clock::now();
         vault.propose_snapshot({watermark, watermark}, snapshot_filename.c_str());
         auto stop = high_resolution_clock::now();
         std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << std::endl;
         ++watermark;
         std::this_thread::sleep_for(sleep_time);
      }

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      return 1;
   }
}
