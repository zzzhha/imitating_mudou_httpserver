#include"HttpServer.h"

HttpServer::HttpServer(const std::string &ip,uint16_t port,int timeoutS,bool OptLinger,
int sqlPort,const char*sqlUser,const char*sqlPwd,const char*dbName,
int subthreadnum,int workthreadnum,int connpoolnum,const std::string&static_path)
      :tcpserver_(ip,port,subthreadnum,timeoutS,OptLinger),threadpool_(workthreadnum,"WORKS"),static_path_(static_path)
{
  // 以下代码不是必须的，业务关心什么事件，就指定相应的回调函数。
  tcpserver_.setnewconnection(std::bind(&HttpServer::HandleNewConnection, this, std::placeholders::_1));
  tcpserver_.setcloseconnection(std::bind(&HttpServer::HandleClose, this, std::placeholders::_1));
  tcpserver_.seterrorconnection(std::bind(&HttpServer::HandleError, this, std::placeholders::_1));
  tcpserver_.setonmessage(std::bind(&HttpServer::HandleMessage, this, std::placeholders::_1/*, std::placeholders::_2*/));
  tcpserver_.setsendcomplete(std::bind(&HttpServer::HandleSendComplete, this, std::placeholders::_1));
  //tcpserver_.settimeout(std::bind(&HttpServer::HandleTimeOut, this, std::placeholders::_1));
LOGINFO("尝试连接数据库");
  SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connpoolnum);
LOGINFO("数据库连接成功");
}
HttpServer::~HttpServer(){

}
void HttpServer::start(){
  LOGINFO("Http服务器启动");
  tcpserver_.start();
}

void HttpServer::Stop(){
  LOGINFO("Http服务器关闭");
  SqlConnPool::Instance()->ClosePool();
  //停止工作线程
  threadpool_.stop();
  //停止IO线程
  tcpserver_.stop();
}
void HttpServer::HandleNewConnection(spConnection conn){
  //std::cout<<"New Connection Come in."<<std::endl;
  char buf[128];
  sprintf (buf,"new connection(fd=%d,ip=%s,port=%d) ok.",conn->fd(),conn->ip().c_str(),conn->port());
  LOGINFO(buf);
  //根据业务需求,在这里增加其他代码
}
void HttpServer::HandleClose(spConnection conn){
  // std::cout<<"EchoServer conn closed."<<std::endl;
  char buf[128];
  sprintf (buf,"connection close(fd=%d,ip=%s,port=%d).",conn->fd(),conn->ip().c_str(),conn->port());
  LOGINFO(buf);
  //根据业务需求,在这里增加其他代码
}
void HttpServer::HandleError(spConnection conn){
  char buf[128];
  sprintf(buf, "connection error(fd=%d,ip=%s,port=%d).", conn->fd(), conn->ip().c_str(), conn->port());
  LOGERROR(buf);
  //根据业务需求,在这里增加其他代码
}
void HttpServer::HandleMessage(spConnection conn/*暂且先注释了等后面需要用到工作线程在开出来,string& buffer*/){
  char buf[128];
  sprintf(buf, "处理了(fd=%d,ip=%s,port=%d)的数据.", conn->fd(), conn->ip().c_str(), conn->port());
  LOGINFO(buf);
  // if(threadpool_.size()==0){
  //    ProcessHttpRequest(conn, buffer);
  // }
  // else{
  //   //把业务添加到线程池的任务队列中
  //   size_t readable = buffer.readableBytes();
  //   if(readable ==0) return;

  //   //创建数据副本
  //   std::string data=buffer.bufferToString();
  //   buffer.consumeBytes(buffer.readableBytes());
  //   // std::unique_ptr<std::string> message(new std::string());
  //   // message->resize(readable);
  //   // buffer.readBytes(&(*message)[0],readable);
  //   std::weak_ptr<Connection> weak_conn = conn;
    
  //     threadpool_.addtask([this,weak_conn,data=std::move(data)](){
  //     BufferBlock temp_block;
  //     temp_block.append(data.data(),data.size());

  //     if(auto shared_conn = weak_conn.lock()){
  //     this->ProcessHttpRequest(shared_conn,temp_block);
  //     }
  //   });
  // }
}

// void HttpServer::ProcessHttpRequest(spConnection conn,BufferBlock &buffer){
//   HttpRequest request;

//   size_t original_size = buffer.readableBytes();

//   if(request.parse(buffer)){
//     HttpResponse response;

//     bool keep_alive = request.IsKeepAlive();
// LOGDEBUG("keepalive: "+std::to_string(keep_alive));
//     response.Init(static_path_,request.path(),keep_alive);

//     BufferBlock output_buffer;
//     response.MakeResponse(output_buffer);
// LOGDEBUG("send数据");
//     conn->send(output_buffer.peek(),output_buffer.readableBytes());
//     if(response.File()!= nullptr){
//       conn->send(response.File(),response.FileLen());
//     } 
// LOGDEBUG("消费临时output_buffer数据");
//     output_buffer.consumeBytes(output_buffer.readableBytes());
//     char log_buf[256];
//     sprintf(log_buf, "HTTP %s %s - %d (processed %zu bytes)", 
//             request.method().c_str(), 
//             request.path().c_str(), 
//             response.Code(),
//             original_size - buffer.readableBytes()); // 消费的字节数
//     LOGINFO(log_buf);
//     if(!keep_alive){
// LOGDEBUG("Connection: close requested, scheduling connection close");
      
//     }
//   }
//   else {
//     // 解析失败（可能数据不完整）
//     if (buffer.readableBytes() == original_size){
//       // 没有消费任何数据，说明数据完全不匹配
//       LOGERROR("HTTP request format error");
//     } else{
//       // 消费了部分数据，等待更多数据
//       LOGINFO("HTTP request incomplete, waiting for more data");
//     }
//   }
//   //conn->send(message.data(),message.size());
// }
void HttpServer::HandleSendComplete(spConnection conn){

  LOGINFO("Message send complete.");

  //根据业务需求,在这里增加其他代码
}
/*
void HttpServer::HandleTimeOut(EventLoop*loop){
  std::cout<<"EchoServer timeout."<<std::endl;

  //根据业务需求,在这里增加其他代码
}   
*/