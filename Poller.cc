#include "Poller.h"
#include "Channel.h"

//将Poller所属的EventLoop记录下来即可！
Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

bool Poller::hasChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}
