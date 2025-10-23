#ifndef SQLITE_HPP_
#define SQLITE_HPP_

#include <sqlite3.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

namespace Database {

class SQLite {
public:
  struct QueryResult {
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
  };

  // Get the singleton instance; returns nullptr if not initialized.
  static SQLite *get_instance();

  // Initialize the singleton. Returns true on first successful init, false if
  // already initialized. Creates a writer connection and N read-only
  // connections to enable concurrent reads and serialized writes.
  static bool init(const std::string &db_path, int read_pool_size = 4,
                   bool enable_wal = true, int busy_timeout_ms = 5000);

  // Disallow copy/move
  SQLite(const SQLite &) = delete;
  SQLite &operator=(const SQLite &) = delete;
  SQLite(SQLite &&) = delete;
  SQLite &operator=(SQLite &&) = delete;

  // Execute write/DDL statements (INSERT/UPDATE/DELETE/CREATE/etc.).
  // Serialized via an internal mutex. Optional error message filled on failure.
  bool execute(std::string_view sql, std::string *error_msg = nullptr);

  // Execute a read-only query and collect all rows into QueryResult.
  // Uses a read-only connection from the pool (supports concurrency).
  bool query(std::string_view sql, QueryResult &out,
             std::string *error_msg = nullptr);

  // Convenience overload returning the whole result by value.
  std::optional<QueryResult> query(std::string_view sql, std::string *error_msg = nullptr) {
    QueryResult r;
    if (!query(sql, r, error_msg)) return std::nullopt;
    return r;
  }

  ~SQLite();

private:
  explicit SQLite(const std::string &db_path, int read_pool_size, bool enable_wal,
                  int busy_timeout_ms);

  // Internal helpers
  static std::string last_sqlite_error(sqlite3 *db, int rc);
  static bool set_pragmas(sqlite3 *db, bool enable_wal, int busy_timeout_ms,
                          std::string *err);
  static bool is_readonly_statement(std::string_view sql);

  // Read connection pool management
  struct ReadConnGuard {
    SQLite &owner;
    int idx;
    ReadConnGuard(SQLite &o, int i) : owner(o), idx(i) {}
    ~ReadConnGuard();
  };
  int acquire_read_conn();
  void release_read_conn(int idx);

private:
  static std::unique_ptr<SQLite> instance_;

  // Writer connection guarded by write_mutex_ to serialize writes.
  sqlite3 *write_db_ = nullptr;
  std::mutex write_mutex_;

  // Read-only connection pool and its coordination primitives.
  std::vector<sqlite3 *> read_dbs_;
  std::queue<int> free_read_indices_;
  std::mutex read_mutex_;
  std::condition_variable read_cv_;
};

} // namespace Database

#endif
