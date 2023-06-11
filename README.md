# Readme

## 简介

Imagine_ZooKeeper是在社区ZooKeeper实现思路上，重新设计了数据模型的框架，当前主要功能有：

- 集群节点注册与删除
- Watcher机制
- 负载均衡
- 避免重复注册

注：上述功能模块目前都只配套给负载均衡集群

与ZooKeeper的差异性有：

- 数据模型的完全不同

## 快速上手

添加头文件ZooKeeper.h，使用namespace Imagine_ZooKeeper并继承class ZooKeeper即可快速使用

- 操作系统：Linux
- 依赖：
  - Imagine_Muduo
  - Linux下线程库pthread

- 启动：调用EventLoop::loop()函数即可启动服务器

## 说明

- 节点的增删改查

  ```cpp
  //向集群注册节点(也可以新建集群)
  bool InsertZnode(const std::string& cluster_name_, const std::string& stat_, const ClusterType cluster_type_=Load_Balance, const std::string& watcher_stat_="",const std::string& data_="");
  /*
  -参数
  	-cluster_name_:所属的集群名
  	-stat_:节点的stat_字段,作为节点的全局唯一标识
  	-cluster_type_:集群类型,不同集群有不同逻辑(当前只支持负载均衡集群)
  		-ZooKeeper::ClusterType::High_Availability:代表HA集群
  		-ZooKeeper::ClusterType::Load_Balance:代表LB集群
  		-ZooKeeper::ClusterType::High_Performance:代表HP集群
  	-watcher_stat_:设置当前节点的状态(watcher机制用)
  	-data_:节点的data_字段
  -返回值
  	-true:成功
  	-false:失败
  */
  
  //删除集群节点(若是唯一节点则删除集群)
  bool DeleteZnode(const std::string& cluster_name_, const std::string& stat_, const std::string& watcher_stat_="");
  /*
  -参数
  	-cluster_name_:所属的集群名
  	-stat_:节点的stat_字段,作为节点的全局唯一标识
  	-watcher_stat_:设置当前节点的状态(watcher机制用)
  -返回值
  	-true:成功
  	-false:失败
  */
  
  //获取集群master节点的stat
  std::string GetClusterZnodeStat(const std::string& cluster_name_, bool update_=false, std::shared_ptr<Watcher> new_watcher_=nullptr);
  /*
  -参数
  	-cluster_name_:所属的集群名
  	-update_:是否更新集群master节点(负载均衡用)
  	-new_watcher_:若有,则为master节点设置watcher
  -返回值:
  	-集群master节点的stat内容
  */
  
  //获取集群master节点的data
  std::string GetClusterZnodeData(const std::string& cluster_name_, bool update_=false, std::shared_ptr<Watcher> new_watcher_=nullptr);
  /*
  -参数
  	-cluster_name:所属的集群名
  	-update:是否更新集群master节点(负载均衡用)
      -new_watcher_:若有,则为master节点设置watcher
  -返回值:
  	-集群master节点的data内容
  */
  ```

  