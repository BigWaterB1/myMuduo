#include "Acceptor.h"
#include "Logger.h"
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if( sockfd < 0 )
    {
        LOG_FATAL("%s:%s:%d listen socket create error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    LOG_DEBUG("Acceptor::ctor create nonblocking socket %d for listening", acceptSocket_.fd());
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reusePort);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading(); // 设置对读事件刚兴趣，并注册fd到mainloop的poller上
}

// listenfd有读事件发生了，就是有新用户连接了
void Acceptor::handleRead()
{
    LOG_DEBUG("Acceptor::handleRead");
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    LOG_DEBUG("newconnection: connfd:%d, peerAddr:%s", connfd, peerAddr.toIpPort().c_str());
    if( connfd >= 0 )
    {
        if( newConnectionCallback_ )
        {
            //轮询找到subloop，唤醒，分发当前的新客户端连接
            newConnectionCallback_(connfd, peerAddr);
        }
        else 
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if( errno == EMFILE )
        {
            LOG_ERROR("%s:%s:%d accppet reach limit \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}