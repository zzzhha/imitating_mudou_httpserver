#include"httpresponse.h"

std::string UrlEncode(const std::string& utf8Str) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0'); // 十六进制输出，不足两位补0

    for (char c : utf8Str) {
        // 无需编码的字符：字母、数字、- _ . ~
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            ss << c;
        } else {
            // 其他字符（包括中文的 UTF-8 字节）转换为 %XX
            ss << "%" << std::setw(2) << std::uppercase << (static_cast<unsigned int>(static_cast<unsigned char>(c)));
        }
    }
    return ss.str();
}

const std::unordered_map<std::string,std::string> HttpResponse::SUFFIX_TYPE={
  {".html","text/html"},
  {".xml","text/xml"},
  {".xhtml","application/xhtml+xml"},
  {".txt","text/plain"},
  {".rtf","application/rtf"},
  {".pdf","application/pdf"},
  {".word","application/nsword"},
  {".png","image/png"},
  {".gif","image/gif"},
  {".jpg","image/jpg"},
  {".jpeg","image/jpeg"},
  {".au","audio/basic"},
  {".mpeg","video/mpeg"},
  {".mpg","video/mpeg"},
  {".avi","video/x-msvideo"},
  {".gz","application/x-gzip"},
  {".tar","application/x-tar"},
  {".css","text/css"},
  {".js","application/javascript"},
};

const std::unordered_map<int,std::string>HttpResponse::CODE_STATUS={
  {200,"OK"},
  {400,"Bad Request"},
  {403,"Forbindden"},
  {404,"Not Found"},
};

const std::unordered_map<int,std::string> HttpResponse::CODE_PATH={
  {400,"/400.html"},
  {403,"/403.html"},
  {404,"/404.html"},
};

HttpResponse::HttpResponse(){
  code_ = -1;
  path_ = srcDir_ ="";
  isKeepAlive_ = false;
  mmFile_ = nullptr;
  mmFileStat_ = {0};
};

HttpResponse::~HttpResponse(){
  UnmapFile();
}

void HttpResponse::Init(const std::string& srcDir,std::string& path,bool isKeepAlive, bool is_js ,bool is_js_success ,bool is_download,int code){
  assert(srcDir!="");
  if(mmFile_){UnmapFile();}
  code_ = code;
  path_ =path;
  srcDir_ = srcDir;
  mmFile_ = nullptr;
  mmFileStat_ ={0};
  isKeepAlive_=isKeepAlive;
  is_js_ = is_js;
  is_js_success_ = is_js_success;
  is_download_=is_download;
};

void HttpResponse::MakeResponse(BufferBlock& buf){
  //判断请求的资源文件
  if(stat((srcDir_+path_).data(),&mmFileStat_)<0 || S_ISDIR(mmFileStat_.st_mode)){
    code_ = 404;
  }else if(!(mmFileStat_.st_mode & S_IROTH)){
    code_ = 403;
  }else if(code_ == -1){
    code_ = 200;
  }
  ErrorHtml_();
  AddStateLine_(buf);
  AddHeader_(buf);
  AddContent_(buf);
}


char* HttpResponse::File() {
  return mmFile_;
}

size_t HttpResponse::FileLen() const {
  return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_(){
  //code为400,403,404,则
  if(CODE_PATH.count(code_)==1){
    //将路径指向错误页面的路径
    path_ = CODE_PATH.find(code_)->second;
    //改变文件结构体指向的内容
    stat((srcDir_ + path_).data(),&mmFileStat_);
  }
}

void HttpResponse::AddStateLine_(BufferBlock& buf){
  std::string status;
  //code状态支持200,400,403,404
  if(CODE_STATUS.count(code_) == 1){
    status = CODE_STATUS.find(code_)->second;
  }else{
    code_ = 400;
    status = CODE_STATUS.find(400)->second;
  }
  std::string str("HTTP/1.1 "+std::to_string(code_)+" "+status + "\r\n");
  buf.append(str);
}

void HttpResponse::AddHeader_(BufferBlock& buf){
  buf.append("Connection: ",strlen("Connection: "));
  if(isKeepAlive_){
    buf.append("keep-alive\r\n",strlen("keep-alive\r\n"));
    //buf.append("keep-alive: max=6, timeout=120\r\n",sizeof("keep-alive: max=6, timeout=120\r\n"));
  }else{
    buf.append("close\r\n",strlen("close\r\n"));
  }
  if (is_js_) {
LOGDEBUG("接口请求强制返回JSON");
    std::string jsonBody;
    if (is_js_success_) {
        jsonBody = "{\"success\": true, \"message\": \"登录成功\"}";
      } else {
        jsonBody = "{\"success\": false, \"message\": \"用户名或密码错误\"}";
      }
    buf.append("Content-Type: application/json; charset=utf-8\r\n",strlen("Content-Type: application/json; charset=utf-8\r\n"));
    buf.append(("Content-Length: " + std::to_string(jsonBody.size()) + "\r\n").c_str(),strlen(("Content-Length: " + std::to_string(jsonBody.size()) + "\r\n").c_str()));
    buf.append("\r\n", strlen("\r\n"));
  } else {
    // 静态资源使用原有逻辑（如text/html）
  LOGDEBUG("静态资源使用原有逻辑");
    if(is_download_){
      std::string encodedFileName = UrlEncode(path_);
      std::string Content_Disposition="Content_Disposition: attachment; filename*=UTF-8''" + encodedFileName;
      buf.append(Content_Disposition);
      buf.append("/r/n",strlen("/r/n"));
    }
    std::string str("Content-type: "+GetFileType_()+"\r\n");
    buf.append(str);
  }
}

void HttpResponse::AddContent_(BufferBlock &buf){
  if (is_js_){
    if(is_js_success_){
LOGDEBUG("准备发送成功js信息");
      std::string jsonBody = "{\"success\": true, \"message\": \"登录成功\"}";
      buf.append(jsonBody);
    }else{
LOGDEBUG("准备发送失败js信息");
      std::string jsonBody = "{\"success\": false, \"message\": \"用户名或密码错误\"}";
      buf.append(jsonBody);
    }
  }else{
LOGDEBUG("发送文件信息");
    int srcFd = open((srcDir_ + path_).data(),O_RDONLY);
    if(srcFd < 0){
      ErrorContent(buf,"FileFound!");
      return;
    }
    /* 将文件映射到内存提高文件的访问速度 
    MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    char buff[256];
    sprintf(buff,"file path %s", (srcDir_ + path_).data());
    LOGINFO(buff);

    void* mmRet = mmap(0,mmFileStat_.st_size,PROT_READ,MAP_PRIVATE,srcFd,0);
    if(mmRet == MAP_FAILED){
      ErrorContent(buf, "File NotFound!");
      return;
    }
    mmFile_ =static_cast<char*>(mmRet);
    close(srcFd);
    std::string str("Content-length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");
    buf.append(str);
  }
  
}

void HttpResponse::UnmapFile(){
  if(mmFile_){
    munmap(mmFile_,mmFileStat_.st_size);
    mmFile_=nullptr;
  }
}

std::string HttpResponse::GetFileType_(){
  std::string path_without_query = path_;
  size_t query_pos = path_without_query.find('?');
  if (query_pos != std::string::npos){
    path_without_query = path_without_query.substr(0, query_pos);
  }

  std::string::size_type idx = path_without_query.find_last_of('.');
  if(idx==std::string::npos){
    return "text/plain";
  }
  std::string suffix = path_without_query.substr(idx);
  if(SUFFIX_TYPE.count(suffix)==1){
    return SUFFIX_TYPE.find(suffix)->second;
  }
  return "text/plain";
}

void HttpResponse::ErrorContent(BufferBlock &buf,std::string message){
  std::string body;
  std::string status;
  body += "<html><title>Error</title>";
  body += "<body bgcolor=\"ffffff\">";
  if(CODE_STATUS.count(code_) ==1){
    status = CODE_STATUS.find(code_)->second;
  }else{
    status = "Bad Request";
  }
  body += std::to_string(code_) + " : "+status+"\n";
  body += "<p>" + message +"</p>";
  body += "<hr><em>MyWebServer</em></body></html>";

  std::string str("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
  buf.append(str);
  buf.append(body);
}