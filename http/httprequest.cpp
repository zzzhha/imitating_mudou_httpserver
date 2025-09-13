#include"httprequest.h"


std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index","/register","/login",
    "/welcome","/video","/picture",};
std::unordered_map<std::string,int> HttpRequest::DEFAULT_HTML_TAG{
  {"/register.html",0},{"/login.html",1} };
void HttpRequest::Init(){
  method_ = path_ = version_ =body_ ="";
  return_js_=false;
  is_success_js_=false;
  is_download_=false;
  state_ = PARSE_STATE::REQUEST_LINE;
  header_.clear();
  post_.clear();
}

bool HttpRequest::parse(BufferBlock &buff){
  if(buff.readableBytes() <=0) {
    return false;
  }
  size_t start_pos = buff.read_pos_;
  while(buff.readableBytes() && state_ != PARSE_STATE::FINISH){
    std::string line;
    int consumed=0;
    if(state_ != PARSE_STATE::BODY) consumed=readLine(buff,line);
    else{
      line = buff.bufferToString();
      consumed=line.size();
    }
    if(consumed == 0) {
      buff.read_pos_=start_pos;
      return false;
    }
LOGDEBUG("state: "+std::to_string((int)state_));
    switch (state_)
    {
    case PARSE_STATE::REQUEST_LINE:
      if(!ParseRequestLine_(line)){
        return false;
      }
      ParsePath_();
      break;
    case PARSE_STATE::HEADERS:
      ParseHeader_(line);
//LOGDEBUG(buff.bufferToString());
      if(buff.readableBytes() <= 2){
LOGDEBUG("结束解析");
        state_ = PARSE_STATE::FINISH;

      }
      break;
    case PARSE_STATE::BODY:
LOGDEBUG("解析body");
LOGDEBUG(line);
      ParseBody_(line);
      break;
    default:
      break;
    }
  buff.consumeBytes(consumed);
  }
  char buf[256];
  sprintf(buf,"[%s], [%s], [%s]",method_.c_str(),path_.c_str(),version_.c_str());
  return true;
}
bool HttpRequest::parseStartLine(std::string& line){
  if(!line.empty()){
    std::istringstream iss(line);
    iss >> method_ >> path_ >> version_;
    state_ = PARSE_STATE::HEADERS;
    return true;
  }
  return false;
}

int HttpRequest::readLine(BufferBlock& buffer, std::string& line){
  line.clear();
  LineState state = LineState::READING;
  size_t bytesConsumed = 0;
  for(size_t i = 0;i<buffer.blocks_.size();i++){
    const auto& block = buffer.blocks_[i];
    size_t start_pos = (i==0)? buffer.read_pos_ : 0;
    size_t block_remaining = block.size - start_pos;
    const char* data = static_cast<const char*>(block.data) + start_pos;

    for(size_t j=0;j<block_remaining;j++){
      char c =data[j];
      bytesConsumed++;

      switch (state)
      {
      case LineState::READING:
        if(c=='\r'){
          state = LineState::FOUND_CR;
        }else{
          line +=c;
        }
        break;
      case LineState::FOUND_CR:
        if(c=='\n'){
          return bytesConsumed;
        }else{
          line+=c;
          state = LineState::READING;
        }
        break;
      }
    }
  }
  return 0;
}

std::string HttpRequest::path() const{
  return path_;
}
std::string& HttpRequest::path(){
  return path_;
}
std::string HttpRequest::method() const{
  return method_;
}
std::string HttpRequest::version() const{
  return version_;
}
std::string HttpRequest::GetPost(const std::string& key)const{
   assert(key != "");
    if(post_.count(key) == 1) {
      return post_.find(key)->second;
    }
    return "";
}
std::string HttpRequest::GetPost(const char* key) const{
  assert(key != nullptr);
  if(post_.count(key) == 1) {
    return post_.find(key)->second;
  }
  return "";
}
bool HttpRequest::IsKeepAlive() const {
    // 首先检查明确的Connection头部
    if (header_.count("Connection") == 1) {
        std::string connection = header_.find("Connection")->second;
        
        // 转换为小写进行比较（处理大小写不一致的情况）
        std::transform(connection.begin(), connection.end(), connection.begin(), 
                      [](unsigned char c){ return std::tolower(c); });
        
        if (connection == "close") {
            return false;  // 明确要求关闭连接
        } else if (connection == "keep-alive") {
            return true;   // 明确要求保持连接
        }
    }
    
    // 根据HTTP版本决定默认行为
    // HTTP/1.1 默认保持连接，HTTP/1.0 默认关闭连接
    return version_ == "1.1";
}
bool HttpRequest::IsReturnJs() const{
  return return_js_;
}
bool HttpRequest::IsSuccessJs() const{
  return is_success_js_;
}
bool HttpRequest::IsDownload() const{
  return is_download_;
}
bool HttpRequest::ParseRequestLine_(const std::string& line){
  std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
  std::smatch subMatch;
  if(regex_match(line,subMatch,patten)){
    method_ = subMatch[1];
    path_ = subMatch[2];
    version_ = subMatch[3];
    state_ = PARSE_STATE::HEADERS;
    return true;
  }
  LOGERROR("Requestine Error");
  return false;
}
void HttpRequest::ParseHeader_(const std::string& line){
  std::regex patten(R"(^([a-zA-Z0-9-]+):\s*(.*)$)");
  std::smatch subMatch;
  if(regex_match(line, subMatch, patten)) {
    header_[subMatch[1]] = subMatch[2];
  }else{
LOGDEBUG("state转化为body");
    state_ = PARSE_STATE::BODY;
  }
}

void HttpRequest::ParseBody_(const std::string& line){
  body_ = line;
LOGDEBUG("解析body");
LOGDEBUG(path_+" "+"Content-Type "+header_["Content-Type"]);
  if(path_.find("/download")!=std::string::npos && header_["Content-Type"]=="application/json"){
LOGDEBUG("解析下载");
    ParseDownload_();
  }else{
LOGDEBUG("解析登录或注册");
    ParsePost_();
    
  }
  state_ = PARSE_STATE::FINISH;

  char buf[256];
  sprintf(buf,"Body:%s, len:%d", line.c_str(), line.size());
  LOGINFO(buf);
}

void HttpRequest::ParsePath_(){
LOGINFO("path_ "+path_+" "+std::to_string(path_.size()));
  if(path_ == "/") {
    path_ = "/welcome.html"; 
  }else{
    for(auto &item:DEFAULT_HTML){
      if(item == path_){
        path_ += ".html";
        break;
      }
    }
  }
}

void HttpRequest::ParseDownload_(){
  rapidjson::Document doc;
LOGDEBUG("分析下载body");
  doc.Parse(body_.c_str());
  if(doc.IsObject()){
    if (doc.HasMember("fileName") && doc["fileName"].IsString()) {
      post_["fileName"] = doc["fileName"].GetString();
LOGDEBUG("fliename:");
LOGDEBUG(doc["fileName"].GetString());
    }
    if (doc.HasMember("folder") && doc["folder"].IsString()) {
      post_["folder"] = doc["folder"].GetString();  // 拿到images或video
LOGDEBUG("folder:");
LOGDEBUG(doc["folder"].GetString());
    }
  }else{
    LOGERROR("分析失败");
  }
}

void HttpRequest::ParsePost_(){
  if(method_.find("POST")!=std::string::npos){
    if(header_["Content-Type"]=="application/x-www-form-urlencoded"){
LOGDEBUG("解析url命令");
      ParseFromUrlencoded_();
      if(DEFAULT_HTML_TAG.count(path_)){
        int tag = DEFAULT_HTML_TAG.find(path_)->second;

        char buf[128];
        sprintf(buf,"Tag:%d",tag);
        LOGINFO(buf);

        if(tag==0 || tag==1){
          bool isLogin = (tag==1);
          return_js_=true;
          if(isLogin){
            HandleUserCommand("login",post_["username"],post_["password"]);
          }else{
            HandleUserCommand("register",post_["username"],post_["password"]);
          }
          // if(UserVerify(post_["username"], post_["password"],isLogin)){
          //   path_ = "/index.html";
          // }else{
          //   path_ = "/error.html";
          // }
        }
      }
    }
  }
}

std::string urlDecode(const std::string& str) {
    std::string result;
    char ch;
    int i, len = str.length();
    unsigned int hex;

    for (i = 0; i < len; ++i) {
        if (str[i] == '+') {
            result += ' '; // +号解码为空格
        } else if (str[i] == '%' && i + 2 < len) {
            // 解析%XX格式的十六进制编码
            std::string hexStr = str.substr(i + 1, 2);
            if (sscanf(hexStr.c_str(), "%x", &hex) == 1) {
                ch = static_cast<char>(hex);
                result += ch;
                i += 2; // 跳过已解析的两个字符
            } else {
                result += '%'; // 解析失败，保留原字符
            }
        } else {
            result += str[i]; // 普通字符直接保留
        }
    }
    return result;
}
void HttpRequest::ParseFromUrlencoded_() {
    if (body_.empty()) return;

    post_.clear(); // 清空原有数据
    std::string key, value;
    size_t start = 0;
    size_t len = body_.size();

    while (start < len) {
        // 1. 查找键值对分隔符&
        size_t ampersand = body_.find('&', start);
        size_t end = (ampersand == std::string::npos) ? len : ampersand;

        // 2. 查找键值分隔符=
        size_t equal = body_.find('=', start);
        if (equal == std::string::npos || equal > end) {
            // 没有=号的无效字段，跳过（如"username"而非"username=xxx"）
            start = end + 1;
            continue;
        }

        // 3. 提取key和value并解码
        key = body_.substr(start, equal - start);
        value = body_.substr(equal + 1, end - equal - 1);
        
        // 处理URL编码（如%20->空格，+->空格）
        key = urlDecode(key);
        value = urlDecode(value);

        // 4. 存入post_映射（支持login.html的username和password字段）
        post_[key] = value;

        // 5. 移动到下一个键值对
        start = end + 1;
    }
}

void HttpRequest::HandleUserCommand(const std::string& cmd, const std::string& username, const std::string& password) {

  if (cmd == "register") {
    bool ret = UserDao::Register(username, password);
    if (ret) {
      is_success_js_=true;
      LOGINFO("登录成功");
    } else {
       is_success_js_=false;
       //path_ = "/index.html";
    }
  } else if (cmd == "login") {
    bool ret = UserDao::Login(username, password);
    if (ret) {
      is_success_js_=true;
    } else {
      is_success_js_=false;
      //path_ = "/error.html";
    }
  }
}
// void HttpRequest::ParseFromUrlencoded_(){
//   if(body_.size()==0) return;

//   //std:: string decoded_str;
//   std:: string key,value;
//   int num = 0;
//   int n= body_.size();
//   int i=0;
//   int j=0;

//   for(;i<n;i++){
//     char ch = body_[i];
//     switch(ch){
//       case '=':
//         key = body_.substr(j,i-j);
//         j = i + 1;
//         break;
//       case '+':
//         body_[i]= ' ';
//         break;
//       case '%':
//         if(i+2<n){
//           num = ConverHex(body_[i + 1]) * 16 +ConverHex(body_[i + 2]);
//           body_[i + 2] = num % 10 + '0';
//           body_[i + 1] = num / 10 + '0';
//           i += 2;
//         }
//         break;
//       case '&':
//        value = body_.substr(j,i - j);
//         j = i + 1;
//         post_[key] = value;

//         char buf[256];
//         sprintf(buf,"%s = %s", key.c_str(),value.c_str());
//         LOGINFO(buf);

//         break;
//       default:
//        // decoded_str+=ch;
//         break;
//     }
//   }
//   assert(j <= i);
//   if(!key.empty()&& post_.count(key)== 0 && j< i){
//     value = body_.substr(j, i - j);
//     post_[key] = value;
//   }
// }


// bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool isLogin){
//   if(name == "" || pwd == "") { return false; }
//   MYSQL* sql;
//   SqlConnRAII(&sql , SqlConnPool::Instance());
//   assert(sql);

//   bool flag = false;
//   unsigned int j=0;
//   char order[256] = {0};
//   MYSQL_FIELD *fields = nullptr;
//   MYSQL_RES *res = nullptr;

//   if(!isLogin){flag = true;}

//   snprintf(order,256,"SELECT username, password FROM user WHERE username='%s' LIMIT 1",name.c_str());
//   LOGINFO(order);

//   if(mysql_query(sql,order)){
//     mysql_free_result(res);
//     return false;
//   }
//   res = mysql_store_result(sql);
//   j = mysql_num_fields(res);
//   fields = mysql_fetch_fields(res);

//   while(MYSQL_ROW row = mysql_fetch_row(res)){
//     char buf[256];
//     sprintf(buf,"MYSQL ROW: %s %s", row[0], row[1]);
//     LOGINFO(buf);

//     std::string password(row[1]);
//     /* 注册行为 且 用户名未被使用*/
//     if(isLogin){
//       if(pwd == password) {flag = true;}
//       else{
//         flag = false;
//         LOGINFO("pwd error!");
//       }
//     }else{
//       flag = false;
//       LOGINFO("user used!");
//     }
//   }
//   mysql_free_result(res);

//   /* 注册行为 且 用户名未被使用*/
//   if(!isLogin && flag == true){
//     LOGINFO("register.");
//     bzero(order,256);
//     snprintf(order,256,"INSERT INTO user(username, password) VALUES('%s','%s')",name.c_str(),pwd.c_str());
//     LOGINFO(order);
//     if(mysql_query(sql,order)){
//       LOGINFO("Insert error!");
//       flag = false;
//     }
//     flag = true;
//   }
//   SqlConnPool::Instance()->FreeConn(sql);
//   LOGINFO("UserVerify success!");
//   return flag;

// }

// int HttpRequest::ConverHex(char ch){
//   if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
//   if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
//   return ch;
// }

