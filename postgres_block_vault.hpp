#pragma once
#include "block_vault.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <pqxx/pqxx>
#include <unistd.h>

class postgres_block_vault : public eosio::blockvault::block_vault_interface {
      pqxx::connection conn;

 public:

   postgres_block_vault(const std::string& options) : conn(options){
      pqxx::work w(conn);
      w.exec("CREATE TABLE IF NOT EXISTS BlockData (watermark_bn bigint, watermark_ts bigint, lib bigint, "
             "previous_block_id "
             "bytea,block oid, "
             "block_size bigint);"
             "CREATE TABLE IF NOT EXISTS SnapshotData (watermark_bn bigint, watermark_ts bigint, snapshot oid);");
      w.commit();

      conn.prepare("insert_proposed_block",
                   "INSERT INTO BlockData (watermark_bn, watermark_ts, lib, previous_block_id, block, block_size) "
                   "SELECT $1, $2, $3, $4, $5, $6  WHERE NOT "
                   "EXISTS (SELECT * FROM BlockData WHERE (watermark_bn >= $1 AND watermark_ts > $2) OR lib > $2)");

      conn.prepare("insert_external_block",
                   "INSERT INTO BlockData (watermark, lib, previous_block_id, block, block_size) SELECT "
                   "COALESCE((SELECT MAX(watermark_bn) FROM BlockData),0), COALESCE((SELECT MAX(watermark_ts) FROM "
                   "BlockData),0), $2, $3, $4, $5 WHERE NOT "
                   "EXISTS (SELECT * FROM BlockData WHERE lib >= $1)");

      conn.prepare("get_block_insertion_result", "SELECT block from BlockData WHERE block=$1");

      conn.prepare("insert_snapshot",
                   "INSERT INTO SnapshotData (watermark_bn, watermark_ts, snapshot) SELECT $1, $2 WHERE NOT EXISTS "
                   "(SELECT * FROM SnapshotData WHERE watermark_bn >= $1 AND watermark_ts >= $2)");

      conn.prepare("get_snapshot_insertion_result", "SELECT snapshot from SnapshotData WHERE snapshot=$1");

      conn.prepare("get_sync_watermark", "SELECT watermark_bn, watermark_ts FROM BlockData WHERE"
                                         "previous_block_id = $1 ORDER BY watermark_bn, watermark_ts LIMIT 1");

      conn.prepare("get_lastest_snapshot_watermark", "SELECT snapshot, watermark_bn, watermark_ts FROM SnapshotData "
                                                     "ORDER BY watermark_bn DSC, watermark_ts DSC LIMIT 1");

      conn.prepare("get_blocks", "SELECT block, block_size FROM BlockData WHERE "
                                 "watermark_bn >= $1 AND watermark_ts >= $2");
   }

   bool append_proposed_block(eosio::blockvault::watermark_t watermark, uint32_t lib, std::string_view block_content) override {
      try {
         std::string_view previous_block_id(block_content.data() + 14, 32);
         pqxx::work       w(conn);

         pqxx::largeobjectaccess obj(w);
         obj.write(block_content.data(), block_content.size());
         pqxx::binarystring blob(previous_block_id.data(), previous_block_id.size());
         w.exec_prepared0("insert_proposed_block", watermark.first, watermark.second, lib, blob, obj.id(),
                          block_content.size());
         auto r = w.exec_prepared1("get_block_insertion_result", obj.id());
         w.commit();
         return true;
      } catch (...) {
         return false;
      }
   }

   bool append_external_block(uint32_t block_num, uint32_t lib, std::string_view block_content) override {
      try {
         std::string_view previous_block_id(block_content.data() + 14, 32);
         pqxx::work       w(conn);

         pqxx::largeobjectaccess obj(w);
         obj.write(block_content.data(), block_content.size());
         pqxx::binarystring blob(previous_block_id.data(), previous_block_id.size());
         w.exec_prepared0("insert_external_block", block_num, lib, blob, obj.id(), block_content.size());
         auto r = w.exec_prepared1("get_block_insertion_result", obj.id());
         w.commit();
         return true;
      } catch (...) {
         return false;
      }
   }

   bool propose_snapshot(eosio::blockvault::watermark_t watermark, const char* snapshot_filename) override {
      try {
         std::filebuf infile;

         infile.open(snapshot_filename, std::ios::in);
         const int chunk_size = 4096;
         char      chunk[chunk_size];

         pqxx::work              w(conn);
         pqxx::largeobjectaccess obj(w);

         auto sz = chunk_size;
         while (sz == chunk_size) {
            sz = infile.sgetn(chunk, chunk_size);
            obj.write(chunk, sz);
         };

         w.exec_prepared0("insert_snapshot", watermark.first, watermark.second, obj.id());
         auto r = w.exec_prepared1("get_snapshot_insertion_result", obj.id());

         w.commit();
         return true;
      } catch (...) {
         return false;
      }
   }

   void sync(std::string_view previous_block_id, eosio::blockvault::sync_callback& callback) override {

      using eosio::blockvault::watermark_t;
      pqxx::work trx(conn);

      std::optional<watermark_t> watermark_after_id;
      std::optional<watermark_t> latest_snapshot_watermark;
      pqxx::oid                  latest_snapshot_oid;

      if (previous_block_id.size()) {
         pqxx::binarystring blob(previous_block_id.data(), previous_block_id.size());
         auto               r = trx.exec_prepared1("get_sync_watermark", blob);

         if (!r[0].is_null())
            watermark_after_id = std::make_pair(r[0].as<uint32_t>(), r[1].as<uint32_t>());
      }

      auto r0 = trx.exec_prepared1("get_lastest_snapshot_watermark");
      if (!r0[1].is_null()) {
         latest_snapshot_oid       = r0[0].as<pqxx::oid>();
         latest_snapshot_watermark = std::make_pair(r0[1].as<uint32_t>(), r0[2].as<uint32_t>());
      }

      if (!watermark_after_id && !latest_snapshot_watermark) {
         throw std::runtime_error("unable to sync");
      }

      bool need_snapshot = latest_snapshot_watermark.has_value() &&
                           (!watermark_after_id || watermark_after_id->first < latest_snapshot_watermark->first);

      auto watermark_to_sync = need_snapshot ? *latest_snapshot_watermark : *watermark_after_id;

      if (need_snapshot) {

         pqxx::largeobject snapshot_obj(latest_snapshot_oid);

         char tmp_name[PATH_MAX];
         snprintf(tmp_name, PATH_MAX, "/tmp/eos-snapshot-XXXXXX");
         int fd = mkstemp(tmp_name);
         if (fd == -1) {
            throw std::filesystem::filesystem_error("mkstemp error", tmp_name,
                                                    std::make_error_code(static_cast<std::errc>(errno)));
         }

         snapshot_obj.to_file(trx, tmp_name);
         close(fd);
         callback.on_snapshot(tmp_name);
      }

      auto r = trx.exec_prepared("get_blocks", watermark_to_sync.first, watermark_to_sync.second);

      std::vector<char> block_data;

      for (const auto& x : r) {
         pqxx::oid               block_oid  = x[0].as<pqxx::oid>();
         uint64_t                block_size = x[1].as<uint64_t>();
         pqxx::largeobjectaccess obj(trx, block_oid, std::ios::in | std::ios::binary);
         block_data.resize(block_size);
         obj.read(block_data.data(), block_size);
         callback.on_block(std::string{block_data.data(), block_data.size()});
      }

      trx.commit();
   }
};