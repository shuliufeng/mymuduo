#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>

//标识channel和epoll的状态 
//channel未添加到poller中
const int kNew = -1;//channel的成员index_ = -1
//channel已添加到poller中
const int kAdded = 1;
//channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)//构造函数 
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)//vector<epoll_event>
	//默认的长度是16 
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()//析构函数 
{
    ::close(epollfd_);
}

//epoll_wait 
//eventloop会创建一个channellist，并把创建好的channellist的地址传给poll
//poll通过epoll_wait监听到哪些fd发生了事件，把真真正正发生事件的channel通过形参发送到eventloop提供的实参里面 
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    //实际上应该用LOG_DEBUG输出日志更为合理，可以设置开启或者不开启 
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());
    //对poll的执行效率有所影响 
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    //events_.begin()返回首元素的迭代器（数组），也就是首元素的地址，是面向对象的，要解引用，就是首元素的值，然后取地址 
    //就是vector底层数组的起始地址   static_cast类型安全的转换   timeoutMs超时时间 
    int saveErrno = errno;//全局的变量errno，库里的，poll可能在多个线程eventloop被调用 ，所以有局部变量存起来 
    Timestamp now(Timestamp::now());//获取当前时间 

    if (numEvents > 0)//表示有已经发生相应事件的个数 
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())//所有的监听的event都发生事件了，得扩容了 
        {
            events_.resize(events_.size() * 2);//扩容 
        }
    }
    else if (numEvents == 0)//epoll_wait这一轮监听没有事件发生，timeout超时了 
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)//不等于外部的中断 ，是由其他错误类型引起的 
        {
            /*
            适配,把errno重置成当前loop之前发生的错误的值(其他loop可能会发生错误，将全局的errno修改了) 
            */
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
/**
 *            EventLoop  =>   poller.poll
 *     ChannelList      Poller
 *                     ChannelMap  <fd, channel*>   epollfd
 */ 
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)//未添加或者已删除 
    {
        if (index == kNew)//未添加，键值对写入map中 
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);//相当于调用epoll_ctr，添加1个channel到epoll中 
    }
    else//channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())//已经对任何事件不感兴趣，不需要poller帮忙监听了 
        {
            update(EPOLL_CTL_DEL, channel);//删除已注册的channel的感兴趣的事件 
            channel->set_index(kDeleted);//删掉 
        }
        else//包含了fd的事件，感兴趣 
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

//从poller中删除channel
void EPollPoller::removeChannel(Channel *channel) 
{
    int fd = channel->fd();
    channels_.erase(fd);//从map中删掉 

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);
    
    int index = channel->index();
    if (index == kAdded)//如果已注册过 
    {
        update(EPOLL_CTL_DEL, channel);//通过epoll_ctrl 删掉 
    }
    channel->set_index(kNew);//设置成未添加的状态 
}

//填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i=0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);//类型强转 
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);//EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了
        //至于EventLoop拿到这些channel干什么事情，我们看 EventLoop的代码 
    }
} 

//更新channel通道 就是epoll_ctl 的 add/mod/del 操作 
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);
    
    int fd = channel->fd();

    event.events = channel->events();//返回的就是fd所感兴趣的事件 
    event.data.fd = fd; 
    event.data.ptr = channel;//绑定的参数 
    
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)//把fd相关事件更改 
    {   
        //出错了 
        if (operation == EPOLL_CTL_DEL)//没有删掉，我们可以不用，影响不大，LOG_ERROR输出即可
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else//add添加或者mod更改错误，是LOG_FATAL(不可容忍的错误),这个会自动exit 
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}
