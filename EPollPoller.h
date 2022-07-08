#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

/**
 * epoll的使用  
 * epoll_create  创建fd, 在EPollPoller的析构和构造函数中
 * epoll_ctl     进行add/mod/del,在updateChannel和removeChannel函数中
 * epoll_wait    在poll函数中
 */ 
 
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    //重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
    //上面两个方法表示epoll_ctl的行为
private:
    static const int kInitEventListSize = 16;//初始化vector长度 

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;//epoll_wait的第二个参数 
};
