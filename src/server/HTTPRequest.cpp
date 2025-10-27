#include "HTTPRequest.hpp"
#include "logger.hpp"
#include "sqlite.hpp"
#include <cassert>
#include <cctype>
#include <format>
#include <optional>
#include <string_view>

using namespace std;
using namespace Web;

const unordered_set<string> HTTPRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

const unordered_map<string, int> HTTPRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HTTPRequest::init() {
  method_ = path_ = version_ = body_ = "";
  state_ = PARSE_STATE::REQUEST_LINE;
  header_.clear();
  post_.clear();
}

bool HTTPRequest::IsKeepAlive() const {
  if (header_.count("Connection") == 1) {
    return header_.find("Connection")->second == "keep-alive" &&
           version_ == "1.1";
  }
  return false;
}

bool HTTPRequest::parse(Buffer &buff) {
  const char CRLF[] = "\r\n";
  if (buff.ReadableBytes() <= 0) {
    return false;
  }
  while (buff.ReadableBytes() && state_ != PARSE_STATE::FINISH) {
    const char *lineEnd =
        search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
    std::string line(buff.Peek(), lineEnd);
    switch (state_) {
    case PARSE_STATE::REQUEST_LINE:
      if (!ParseRequestLine_(line)) {
        return false;
      }
      ParsePath_();
      break;
    case PARSE_STATE::HEADERS:
      ParseHeader_(line);
      if (buff.ReadableBytes() <= 2) {
        state_ = PARSE_STATE::FINISH;
      }
      break;
    case PARSE_STATE::BODY:
      ParseBody_(line);
      break;
    default:
      break;
    }
    if (lineEnd == buff.BeginWrite()) {
      break;
    }
    buff.RetrieveUntil(lineEnd + 2);
  }
  LOG_DEBUG("[{}], [{}], [{}]", method_, path_, version_);
  return true;
}

void HTTPRequest::ParsePath_() {
  if (path_ == "/") {
    path_ = "/index.html";
  } else {
    for (auto &item : DEFAULT_HTML) {
      if (item == path_) {
        path_ += ".html";
        break;
      }
    }
  }
}

bool HTTPRequest::ParseRequestLine_(std::string_view line) {
  // Expected: METHOD SP PATH SP HTTP/VERSION
  auto s1 = line.find(' ');
  if (s1 == std::string_view::npos) {
    LOG_ERROR("RequestLine Error: missing first space");
    return false;
  }
  auto s2 = line.find(' ', s1 + 1);
  if (s2 == std::string_view::npos) {
    LOG_ERROR("RequestLine Error: missing second space");
    return false;
  }
  std::string_view m = line.substr(0, s1);
  std::string_view p = line.substr(s1 + 1, s2 - s1 - 1);
  std::string_view hv = line.substr(s2 + 1); // e.g. HTTP/1.1

  constexpr std::string_view prefix = "HTTP/";
  if (hv.size() < prefix.size() || hv.substr(0, prefix.size()) != prefix) {
    LOG_ERROR("RequestLine Error: bad HTTP prefix");
    return false;
  }
  std::string_view v = hv.substr(prefix.size());
  if (v.empty()) {
    LOG_ERROR("RequestLine Error: empty version");
    return false;
  }

  method_.assign(m.begin(), m.end());
  path_.assign(p.begin(), p.end());
  version_.assign(v.begin(), v.end());
  state_ = PARSE_STATE::HEADERS;
  return true;
}

void HTTPRequest::ParseHeader_(std::string_view line) {
  // Empty line indicates end of headers
  if (line.empty()) {
    state_ = PARSE_STATE::BODY;
    return;
  }
  auto pos = line.find(':');
  if (pos == std::string_view::npos) {
    state_ = PARSE_STATE::BODY;
    return;
  }
  // Trim key's trailing whitespace
  size_t key_end = pos;
  while (key_end > 0 &&
         std::isspace(static_cast<unsigned char>(line[key_end - 1]))) {
    --key_end;
  }
  std::string_view key_sv = line.substr(0, key_end);

  // Skip spaces after ':' to start of value
  size_t val_begin = pos + 1;
  while (val_begin < line.size() &&
         std::isspace(static_cast<unsigned char>(line[val_begin]))) {
    ++val_begin;
  }
  std::string_view value_sv = line.substr(val_begin);

  header_[std::string(key_sv)] = std::string(value_sv);
}

void HTTPRequest::ParseBody_(std::string_view line) {
  body_ = line;
  ParsePost_();
  state_ = PARSE_STATE::FINISH;
  LOG_DEBUG("Body:{}, len:{}", line, line.size());
}

int HTTPRequest::ConverHex(char ch) {
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  return ch;
}

void HTTPRequest::ParsePost_() {
  if (method_ == "POST" &&
      header_["Content-Type"] == "application/x-www-form-urlencoded") {
    ParseFromUrlencoded_();
    if (DEFAULT_HTML_TAG.count(path_)) {
      int tag = DEFAULT_HTML_TAG.find(path_)->second;
      LOG_DEBUG("Tag:%d", tag);
      if (tag == 0 || tag == 1) {
        bool isLogin = (tag == 1);
        if (UserVerify(post_["username"], post_["password"], isLogin)) {
          path_ = "/welcome.html";
        } else {
          path_ = "/error.html";
        }
      }
    }
  }
}

void HTTPRequest::ParseFromUrlencoded_() {
  if (body_.size() == 0) {
    return;
  }

  string key, value;
  int num = 0;
  int n = body_.size();
  int i = 0, j = 0;

  for (; i < n; i++) {
    char ch = body_[i];
    switch (ch) {
    case '=':
      key = body_.substr(j, i - j);
      j = i + 1;
      break;
    case '+':
      body_[i] = ' ';
      break;
    case '%':
      num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
      body_[i + 2] = num % 10 + '0';
      body_[i + 1] = num / 10 + '0';
      i += 2;
      break;
    case '&':
      value = body_.substr(j, i - j);
      j = i + 1;
      post_[key] = value;
      LOG_DEBUG("{} = {}", key.c_str(), value.c_str());
      break;
    default:
      break;
    }
  }
  assert(j <= i);
  if (post_.count(key) == 0 && j < i) {
    value = body_.substr(j, i - j);
    post_[key] = value;
  }
}

bool HTTPRequest::UserVerify(std::string_view name, std::string_view pwd,
                             bool isLogin) {
  if (name == "" || pwd == "") {
    return false;
  }
  LOG_INFO("Verify name:{} pwd:{}", name, pwd);
  auto sql = Database::SQLite::get_instance();
  if (!sql) {
    LOG_ERROR("sql uninialized");
    return false;
  }
  auto order = std::format(
      "SELECT username, password FROM user WHERE username='{}' LIMIT 1", name);

  std::string error_msg;
  auto res = sql->query(order, &error_msg);
  if (res == nullopt) {
    LOG_ERROR("query failed");
    return false;
  }

  if (isLogin) {
    if (res->rows.empty()) {
      LOG_ERROR("record not found");
      return false;
    }
    return res->rows[0][0] == name && res->rows[0][1] == pwd;
  }
  if (!res->rows.empty()) {
    LOG_ERROR("already registered");
    return false;
  }
  auto insert_flag = sql->execute(std::format(
      "INSERT INTO user(username, password) VALUES('{}','{}')", name, pwd));
  return insert_flag;
}

std::string HTTPRequest::path() const { return path_; }

std::string &HTTPRequest::path() { return path_; }
std::string HTTPRequest::method() const { return method_; }

std::string HTTPRequest::version() const { return version_; }

std::string HTTPRequest::GetPost(std::string_view key) const {
  string s(key);
  if (post_.count(s) == 1) {
    return post_.find(s)->second;
  }
  return "";
}
