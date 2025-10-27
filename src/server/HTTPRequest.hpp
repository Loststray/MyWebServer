#ifndef HTTP_REQUEST_HPP_
#define HTTP_REQUEST_HPP_

#include "buffer.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
namespace Web {

class HTTPRequest {
public:
  enum class PARSE_STATE {
    REQUEST_LINE,
    HEADERS,
    BODY,
    FINISH,
  };

  enum class HTTP_CODE {
    NO_REQUEST = 0,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURSE,
    FORBIDDENT_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION,
  };

  HTTPRequest() { init(); }
  ~HTTPRequest() = default;

  void init();
  bool parse(Buffer &buff);

  std::string path() const;
  std::string &path();
  std::string method() const;
  std::string version() const;
  std::string GetPost(std::string_view key) const;

  bool IsKeepAlive() const;

  /*
  todo
  void HttpConn::ParseFormData() {}
  void HttpConn::ParseJson() {}
  */

private:
  bool ParseRequestLine_(std::string_view line);
  void ParseHeader_(std::string_view line);
  void ParseBody_(std::string_view line);

  void ParsePath_();
  void ParsePost_();
  void ParseFromUrlencoded_();

  static bool UserVerify(std::string_view name, std::string_view pwd,
                         bool isLogin);

  PARSE_STATE state_;
  std::string method_, path_, version_, body_;
  std::unordered_map<std::string, std::string> header_;
  std::unordered_map<std::string, std::string> post_;

  static const std::unordered_set<std::string> DEFAULT_HTML;
  static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
  static int ConverHex(char ch);
};
} // namespace Web

#endif // HTTP_REQUEST_H