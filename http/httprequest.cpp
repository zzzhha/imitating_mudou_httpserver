#include"httprequest.h"


std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index","/register","/login",
    "/welcome","/video","/picture",};
std::unordered_map<std::string,int> HttpRequest::DEFAULT_HTML_TAG{
  {"/register.html",0},{"/login.html",1} };
void HttpRequest::Init(){
  method_ = path_ = version_ =body_ ="";
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
    int consumed=readLine(buff,line);
    if(consumed == -1 ) {
      buff.read_pos_=start_pos;
      return false;
    }
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
      if(buff.readableBytes() <= 2){
        state_ = PARSE_STATE::FINISH;
      }
      break;
    case PARSE_STATE::BODY:
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
  return -1;
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
  std::regex patten("^([^:]*): ?(.*)$");
  std::smatch subMatch;
  if(regex_match(line, subMatch, patten)) {
    header_[subMatch[1]] = subMatch[2];
  }else{
    state_ = PARSE_STATE::BODY;
  }
}

void HttpRequest::ParseBody_(const std::string& line){
  body_ = line;
  ParsePost_();
  state_ = PARSE_STATE::FINISH;

  char buf[256];
  sprintf(buf,"Body:%s, len:%d", line.c_str(), line.size());
  LOGINFO(buf);
}

void HttpRequest::ParsePath_(){
  if(path_ == "/") {
    path_ = "/index.html"; 
  }else{
    for(auto &item:DEFAULT_HTML){
      if(item == path_){
        path_ += ".html";
        break;
      }
    }
  }
}
void HttpRequest::ParsePost_(){
  if(method_.find("POST")!=std::string::npos && header_["Content-Type"]=="application/x-www-form-urlencoded"){
    ParseFromUrlencoded_();
    if(DEFAULT_HTML_TAG.count(path_)){
      int tag = DEFAULT_HTML_TAG.find(path_)->second;

      char buf[128];
      sprintf(buf,"Tag:%d",tag);
      LOGINFO(buf);

      if(tag==0 || tag==1){
        bool isLogin = (tag==1);
        if(UserVerify(post_["username"], post_["password"],isLogin)){
          path_ = "/welcome.html";
        }else{
          path_ = "/error.html";
        }
      }
    }
  }
}
void HttpRequest::ParseFromUrlencoded_(){
  if(body_.size()==0) return;

  std:: string decoded_str;
  std:: string key,value;
  int num = 0;
  int n= body_.size();
  int i=0;
  int j=0;

  for(;i<n;i++){
    char ch = body_[i];
    switch(ch){
      case '=':
        key = body_.substr(j,i-j);
        j = i + 1;
        break;
      case '+':
        body_[i]= ' ';
        break;
      case '%':
        if(i+2<n){
          num = ConverHex(body_[i + 1]) * 16 +ConverHex(body_[i + 2]);
          body_[i + 2] = num % 10 + '0';
          body_[i + 1] = num / 10 + '0';
          i += 2;
        }
        break;
      case '&':
       value = body_.substr(j,i - j);
        j = i + 1;
        post_[key] = value;

        char buf[256];
        sprintf(buf,"%s = %s", key.c_str(),value.c_str());
        LOGINFO(buf);

        break;
      default:
        decoded_str+=ch;
        break;
    }
  }
  assert(j <= i);
  if(!key.empty()&& post_.count(key)== 0 && j< i){
    value = body_.substr(j, i - j);
    post_[key] = value;
  }
}

bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool isLogin){
  if(name == "" || pwd == "") { return false; }
  MYSQL* sql;
  SqlConnRAII(&sql , SqlConnPool::Instance());
  assert(sql);

  bool flag = false;
  unsigned int j=0;
  char order[256] = {0};
  MYSQL_FIELD *fields = nullptr;
  MYSQL_RES *res = nullptr;

  if(!isLogin){flag = true;}

  snprintf(order,256,"SELECT username, password FROM user WHERE username='%s' LIMIT 1",name.c_str());
  LOGINFO(order);

  if(mysql_query(sql,order)){
    mysql_free_result(res);
    return false;
  }
  res = mysql_store_result(sql);
  j = mysql_num_fields(res);
  fields = mysql_fetch_fields(res);

  while(MYSQL_ROW row = mysql_fetch_row(res)){
    char buf[256];
    sprintf(buf,"MYSQL ROW: %s %s", row[0], row[1]);
    LOGINFO(buf);

    std::string password(row[1]);
    /* 注册行为 且 用户名未被使用*/
    if(isLogin){
      if(pwd == password) {flag = true;}
      else{
        flag = false;
        LOGINFO("pwd error!");
      }
    }else{
      flag = false;
      LOGINFO("user used!");
    }
  }
  mysql_free_result(res);

  /* 注册行为 且 用户名未被使用*/
  if(!isLogin && flag == true){
    LOGINFO("register.");
    bzero(order,256);
    snprintf(order,256,"INSERT INTO user(username, password) VALUES('%s','%s')",name.c_str(),pwd.c_str());
    LOGINFO(order);
    if(mysql_query(sql,order)){
      LOGINFO("Insert error!");
      flag = false;
    }
    flag = true;
  }
  SqlConnPool::Instance()->FreeConn(sql);
  LOGINFO("UserVerify success!");
  return flag;

}

int HttpRequest::ConverHex(char ch){
  if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
  if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
  return ch;
}

