#pragma once

#include<unordered_map>
#include<unordered_set>
#include<string>
#include<regex>
#include<errno.h>
#include<mysql/mysql.h>
#include <sys/stat.h>
#include"../reactor/Buffer.h"
#include"../logger/log_fac.h"
#include"../mysql/sqlConnRAII.h"


class HttpRequest{
public:
  enum class PARSE_STATE{
    REQUEST_LINE,
    HEADERS,
    BODY,
    FINISH,
    ERROR
  };
  enum class LineState {
    READING,
    FOUND_CR,
  };
  enum class HTTP_CODE{
    NO_REQUEST =0,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURSE,
    FORBIDDENT_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION,
  };

  enum class METHOD{
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATCH
  };

  HttpRequest() { Init(); }
  ~HttpRequest() =default;

  void Init();
  bool parse(BufferBlock &buff);
  bool parseStartLine(std::string& line);
  int readLine(BufferBlock& buffer, std::string& line);

  std::string path() const;
  std::string& path();
  std::string method() const;
  std::string version() const;
  std::string GetPost(const std::string& key)const;
  std::string GetPost(const char* key) const;

  bool IsKeepAlive() const;

private:
  bool ParseRequestLine_(const std::string& line);
  void ParseHeader_(const std::string& line);
  void ParseBody_(const std::string& line);

  void ParsePath_();
  void ParsePost_();
  void ParseFromUrlencoded_();

  static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);
  
  PARSE_STATE state_;
  std::string method_;
  std::string url_;
  std::string path_;
  std::string version_;
  std::string host_;
  std::string body_;

  std::unordered_map<std::string,std::string> header_;
  std::unordered_map<std::string,std::string> post_;

  static std::unordered_set<std::string> DEFAULT_HTML;
  static std::unordered_map<std::string,int> DEFAULT_HTML_TAG;
  static int ConverHex(char ch);
  
};