#include "postgres_block_vault.hpp"
#include <chrono>
#include <filesystem>

int main() {
   postgres_block_vault vault;

   auto start = high_resolution_clock::now();

   pqxx::oid         oid = 17454;
   pqxx::largeobject snapshot_obj(oid);

   char tmp_name[PATH_MAX];
   snprintf(tmp_name, PATH_MAX, "/tmp/eos-snapshot-XXXXXX");
   int fd = mkstemp(tmp_name);
   if (fd == -1) {
      throw std::filesystem::filesystem_error("mkstemp error", tmp_name,
                                              std::make_error_code(static_cast<std::errc>(errno)));
   }

   snapshot_obj.to_file(trx, tmp_name);
   close(fd);

   auto stop = high_resolution_clock::now();
   std::cout << std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() << std::endl;
   return 0;
}