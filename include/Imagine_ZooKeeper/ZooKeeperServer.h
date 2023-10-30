#ifndef IMAGINE_ZOOKEEPER_ZOOKEEPERSERVER_H
#define IMAGINE_ZOOKEEPER_ZOOKEEPERSERVER_H

#include "Imagine_Log/Logger.h"
#include "Imagine_Muduo/EventLoop.h"
#include "Imagine_Muduo/TcpServer.h"
#include "Watcher.h"

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
        std::string cluster_name_; // 用于记录所属的cluster
        std::string stat_;         // 暂用于记录服务器名
        std::string data_;         // 暂用于记录服务器信息
        std::list<std::shared_ptr<Watcher>> watcher_list_;
        std::string watcher_stat_; // 告诉watcher自己的状态

     public:
        Znode(const std::string &cluster_name, const std::string &stat, const std::string &data);

        virtual ~Znode();

        std::string GetCluster();

        std::string GetStat();

        std::string GetData();

        bool SetWatcherStat(const std::string &watcher_stat);

        virtual bool Insert(Znode *next) = 0;

        // 将节点从集群结构中移除(不是真的delete)
        virtual bool Delete(Znode *aim_node) = 0;

        virtual Znode *Find(const std::string &stat) = 0;

        // clusternode调用,返回集群更新后选举的master节点
        virtual Znode *Update() = 0;

        void Notify();

        bool AddWatcher(std::shared_ptr<Watcher> new_watcher);
    };

    class ZnodeHA : public Znode
    {
     public:
        ZnodeHA(const std::string &cluster_name, const std::string &stat, const std::string &data);

        ~ZnodeHA();

        Znode *GetPre();

        Znode *GetNext();

        // bool SetPre(Znode* pre);

        // bool SetNext(Znode* next);

        bool Insert(Znode *next);

        bool Delete(Znode *aim_node);

        Znode *Find(const std::string &stat);

        Znode *Update();

     private:
        ZnodeHA *pre_;
        ZnodeHA *next_;
    };

    class ZnodeLB : public Znode
    {
     public:
        ZnodeLB(const std::string &cluster_name, const std::string &stat, const std::string &data);

        ~ZnodeLB();

        Znode *GetPre();

        Znode *GetNext();

        // bool SetPre(Znode* pre);

        // bool SetNext(Znode* next);

        bool Insert(Znode *next);

        bool Delete(Znode *aim_node);

        Znode *Find(const std::string &stat);

        Znode *Update();

     private:
        ZnodeLB *pre_;
        ZnodeLB *next_;
    };

    class ZnodeHP : public Znode
    {
     public:
        ZnodeHP(const std::string &cluster_name, const std::string &stat, const std::string &data);

        ~ZnodeHP();

        Znode *GetPre();

        Znode *GetNext();

        // bool SetPre(Znode* pre);

        // bool SetNext(Znode* next);

        bool Insert(Znode *next);

        bool Delete(Znode *aim_node);

        Znode *Find(const std::string &stat);

        Znode *Update();

     private:
        ZnodeHP *pre_;
        ZnodeHP *next_;
    };

 public:
    ZooKeeperServer();

    ZooKeeperServer(std::string profile_name);

    ZooKeeperServer(YAML::Node config);

    ZooKeeperServer(std::string profile_name, Imagine_Muduo::Connection* msg_conn);

    ZooKeeperServer(YAML::Node config, Imagine_Muduo::Connection* msg_conn);

    virtual ~ZooKeeperServer();

    void Init(std::string profile_name);

    void Init(YAML::Node config);

    void LoadBalance(); // 暂未启用

    // 设置默认的读写回调函数以及粘包判断函数
    virtual void SetDefaultReadCallback() = 0;
    virtual void SetDefaultWriteCallback() = 0;
    virtual void SetDefaultCommunicateCallback() = 0;

    // 对外接口:节点注册
    bool InsertZnode(const std::string &cluster_name, const std::string &stat, const ClusterType cluster_type = Load_Balance, const std::string &watcher_stat = "", const std::string &data = "");

    // 对外接口:节点下线,并可以更新watcher_stat
    bool DeleteZnode(const std::string &cluster_name, const std::string &stat, const std::string &watcher_stat = "");

    // 对外接口:查找集群master节点的stat,并提供更新master节点选项
    std::string GetClusterZnodeStat(const std::string &cluster_name, bool update = false, std::shared_ptr<Watcher> new_watcher = nullptr);

    // 对外接口:查找集群master节点的data,并提供更新master节点选项
    std::string GetClusterZnodeData(const std::string &cluster_name, bool update = false, std::shared_ptr<Watcher> new_watcher = nullptr);

 private:
    // 以下函数均不加任何锁,安全性由接口函数保证

    // 在集群中增加节点
    bool HighAvailabilityInsert(Znode *cluster_node, const std::string &stat, const std::string &watcher_stat = "", const std::string &data = "");
    bool LoadBalanceInsert(Znode *cluster_node, const std::string &stat, const std::string &watcher_stat = "", const std::string &data = "");
    bool HighPerformanceInsert(Znode *cluster_node, const std::string &stat, const std::string &watcher_stat = "", const std::string &data = "");

    // 在集群中移除(不delete)节点并返回移除的节点,在调用函数中进行删除.在调用时会保证待删除节点不是master节点
    Znode *HighAvailabilityDelete(Znode *cluster_node, const std::string &stat);
    Znode *LoadBalanceDelete(Znode *cluster_node, const std::string &stat);
    Znode *HighPerformanceDelete(Znode *cluster_node, const std::string &stat);

    // bool HighAvailabilityFind(std::unordered_map<std::string,Znode*>::iterator it);

    // bool LoadBalanceFind(std::unordered_map<std::string,Znode*>::iterator it);

    // bool HighPerformanceFind(std::unordered_map<std::string,Znode*>::iterator it);

    // 更新集群的master节点,并返回新的master节点
    Znode *HighAvailabilityUpdate(Znode *cluster_node);
    Znode *LoadBalanceUpdate(Znode *cluster_node);
    Znode *HighPerformanceUpdate(Znode *cluster_node);

    // 创造Znode节点
    Znode *CreateZnode(const std::string &cluster_name, ClusterType cluster_type, const std::string &stat, const std::string &data = nullptr);

    // 在map中增加集群
    bool CreateClusterInMap(const std::string &cluster_name, Znode *root_node, const ClusterType cluster_type);

    // 删除map中的相应集群
    bool DeleteClusterInMap(const std::string &cluster_name);

    // 不加锁的查找集群的主节点,因此为private方法,不允许将Znode暴露给外界,避免线程同步问题出现
    // Znode* FindClusterZnode(const std::string& cluster_name, bool update = false);

    // 更新集群master节点,返回更新结果
    Znode *UpdateClusterZnode(Znode *cluster_node, ClusterType cluster_type);

 protected:
    std::string ip_;
    std::string port_;
    size_t max_channel_num_;
    std::string log_name_;
    std::string log_path_;
    size_t max_log_file_size_;
    bool async_log_;
    bool singleton_log_mode_;
    std::string log_title_;
    bool log_with_timestamp_;
    Imagine_Tool::Logger* logger_;

 protected:
    int max_cluster_num_;                                           // 能够接收的最大集群数，暂不限定单个集群内的节点数目

 private:
    pthread_mutex_t map_lock_;
    std::unordered_map<std::string, ClusterType> type_map_;
    std::unordered_map<std::string, Znode *> node_map_;
    std::unordered_map<std::string, pthread_mutex_t *> lock_map_;

    std::unordered_map<std::string, int> unique_map_;               // 用于保证全局唯一,避免重复注册
};

} // namespace Imagine_ZooKeeper

#endif