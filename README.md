# mymuduo

## mymuduo介绍
1.	学习陈硕老师的C++ muduo 网络库优秀的代码设计及编程方法；
2.	重写muduo核心组件，去依赖boost，用C++11重构代码，可移植性好；
3.	利用Reactor反应堆模型搭建多线程C++11网络库；
4.	基于事件驱动和事件回调的epoll+线程池面向对象编程；
5.	实现Channel、Poller、EventLoop、TcpServer、Buffer、TcpConnection等重要部分。

## 环境配置
- ubuntu18.04虚拟机配置
- win10 vscode链接虚拟机
- 安装cmake、g++

## mymuduo库的安装
```bash
chmod +x autobuild.sh
sudo ./autobuild.sh
```
## 编译测试

```bash
example文件下
sudo make
./testserver
```

## 项目架构
![image](https://user-images.githubusercontent.com/94746995/177958979-f296746a-2f01-405b-9a7a-9425650721d3.png)
![image](https://user-images.githubusercontent.com/94746995/177959004-947c0ce5-325a-4448-996b-85f91c540eb9.png)
