#include "postgres_block_vault.hpp"
#include <boost/program_options.hpp>
#include <chrono>
#include <random>
#include <vector>
#include <arpa/inet.h>

using namespace std::chrono;
using eosio::blockvault::watermark_t;

std::string to_hex(std::string_view s, bool upper_case = true) {
   std::ostringstream ret;

   for (size_t i = 0; i < s.length(); ++i)
      ret << std::hex << std::setfill('0') << std::setw(2) << (upper_case ? std::uppercase : std::nouppercase)
          << (int)s[i] << " ";

   return ret.str();
}

std::vector<char> get_block(uint32_t block_num, size_t size) {
   std::vector<char> result(size);
   uint32_t          big_endian_block_num = htonl(block_num);
   memcpy(result.data() + 14, &big_endian_block_num, sizeof(uint32_t));
   return result;
}

void append_proposed_block(postgres_block_vault& vault, watermark_t watermark, uint32_t lib, uint32_t block_num) {
   auto b = get_block(block_num, 128);
   vault.append_proposed_block(watermark, lib, {b.data(), b.size()});
}

void append_external_block(postgres_block_vault& vault, uint32_t block_num, uint32_t lib) {
   auto b = get_block(block_num, 128);
   vault.append_external_block(block_num, lib, {b.data(), b.size()});
}

class test_callback : public eosio::blockvault::sync_callback {
 public:
   void on_block(watermark_t watermark, std::string_view block) override {
      std::cout << "on_block( {" << watermark.first << ", " << watermark.second << "}, " << to_hex(block) << ")\n";
   }
   void on_snapshot(watermark_t watermark, std::string_view snapshotfile) override {
      std::cout << "on_snapshot({" << watermark.first << ", " << watermark.second << "}, " << snapshotfile << ")\n";
   }
};

std::string_view block_id_for(uint32_t block_num) {
   static char buf[32]              = {0};
   uint32_t    big_endian_block_num = htonl(block_num);
   memcpy(&buf, &big_endian_block_num, sizeof(big_endian_block_num));
   return std::string_view(buf, 32);
}

int main() {
   try {

      namespace po = boost::program_options;
      postgres_block_vault vault;

      //   append_proposed_block(vault, 10, 10, 1);
      //   append_external_block(vault, 11, 2);
      //   vault.propose_snapshot(12, "test.bin");
      //   append_proposed_block(vault, 11, 11, 3);
      //   append_proposed_block(vault, 12, 12, 4);
      //   append_proposed_block(vault, 13, 13, 5);
      //   append_proposed_block(vault, 14, 14, 6);

      test_callback callback;
      vault.sync(block_id_for(5), callback);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      return 1;
   }
}
