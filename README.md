# ddz_net_server

## 项目简介

本项目实现了关了斗地主的服务器端，涉及技术栈有：protobuf，jsoncpp，OpenSSL，mysql，redis，select，poll，epoll，Glog等。客户端使用QT编写，详情：[feibo888/ddz_net_client](https://github.com/feibo888/ddz_net_client)

## 重点介绍

1. 基于Protobuf设计并实现自定义应用层协议进行数据序列化与通信，有效解决TCP粘包问题。
2. 采用多Reactor架构，实现了一个线程池，每个线程对应一个反应堆，由主线程向各个反应堆添加任务，避免惊群问题。每个反应堆可在select,poll,epoll中切换。
3. 基于OpenSSL实现RSA密钥交换+AES数据加密的双层防护体系，避免中间人攻击。
4. 使用mysql保存用户信息，采用参数化查询，避免sql注入。使用redis缓存房间以及房间中的用户，减轻mysql压力。
5. mysql，redis配置信息保存在文件中，使用jsoncpp进行数据的读取。
6. 使用分布式锁解决加入房间和继续游戏的竞态问题
7. 集成Glog日志库，实现高性能，线程安全的日志功能，提供多种日志等级，便于快速定位bug。

## TODO

1. 实现更多功能，如聊天，显示用户头像等信息，充值功能等
2. 可以考虑使用连接池
3. 引入协程
