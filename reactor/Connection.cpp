#include"Connection.h"


Connection::Connection(EventLoop* loop,std::unique_ptr<Socket>clientsock)
:loop_(loop),clientsock_(std::move(clientsock)),disconnect_(false),clientchannel_(new Channel(loop_,clientsock_->fd())){
  //clientchannel_=new Channel(loop_,clientsock_->fd());   
  clientchannel_->setreadcallback (std::bind(&Connection::onmessage,this));
  clientchannel_->setclosecallback(std::bind(&Connection::closecallback,this));
  clientchannel_->seterrorcallback(std::bind(&Connection::errorcallback,this));
  clientchannel_->setwritecallback(std::bind(&Connection::writecallback,this));
  
  //clientchannel_->useet(); //设置边缘触发  
  clientchannel_->enablereading();//检测读事件
}
Connection::~Connection(){
LOGDEBUG("Connection析构函数调用");
  
}

int Connection::fd() const{
  return clientsock_->fd();
}
std::string Connection::ip()const{
  return clientsock_->ip();
} 
uint16_t Connection::port() const{
  return clientsock_->port();
}

void Connection::closecallback(){
LOGINFO("正常关闭Connection");
  disconnect_=true;
  clientchannel_->remove();
  // if(tc_fd!= -1){
  //   closetimercallback_(shared_from_this());
  // }
  closecallback_(shared_from_this());
}   
void Connection::errorcallback(){
LOGDEBUG("因错误关闭Connection");
  disconnect_=true;
  clientchannel_->remove();
  // if(tc_fd!= -1){
  //   closetimercallback_(shared_from_this());
  // }
  errorcallback_(shared_from_this());
}

//不管在任何线程中,都是调用此函数发送数据
void Connection::send(){
LOGDEBUG("调用Connection send函数");

  if(loop_->isinloopthread()){   //判断当前线程是否为事件循环线程（io线程）

LOGDEBUG("本线程是从事件线程，将处理发送事件");
    sendinloop();
  }else{
    //如果当前线程不是io线程，调用EventLoop::queueinloop(),把sendinloop()交给事件循环去执行
    //printf("send()不在事件循环的线程中\n");
LOGDEBUG("将发送数据传输给从事件循环去做");
    loop_->queueinloop(std::bind(&Connection::sendinloop,this));
  }

}

//发送数据，如果当前线程是IO线程，则直接调用此函数，如果是工作线程则把此函数传给IO线程
void Connection::sendinloop(){
LOGDEBUG("调用Connection sendinloop函数");

LOGDEBUG("唤起写事件");
  clientchannel_->enablewriting();
}
void Connection::writecallback(){
  
  //新版本
LOGDEBUG("准备发送数据");
  const size_t max_ioves =16;
  struct iovec iovs[max_ioves];
  size_t iov_count = 0;
  size_t total_bytes_to_send =0;

  iov_count =outputbuffer_.getIOVecs(iovs,max_ioves,outputbuffer_.read_pos_);
  total_bytes_to_send = outputbuffer_.readableBytes();

  if(iov_count == 0){
    clientchannel_->disablewriting();
    return;
  }

  ssize_t nwritten = ::writev(fd(),iovs,iov_count);

  if(nwritten >0){
    outputbuffer_.consumeBytes(nwritten);
    //如果发送缓冲区没有数据了，就取消写事件
    if(outputbuffer_.readableBytes() ==0){
      clientchannel_->disablewriting();
LOGDEBUG("发送数据完毕");
      sendcompletecallback_(shared_from_this());
      if(!request_.IsKeepAlive()){
        closecallback();
      }
    }
  }else if(nwritten == -1){
    if(errno ==EAGAIN || errno ==EWOULDBLOCK){
      // 正常情况：内核缓冲区已满，下次再试
      return ;
    }else{
      char buf[128];
      sprintf(buf, "writev failed, fd: %d, error: %s", fd(), strerror(errno));
      LOGERROR(buf);
      errorcallback();
    }
  }

}

void Connection::onmessage(){
  char buffer[1024];
  while (true){
    bzero(&buffer, sizeof(buffer));
    ssize_t nread = read(fd(), buffer, sizeof(buffer));
    if(nread>0){
     inputbuffer_.append(buffer,nread);
    }else if(nread==-1 && errno == EINTR){
      continue;
    }else if(nread== -1 && ((errno==EAGAIN)|| (errno == EWOULDBLOCK))){//全部数据读完
      //定时器
      updatetimercallback_(shared_from_this());
      //时间戳
      //lasttime_=Timestamp::now();
      size_t original_size =inputbuffer_.readableBytes();
      if(request_.parse(inputbuffer_)){
        response_.Init("./www",request_.path(),request_.IsKeepAlive());
        response_.MakeResponse(outputbuffer_);
        if(response_.File()!= nullptr){
          outputbuffer_.append(response_.File(),response_.FileLen());
        }else{
        }
      send();
      //使用工作线程来处理业务
      onmessagecallback_(shared_from_this()/*暂且先注释了等后面需要用到工作线程在开出来,message*/); //回调TcpServer::message()
      } else {
        // 解析失败（可能数据不完整）
        if (inputbuffer_.readableBytes() == original_size){
          // 没有消费任何数据，说明数据完全不匹配
          LOGERROR("HTTP request format error");
        } else{
          // 消费了部分数据，等待更多数据
          LOGINFO("HTTP request incomplete, waiting for more data");
        } 
      }  
      break;
    }else if(nread==0){
      closecallback();  //回调TcpServer::closecallback()
      break;
    }
  } 
}  
void Connection::setclosecallback(std::function<void(spConnection)> fn){
  closecallback_=fn;
}
  //fd_连接错误的回调函数
void Connection::seterrorcallback(std::function<void(spConnection)> fn){
  errorcallback_=fn;
}

void Connection::setonmessagecallback(std::function<void(spConnection/*暂且先注释了等后面需要用到工作线程在开出来,std::string&*/)> fn){
  onmessagecallback_=fn;
}
void Connection::setsendcompletecallback(std::function<void(spConnection)> fn){
  sendcompletecallback_=fn;
}

//时间戳
// bool Connection::timeout(time_t now,int val){
//   return now-lasttime_.toint()>val;
// }
//定时器

void Connection::setupdatetimercallback(std::function<void(spConnection)> fn){
  updatetimercallback_=fn;
}

// void Connection::setclosetimercallback(std::function<void(spConnection)>fn){
//   closetimercallback_=fn;
// }
