#include "sqlite.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace Database {

std::unique_ptr<SQLite> SQLite::instance_ = nullptr;

static int open_db(sqlite3 **out, const std::string &path, bool readonly) {
  int flags = readonly ? SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX
                       : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                             SQLITE_OPEN_NOMUTEX;
  // We use separate connections per thread type; NOMUTEX is fine here.
  return sqlite3_open_v2(path.c_str(), out, flags, nullptr);
}

SQLite *SQLite::get_instance() { return instance_.get(); }

bool SQLite::init(const std::string &db_path, int read_pool_size,
                  bool enable_wal, int busy_timeout_ms) {
  if (instance_)
    return false;
  instance_ = std::unique_ptr<SQLite>(new SQLite(
      db_path, std::max(1, read_pool_size), enable_wal, busy_timeout_ms));
  return true;
}

SQLite::SQLite(const std::string &db_path, int read_pool_size, bool enable_wal,
               int busy_timeout_ms) {
  // Open writer connection
  if (open_db(&write_db_, db_path, /*readonly=*/false) != SQLITE_OK) {
    // Leave instance in a safe destructible state
    write_db_ = nullptr;
    throw std::runtime_error("Failed to open SQLite DB for writing");
  }
  std::string err;
  if (!set_pragmas(write_db_, enable_wal, busy_timeout_ms, &err)) {
    sqlite3_close(write_db_);
    write_db_ = nullptr;
    throw std::runtime_error("Failed to configure SQLite writer: " + err);
  }

  // Open read-only pool
  read_dbs_.reserve(static_cast<size_t>(read_pool_size));
  for (int i = 0; i < read_pool_size; ++i) {
    sqlite3 *rdb = nullptr;
    if (open_db(&rdb, db_path, /*readonly=*/true) != SQLITE_OK) {
      // Clean up already opened
      for (sqlite3 *p : read_dbs_)
        sqlite3_close(p);
      sqlite3_close(write_db_);
      write_db_ = nullptr;
      throw std::runtime_error("Failed to open SQLite DB for reading");
    }
    std::string rerr;
    if (!set_pragmas(rdb, enable_wal, busy_timeout_ms, &rerr)) {
      sqlite3_close(rdb);
      for (sqlite3 *p : read_dbs_)
        sqlite3_close(p);
      sqlite3_close(write_db_);
      write_db_ = nullptr;
      throw std::runtime_error("Failed to configure SQLite reader: " + rerr);
    }
    read_dbs_.push_back(rdb);
    free_read_indices_.push(i);
  }
}

SQLite::~SQLite() {
  if (write_db_) {
    sqlite3_close(write_db_);
    write_db_ = nullptr;
  }
  for (sqlite3 *p : read_dbs_) {
    sqlite3_close(p);
  }
  read_dbs_.clear();
  while (!free_read_indices_.empty())
    free_read_indices_.pop();
}

std::string SQLite::last_sqlite_error(sqlite3 *db, int rc) {
  return std::format("{} (rc={})", sqlite3_errmsg(db), rc);
}

bool SQLite::set_pragmas(sqlite3 *db, bool enable_wal, int busy_timeout_ms,
                         std::string *err) {
  if (busy_timeout_ms > 0) {
    int rc = sqlite3_busy_timeout(db, busy_timeout_ms);
    if (rc != SQLITE_OK) {
      if (err)
        *err = last_sqlite_error(db, rc);
      return false;
    }
  }
  if (enable_wal) {
    // Enable WAL to allow concurrent reads while writes happen.
    sqlite3_stmt *stmt = nullptr;
    int rc =
        sqlite3_prepare_v2(db, "PRAGMA journal_mode=WAL;", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
      if (err)
        *err = last_sqlite_error(db, rc);
      return false;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      // PRAGMA returns one row with the mode string; ignore
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
      if (err)
        *err = last_sqlite_error(db, rc);
      return false;
    }

    // Reasonable default for durability vs performance.
    rc = sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr,
                      nullptr);
    if (rc != SQLITE_OK) {
      if (err)
        *err = last_sqlite_error(db, rc);
      return false;
    }
  }
  return true;
}

bool SQLite::is_readonly_statement(std::string_view sql) {
  // Trim leading whitespace and check the first keyword.
  auto it = sql.begin();
  while (it != sql.end() && std::isspace(static_cast<unsigned char>(*it)))
    ++it;
  std::string kw;
  while (it != sql.end() && std::isalpha(static_cast<unsigned char>(*it))) {
    kw.push_back(
        static_cast<char>(std::toupper(static_cast<unsigned char>(*it))));
    ++it;
  }
  // Common read-only statements
  return (kw == "SELECT" || kw == "PRAGMA" || kw == "WITH");
}

int SQLite::acquire_read_conn() {
  std::unique_lock<std::mutex> lk(read_mutex_);
  read_cv_.wait(lk, [&] { return !free_read_indices_.empty(); });
  int idx = free_read_indices_.front();
  free_read_indices_.pop();
  return idx;
}

void SQLite::release_read_conn(int idx) {
  {
    std::lock_guard<std::mutex> lk(read_mutex_);
    free_read_indices_.push(idx);
  }
  read_cv_.notify_one();
}

SQLite::ReadConnGuard::~ReadConnGuard() { owner.release_read_conn(idx); }

bool SQLite::execute(std::string_view sql, std::string *error_msg) {
  // Guard to ensure only one writer at a time
  std::lock_guard<std::mutex> lk(write_mutex_);
  if (!write_db_) {
    if (error_msg)
      *error_msg = "Writer DB not initialized";
    return false;
  }
  // Safety: prevent mistakenly issuing a SELECT on the writer if caller expects
  // read. Not required, but helps avoid misuse.
  int rc = SQLITE_OK;
  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(write_db_, std::string(sql).c_str(), -1, &stmt,
                          nullptr);
  if (rc != SQLITE_OK) {
    if (error_msg)
      *error_msg = last_sqlite_error(write_db_, rc);
    return false;
  }
  // Step until done (for multi-row DML)
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    // ignore any result rows (shouldn't be rows for DML)
  }
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    if (error_msg)
      *error_msg = last_sqlite_error(write_db_, rc);
    return false;
  }
  return true;
}

bool SQLite::query(std::string_view sql, QueryResult &out,
                   std::string *error_msg) {
  // Select a read-only connection from pool
  const int idx = acquire_read_conn();
  ReadConnGuard guard(*this, idx);
  sqlite3 *db = read_dbs_[static_cast<size_t>(idx)];

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db, std::string(sql).c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    if (error_msg)
      *error_msg = last_sqlite_error(db, rc);
    return false;
  }

  // Column names
  out.columns.clear();
  out.rows.clear();

  bool cols_initialized = false;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    int ncol = sqlite3_column_count(stmt);
    if (!cols_initialized) {
      out.columns.reserve(ncol);
      for (int i = 0; i < ncol; ++i) {
        const char *name = sqlite3_column_name(stmt, i);
        out.columns.emplace_back(name ? name : "");
      }
      cols_initialized = true;
    }

    std::vector<std::string> row;
    row.reserve(ncol);
    for (int i = 0; i < ncol; ++i) {
      int t = sqlite3_column_type(stmt, i);
      switch (t) {
      case SQLITE_INTEGER: {
        sqlite3_int64 v = sqlite3_column_int64(stmt, i);
        row.emplace_back(std::to_string(v));
        break;
      }
      case SQLITE_FLOAT: {
        double v = sqlite3_column_double(stmt, i);
        row.emplace_back(std::to_string(v));
        break;
      }
      case SQLITE_TEXT: {
        const unsigned char *txt = sqlite3_column_text(stmt, i);
        row.emplace_back(txt ? reinterpret_cast<const char *>(txt) : "");
        break;
      }
      case SQLITE_NULL: {
        row.emplace_back("");
        break;
      }
      case SQLITE_BLOB: {
        const void *blob = sqlite3_column_blob(stmt, i);
        int bytes = sqlite3_column_bytes(stmt, i);
        // Store blobs as hex string for a simple representation
        const unsigned char *p = static_cast<const unsigned char *>(blob);
        static const char hex[] = "0123456789ABCDEF";
        std::string s;
        s.resize(static_cast<size_t>(bytes) * 2);
        for (int b = 0; b < bytes; ++b) {
          s[static_cast<size_t>(b) * 2] = hex[(p[b] >> 4) & 0xF];
          s[static_cast<size_t>(b) * 2 + 1] = hex[p[b] & 0xF];
        }
        row.emplace_back(std::move(s));
        break;
      }
      default:
        row.emplace_back("");
        break;
      }
    }
    out.rows.emplace_back(std::move(row));
  }

  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    if (error_msg)
      *error_msg = last_sqlite_error(db, rc);
    return false;
  }
  return true;
}

} // namespace Database
