#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;

/**
 * 理清楚  EventLoop、Channel、Poller之间的关系   =》 Reactor模型上对应 Demultiplex, 采用one loop per thread
 * Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回的具体事件
 */ 
 
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;//事件回调 
    using ReadEventCallback = std::function<void(Timestamp)>;//只读事件的回调 

    Channel(EventLoop *loop, int fd);//构造函数 
    ~Channel();//析构函数 

    //fd得到poller通知以后，处理事件的
    //调用相应的回调方法来处理事件 
    void handleEvent(Timestamp receiveTime);  

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    //防止当channel被手动remove掉，channel还在执行回调操作，就是上面这些回调操作 
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }//fd所感兴趣的事件 
    int set_revents(int revt) { revents_ = revt; }//poller监听事件，设置了channel的fd相应事件 

    //设置fd相应的事件状态，要让fd对这个事件感兴趣 
    //update就是调用epoll_ctrl，通知poller把fd感兴趣的事件添加到fd上
    void enableReading() { events_ |= kReadEvent; update(); }//赋上去 用或  
    void disableReading() { events_ &= ~kReadEvent; update(); }//取反再与，去掉 
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    //返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    //one loop per thread
    EventLoop* ownerLoop() { return loop_; }//当前channel属于哪个eventloop 
    void remove();//删除channel 
private:

    void update();//更新，内部对象调用 
    void handleEventWithGuard(Timestamp receiveTime);//受保护的处理事件 
    
    //表示当前fd和其状态，是没有对任何事件感兴趣，还是对读或者写感兴趣 
    static const int kNoneEvent;//都不感兴趣 
    static const int kReadEvent;//事件 
    static const int kWriteEvent;//写事件 

    EventLoop *loop_;//事件循环
    const int fd_;//fd, Poller监听的对象
    int events_;//注册fd感兴趣的事件
    int revents_;//Poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;//绑定自己 
    bool tied_;
 
    //因为channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    //根据不同的事件，进行相应的回调操作
    //这些回调是用户设定的，通过接口传给channel来负责调用 ，channel才知道fd上是什么事件 
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
