#ifndef IMAGINE_ZOOKEEPER_H
#define IMAGINE_ZOOKEEPER_H

#include<Imagine_Muduo/Imagine_Muduo/EventLoop.h>
#include<unordered_map>
#include<list>

#include"Watcher.h"

using namespace Imagine_Muduo;

namespace Imagine_ZooKeeper{

class ZooKeeper{

public:
    enum ClusterType{
        High_Availability,
        Load_Balance,
        High_Performance
    };

public:
    class Znode{
        std::string cluster_name;//用于记录所属的cluster
        std::string stat;//暂用于记录服务器名
        std::string data;//暂用于记录服务器信息
        std::list<std::shared_ptr<Watcher>> watcher_list;
        std::string watcher_stat;//告诉watcher自己的状态

        public:
            Znode(const std::string& cluster_name_, const std::string& stat_, const std::string& data_);

            virtual ~Znode();

            std::string GetCluster();

            std::string GetStat();

            std::string GetData();

            bool SetWatcherStat(const std::string& watcher_stat_);

            virtual bool Insert(Znode* next)=0;

            virtual bool Delete(Znode* aim_node)=0;//将节点从集群结构中移除(不是真的delete)

            virtual Znode* Find(const std::string& stat_)=0;

            virtual Znode* Update()=0;//clusternode调用,返回集群更新后选举的master节点

            void Notify();

            bool AddWatcher(std::shared_ptr<Watcher> new_watcher);
    };

    class ZnodeHA : public Znode{

        public:
            ZnodeHA(const std::string& cluster_name_, const std::string& stat_, const std::string& data_);
            
            ~ZnodeHA();

            Znode* GetPre();

            Znode* GetNext();

            // bool SetPre(Znode* pre_);

            // bool SetNext(Znode* next_);

            bool Insert(Znode* next_);

            bool Delete(Znode* aim_node);

            Znode* Find(const std::string& stat_);

            Znode* Update();

        private:
            ZnodeHA* pre;
            ZnodeHA* next;
        
    };

    class ZnodeLB : public Znode{

        public:
            ZnodeLB(const std::string& cluster_name_, const std::string& stat_, const std::string& data_);
            
            ~ZnodeLB();

            Znode* GetPre();

            Znode* GetNext();

            // bool SetPre(Znode* pre_);

            // bool SetNext(Znode* next_);

            bool Insert(Znode* next_);

            bool Delete(Znode* aim_node);

            Znode* Find(const std::string& stat_);

            Znode* Update();

        private:
            ZnodeLB* pre;
            ZnodeLB* next;
        
    };

    class ZnodeHP : public Znode{

        public:
            ZnodeHP(const std::string& cluster_name_, const std::string& stat_, const std::string& data_);
            
            ~ZnodeHP();

            Znode* GetPre();

            Znode* GetNext();

            // bool SetPre(Znode* pre_);

            // bool SetNext(Znode* next_);

            bool Insert(Znode* next_);

            bool Delete(Znode* aim_node);

            Znode* Find(const std::string& stat_);

            Znode* Update();

        private:
            ZnodeHP* pre;
            ZnodeHP* next;
        
    };

public:
    ZooKeeper(int port_, int max_request_num_=10000, EventCallback read_callback_=nullptr, EventCallback write_callback_=nullptr, EventCommunicateCallback communicate_callback_=nullptr);

    virtual ~ZooKeeper();

    void loop();

    EventLoop* GetLoop();

    void LoadBalance();//暂未启用

    //向muduo提供读写回调函数以及粘包判断函数
    void SetReadCallback(EventCallback read_callback_);
    void SetWriteCallback(EventCallback write_callback_);
    void SetCommunicateCallback(EventCommunicateCallback communicate_callback_);

    //设置默认的读写回调函数以及粘包判断函数
    virtual void SetDefaultReadCallback()=0;
    virtual void SetDefaultWriteCallback()=0;
    virtual void SetDefaultCommunicateCallback()=0;

    //对外接口:节点注册
    bool InsertZnode(const std::string& cluster_name_, const std::string& stat_, const ClusterType cluster_type_=Load_Balance, const std::string& watcher_stat_="",const std::string& data_="");

    //对外接口:节点下线,并可以更新watcher_stat
    bool DeleteZnode(const std::string& cluster_name_, const std::string& stat_, const std::string& watcher_stat_="");

    //对外接口:查找集群master节点的stat,并提供更新master节点选项
    std::string GetClusterZnodeStat(const std::string& cluster_name_, bool update_=false, std::shared_ptr<Watcher> new_watcher_=nullptr);

    //对外接口:查找集群master节点的data,并提供更新master节点选项
    std::string GetClusterZnodeData(const std::string& cluster_name_, bool update_=false, std::shared_ptr<Watcher> new_watcher_=nullptr);

private:
    //以下函数均不加任何锁,安全性由接口函数保证

    //在集群中增加节点
    bool HighAvailabilityInsert(Znode* cluster_node, const std::string& stat_, const std::string& watcher_stat_="", const std::string& data_="");
    bool LoadBalanceInsert(Znode* cluster_node, const std::string& stat_, const std::string& watcher_stat_="", const std::string& data_="");
    bool HighPerformanceInsert(Znode* cluster_node, const std::string& stat_, const std::string& Watcher_stat_="", const std::string& data_="");

    //在集群中移除(不delete)节点并返回移除的节点,在调用函数中进行删除.在调用时会保证待删除节点不是master节点
    Znode* HighAvailabilityDelete(Znode* cluster_node_, const std::string& stat_);
    Znode* LoadBalanceDelete(Znode* cluster_node_, const std::string& stat_);
    Znode* HighPerformanceDelete(Znode* cluster_node_, const std::string& stat_);

    // bool HighAvailabilityFind(std::unordered_map<std::string,Znode*>::iterator it);

    // bool LoadBalanceFind(std::unordered_map<std::string,Znode*>::iterator it);

    // bool HighPerformanceFind(std::unordered_map<std::string,Znode*>::iterator it);

    //更新集群的master节点,并返回新的master节点
    Znode* HighAvailabilityUpdate(Znode* cluster_node_);
    Znode* LoadBalanceUpdate(Znode* cluster_node_);
    Znode* HighPerformanceUpdate(Znode* cluster_node_);

    //创造Znode节点
    Znode* CreateZnode(const std::string& cluster_name_, ClusterType cluster_type_, const std::string& stat_, const std::string& data_=nullptr);

    //在map中增加集群
    bool CreateClusterInMap(const std::string& cluster_name, Znode* root_node, const ClusterType cluster_type);

    //删除map中的相应集群
    bool DeleteClusterInMap(const std::string& cluster_name);

    // //不加锁的查找集群的主节点,因此为private方法,不允许将Znode暴露给外界,避免线程同步问题出现
    // Znode* FindClusterZnode(const std::string& cluster_name, bool update_=false);

    //更新集群master节点,返回更新结果
    Znode* UpdateClusterZnode(Znode* cluster_node, ClusterType cluster_type);

protected:

    EventLoop* loop_=nullptr;
    EventCallback read_callback;
    EventCallback write_callback;
    EventCommunicateCallback communicate_callback;//向muduo提供粘包判断函数

    int port;
    int max_cluster_num;//能够接收的最大集群数，暂不限定单个集群内的节点数目

private:

    pthread_mutex_t map_lock;
    std::unordered_map<std::string,ClusterType> type_map;
    std::unordered_map<std::string,Znode*> node_map;
    std::unordered_map<std::string,pthread_mutex_t*> lock_map;

    std::unordered_map<std::string,int> unique_map;//用于保证全局唯一,避免重复注册
};


}



#endif