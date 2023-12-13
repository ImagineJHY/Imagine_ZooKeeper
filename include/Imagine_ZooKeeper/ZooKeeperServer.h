#ifndef IMAGINE_ZOOKEEPER_ZOOKEEPERSERVER_H
#define IMAGINE_ZOOKEEPER_ZOOKEEPERSERVER_H

#include "Watcher.h"
#include "common_typename.h"

#include "Imagine_Muduo/Imagine_Muduo.h"

#include <unordered_map>
#include <list>

namespace Imagine_ZooKeeper
{

class ZooKeeperServer : public Imagine_Muduo::TcpServer
{
 public:
    enum ClusterType
    {
        High_Availability,
        Load_Balance,
        High_Performance
    };

 public:
    class Znode
    {
     private:
        std::string cluster_name_;                          // 用于记录所属的cluster
        std::pair<std::string, std::string> stat_;          // 暂用于记录服务器名
        std::string data_;                                  // 暂用于记录服务器信息
        std::list<std::shared_ptr<Watcher>> watcher_list_;  // 用于记录注册了Watcher的列表
        std::string watcher_stat_;                          // 告诉watcher自己的状态

     public:
        Znode(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &data);

        virtual ~Znode();

        std::string GetCluster() const;

        std::pair<std::string, std::string> GetStat() const;

        std::string GetData() const;

        Znode* SetWatcherStat(const std::string &watcher_stat);

        virtual Znode* Insert(Znode *next) = 0;

        // 将节点从集群结构中移除(不是真的delete)
        virtual Znode* Delete(Znode *aim_node) = 0;

        virtual Znode *Find(const std::pair<std::string, std::string> &stat) = 0;

        // clusternode调用,返回集群更新后选举的master节点
        virtual Znode *Update() = 0;

        const Znode* Notify() const;

        Znode* AddWatcher(std::shared_ptr<Watcher> new_watcher);
    };

    class ZnodeHA : public Znode
    {
     public:
        ZnodeHA(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &data);

        ~ZnodeHA();

        Znode *GetPre() const;

        Znode *GetNext() const;

        Znode* Insert(Znode *next);

        Znode* Delete(Znode *aim_node);

        Znode *Find(const std::pair<std::string, std::string> &stat);

        Znode *Update();

     private:
        ZnodeHA *pre_;
        ZnodeHA *next_;
    };

    class ZnodeLB : public Znode
    {
     public:
        ZnodeLB(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &data);

        ~ZnodeLB();

        Znode *GetPre() const;

        Znode *GetNext() const;

        Znode* Insert(Znode *next);

        Znode* Delete(Znode *aim_node);

        Znode *Find(const std::pair<std::string, std::string> &stat);

        Znode *Update();

     private:
        ZnodeLB *pre_;
        ZnodeLB *next_;
    };

    class ZnodeHP : public Znode
    {
     public:
        ZnodeHP(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &data);

        ~ZnodeHP();

        Znode *GetPre() const;

        Znode *GetNext() const;

        Znode* Insert(Znode *next);

        Znode* Delete(Znode *aim_node);

        Znode *Find(const std::pair<std::string, std::string> &stat);

        Znode *Update();

     private:
        ZnodeHP *pre_;
        ZnodeHP *next_;
    };

 public:
    ZooKeeperServer();

    // 使用默认的TcpConnection, 无业务逻辑, 弃用
    ZooKeeperServer(const std::string& profile_name);
    
    // 使用默认的TcpConnection, 无业务逻辑, 弃用
    ZooKeeperServer(const YAML::Node& config);

    ZooKeeperServer(const std::string& profile_name, Imagine_Muduo::Connection* msg_conn);

    ZooKeeperServer(const YAML::Node& config, Imagine_Muduo::Connection* msg_conn);

    virtual ~ZooKeeperServer();

    void Init(const std::string& profile_name);

    void Init(const YAML::Node& config);

    void LoadBalance(); // 暂未启用

    // 对外接口:节点注册
    bool InsertZnode(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const ClusterType cluster_type = Load_Balance, const std::string &watcher_stat = "", const std::string &data = "");

    // 对外接口:节点下线,并可以更新watcher_stat
    bool DeleteZnode(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &watcher_stat = "");

    // 对外接口:查找集群master节点的stat,并提供更新master节点选项
    std::pair<std::string, std::string> GetClusterZnodeStat(const std::string &cluster_name, bool update = false, std::shared_ptr<Watcher> new_watcher = nullptr);

    // 对外接口:查找集群master节点的data,并提供更新master节点选项
    std::string GetClusterZnodeData(const std::string &cluster_name, bool update = false, std::shared_ptr<Watcher> new_watcher = nullptr);

 private:
    // 以下函数均不加任何锁,安全性由接口函数保证

    // 在集群中增加节点
    bool HighAvailabilityInsert(Znode *cluster_node, const std::pair<std::string, std::string> &stat, const std::string &watcher_stat = "", const std::string &data = "");
    bool LoadBalanceInsert(Znode *cluster_node, const std::pair<std::string, std::string> &stat, const std::string &watcher_stat = "", const std::string &data = "");
    bool HighPerformanceInsert(Znode *cluster_node, const std::pair<std::string, std::string> &stat, const std::string &watcher_stat = "", const std::string &data = "");

    // 在集群中移除(不delete)节点并返回移除的节点,在调用函数中进行删除.在调用时会保证待删除节点不是master节点
    Znode *HighAvailabilityDelete(Znode *cluster_node, const std::pair<std::string, std::string> &stat);
    Znode *LoadBalanceDelete(Znode *cluster_node, const std::pair<std::string, std::string> &stat);
    Znode *HighPerformanceDelete(Znode *cluster_node, const std::pair<std::string, std::string> &stat);

    // 更新集群的master节点,并返回新的master节点
    Znode *HighAvailabilityUpdate(Znode *cluster_node) const;
    Znode *LoadBalanceUpdate(Znode *cluster_node) const;
    Znode *HighPerformanceUpdate(Znode *cluster_node) const;

    // 创造Znode节点
    Znode *CreateZnode(const std::string &cluster_name, ClusterType cluster_type, const std::pair<std::string, std::string> &stat, const std::string &data = nullptr) const;

    // 在map中增加集群
    bool CreateClusterInMap(const std::string &cluster_name, Znode *root_node, const ClusterType cluster_type);

    // 删除map中的相应集群
    bool DeleteClusterInMap(const std::string &cluster_name);

    // 更新集群master节点,返回更新结果
    Znode *UpdateClusterZnode(Znode *cluster_node, ClusterType cluster_type) const;

 protected:
    // 配置文件字段
    bool singleton_log_mode_;
    Logger* logger_;

 private:
    pthread_mutex_t map_lock_;                                       // 所有map的锁
    std::unordered_map<std::string, ClusterType> type_map_;          // 记录服务对应的集群类型
    std::unordered_map<std::string, Znode *> node_map_;              // 记录服务对应的Znode主节点
    std::unordered_map<std::string, pthread_mutex_t *> lock_map_;    // 记录服务对应的集群锁
    std::unordered_map<std::string, int> unique_map_;                // 用于保证全局唯一,避免重复注册
};

} // namespace Imagine_ZooKeeper

#endif