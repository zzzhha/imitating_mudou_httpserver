#pragma once
#include"tcpserver.h"
#include"Eventloop.h"
#include"Connection.h"
#include"ThreadPool.h"
#include"../logger/log_fac.h"
#include"Buffer.h"
/*
  HttpServer类: Http服务器
  这是一个业务类，不想文件中的其他9个类是底层类，他根据业务端改变而改变，并非必须
  下边的handle函数是使用TcpServer的接口，如果当前业务不关系则可以直接删去
*/
class HttpServer{
private:
  TcpServer tcpserver_;
  ThreadPool threadpool_;   //工作线程池
  std::string static_path_; //静态资源路径

public:
  HttpServer(const std::string &ip,uint16_t port,int timeoutMS,bool OptLinger=true,
int sqlPort=3306,const char*sqlUser="webuser",const char*sqlPwd="1258977",const char*dbName="webserver",
int subthreadnum=6,int workthreadnum=3,int connpoolnum=12,const std::string&static_path="../www");
  ~HttpServer();

  void start(); //启动业务
  void Stop();  //停止服务
  void HandleNewConnection(spConnection conn); //处理新客户端的连接请求,在Acceptor类回调此函数
  void HandleClose(spConnection conn); //关闭客户端的连接，在Connection类中回调此函数
  void HandleError(spConnection conn); //客户端的连接错误，在Connection类中回调此函数
  void HandleMessage(spConnection conn/*暂且先注释了等后面需要用到工作线程在开出来,std::string&buffer*/);  //处理客户端的请求报文，在Connection类回调此函数
  void HandleSendComplete(spConnection conn);     //数据发送完成后，在Connection类中回调此函数
  //void HandleTimeOut(EventLoop*loop);      //epoll_wait()超时，在EventLoop类中回调此函数

private:
  void ProcessHttpRequest(spConnection conn,BufferBlock &buffer);
};