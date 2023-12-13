#include "Imagine_ZooKeeper/ZooKeeperServer.h"

#include "Imagine_ZooKeeper/log_macro.h"
#include "Imagine_ZooKeeper/ZooKeeperUtil.h"

#include <fstream>

namespace Imagine_ZooKeeper
{

ZooKeeperServer::ZooKeeperServer() : Imagine_Muduo::TcpServer()
{    
    if (pthread_mutex_init(&map_lock_, nullptr) != 0) {
        throw std::exception();
    }
}

ZooKeeperServer::ZooKeeperServer(const std::string& profile_name) : Imagine_Muduo::TcpServer(profile_name)
{
    Init(profile_name);

    if (pthread_mutex_init(&map_lock_, nullptr) != 0) {
        throw std::exception();
    }
}

ZooKeeperServer::ZooKeeperServer(const YAML::Node& config) : Imagine_Muduo::TcpServer(config)
{
    Init(config);

    if (pthread_mutex_init(&map_lock_, nullptr) != 0) {
        throw std::exception();
    }
}

ZooKeeperServer::ZooKeeperServer(const std::string& profile_name, Imagine_Muduo::Connection* msg_conn) : Imagine_Muduo::TcpServer(profile_name, msg_conn)
{
    Init(profile_name);

    if (pthread_mutex_init(&map_lock_, nullptr) != 0) {
        throw std::exception();
    }
}

ZooKeeperServer::ZooKeeperServer(const YAML::Node& config, Imagine_Muduo::Connection* msg_conn) : Imagine_Muduo::TcpServer(config, msg_conn)
{
    Init(config);

    if (pthread_mutex_init(&map_lock_, nullptr) != 0) {
        throw std::exception();
    }
}

ZooKeeperServer::~ZooKeeperServer()
{
}

void ZooKeeperServer::Init(const std::string& profile_name)
{
    if (profile_name == "") {
        throw std::exception();
    }

    YAML::Node config = YAML::LoadFile(profile_name);
    Init(config);
}

void ZooKeeperServer::Init(const YAML::Node& config)
{
    singleton_log_mode_ = config["singleton_log_mode"].as<bool>();

    if (singleton_log_mode_) {
        logger_ = SingletonLogger::GetInstance();
    } else {
        logger_ = new NonSingletonLogger();
        Logger::SetInstance(logger_);
    }

    logger_->Init(config);
}

void ZooKeeperServer::LoadBalance()
{
    // type = Load_Balance;
}

ZooKeeperServer::Znode *ZooKeeperServer::CreateZnode(const std::string &cluster_name, ClusterType cluster_type, const std::pair<std::string, std::string> &stat, const std::string &data) const
{

    switch (cluster_type) 
    {
        case High_Availability:
            return new ZnodeHA(cluster_name, stat, data);
            break;

        case Load_Balance:
            return new ZnodeLB(cluster_name, stat, data);
            break;

        case High_Performance:
            return new ZnodeHP(cluster_name, stat, data);

        default:
            throw std::exception();
            return nullptr;
            break;
    }
}

bool ZooKeeperServer::CreateClusterInMap(const std::string &cluster_name, Znode *root_node, const ClusterType cluster_type)
{

    pthread_mutex_t *cluster_lock = new pthread_mutex_t;
    if (pthread_mutex_init(cluster_lock, nullptr) != 0) {
        IMAGINE_ZOOKEEPER_LOG("CreateClusterInMap exception!");
        throw std::exception();
    }

    node_map_.insert(std::make_pair(cluster_name, root_node));
    type_map_.insert(std::make_pair(cluster_name, cluster_type));
    lock_map_.insert(std::make_pair(cluster_name, cluster_lock));

    return true;
}

bool ZooKeeperServer::DeleteClusterInMap(const std::string &cluster_name)
{
    // pthreadd_mutex_t的删除得保证锁住map_lock!

    // pthread_mutex_lock(&map_lock);
    pthread_mutex_t *cluster_lock = lock_map_.find(cluster_name)->second;
    pthread_mutex_destroy(cluster_lock);

    type_map_.erase(type_map_.find(cluster_name));
    node_map_.erase(node_map_.find(cluster_name));
    lock_map_.erase(lock_map_.find(cluster_name));
    // pthread_mutex_unlock(&map_lock);

    return true;
}

bool ZooKeeperServer::InsertZnode(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const ClusterType type, const std::string &watcher_stat, const std::string &data)
{
    pthread_mutex_lock(&map_lock_);
    if (unique_map_.find(ZooKeeperUtil::GenerateZNodeName(cluster_name, stat)) == unique_map_.end()) {
        unique_map_.insert(std::make_pair(ZooKeeperUtil::GenerateZNodeName(cluster_name, stat), 1));
    } else {
        IMAGINE_ZOOKEEPER_LOG("register repeat!"); // 拒绝重复的注册活动
        pthread_mutex_unlock(&map_lock_);
        return false;
    }
    std::unordered_map<std::string, ClusterType>::iterator type_it = type_map_.find(cluster_name);
    std::unordered_map<std::string, Znode *>::iterator node_it = node_map_.find(cluster_name);
    std::unordered_map<std::string, pthread_mutex_t *>::iterator lock_it = lock_map_.find(cluster_name);
    if (type_it == type_map_.end() && node_it == node_map_.end() && lock_it == lock_map_.end()) {
        // 未创建相应集群,自动创建
        //  pthread_mutex_unlock(&map_lock);
        Znode *new_cluster = CreateZnode(cluster_name, type, stat, data);
        new_cluster->SetWatcherStat(watcher_stat);
        CreateClusterInMap(cluster_name, new_cluster, type);
        pthread_mutex_unlock(&map_lock_);

        return new_cluster;
    }
    pthread_mutex_lock(lock_it->second);
    // 防止迭代器失效
    Znode *cluster_node = node_it->second;
    ClusterType cluster_type = type_it->second;
    pthread_mutex_t *cluster_lock = lock_it->second;
    pthread_mutex_unlock(&map_lock_);

    bool flag = false;
    switch (cluster_type)
    {
        case High_Availability:
            flag = HighAvailabilityInsert(cluster_node, stat, watcher_stat, data);
            break;

        case Load_Balance:
            flag = LoadBalanceInsert(cluster_node, stat, watcher_stat, data);
            break;

        case High_Performance:
            flag = HighPerformanceInsert(cluster_node, stat, watcher_stat, data);
            break;

        default:
            throw std::exception();
            break;
    }
    pthread_mutex_unlock(cluster_lock);

    return flag;
}

bool ZooKeeperServer::DeleteZnode(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &watcher_stat)
{
    // printf("Delete Node Stat is %s\n",&stat_[0]);
    pthread_mutex_lock(&map_lock_);
    if (unique_map_.find(ZooKeeperUtil::GenerateZNodeName(cluster_name, stat)) == unique_map_.end()) {
        IMAGINE_ZOOKEEPER_LOG("delete unregister exception!");
        pthread_mutex_unlock(&map_lock_);
        throw std::exception();
    }
    unique_map_.erase(ZooKeeperUtil::GenerateZNodeName(cluster_name, stat));
    std::unordered_map<std::string, ClusterType>::iterator type_it = type_map_.find(cluster_name);
    std::unordered_map<std::string, Znode *>::iterator node_it = node_map_.find(cluster_name);
    std::unordered_map<std::string, pthread_mutex_t *>::iterator lock_it = lock_map_.find(cluster_name);
    if (type_it == type_map_.end() || node_it == node_map_.end()) {
        pthread_mutex_unlock(&map_lock_);
        IMAGINE_ZOOKEEPER_LOG("DeleteZnode exception!");
        throw std::exception(); // 没有该集群
    }

    IMAGINE_ZOOKEEPER_LOG("Cluster Node Stat is %s:%s", node_it->second->GetStat().first.c_str(), node_it->second->GetStat().second.c_str());

    // 若要删除节点为当前集群主节点,则应更新主节点
    bool is_lock_cluster = false; // 标识是否由于主节点更新而上锁
    if (node_it->second->GetStat() == stat) {
        pthread_mutex_lock(lock_it->second);
        is_lock_cluster = true;
        Znode *update_node = UpdateClusterZnode(node_it->second, type_it->second);
        // printf("UpdateNode Stat is %s\n",&update_node->GetStat()[0]);
        if (update_node == node_it->second) {
            // 只有一个节点,删除集群
            DeleteClusterInMap(cluster_name);
            pthread_mutex_unlock(&map_lock_);
            update_node->SetWatcherStat(watcher_stat);
            update_node->Notify();
            delete update_node;
            IMAGINE_ZOOKEEPER_LOG("delete cluster!");

            return true;
        } else {
            // printf("Update ClusterNode! ClusterNode Stat is %s\n",&update_node->GetStat()[0]);
            node_it->second = update_node; // 更新主节点完毕
        }
    }

    if (!is_lock_cluster) {
        pthread_mutex_lock(lock_it->second); // 未上锁则上锁
    }
    // 防止迭代器失效
    Znode *cluster_node = node_it->second;
    ClusterType cluster_type = type_it->second;
    pthread_mutex_t *cluster_lock = lock_it->second;
    pthread_mutex_unlock(&map_lock_);

    Znode *delete_node = nullptr;
    switch (cluster_type)
    {
        case High_Availability:
            delete_node = HighAvailabilityDelete(cluster_node, stat);
            break;

        case Load_Balance:
            delete_node = LoadBalanceDelete(cluster_node, stat);
            break;

        case High_Performance:
            delete_node = HighPerformanceDelete(cluster_node, stat);
            break;

        default:
            return false;
            throw std::exception();
            break;
    }
    pthread_mutex_unlock(cluster_lock);
    delete_node->SetWatcherStat(watcher_stat);
    delete_node->Notify();

    delete delete_node;

    return true;
}

// ZooKeeperServer::Znode* ZooKeeperServer::FindClusterZnode(const std::string& cluster_name, bool update){

//     std::unordered_map<std::string,ClusterType>::iterator type_it = type_map.find(cluster_name);
//     std::unordered_map<std::string,Znode*>::iterator node_it = node_map.find(cluster_name);
//     std::unordered_map<std::string,pthread_mutex_t*>::iterator lock_it = cluster_lock.find(cluster_name);
//     if(type_it == type_map.end() || node_it == node_map.end() || lock_it == cluster_lock.end()){
//         throw std::exception();//异常
//         //pthread_mutex_unlock(&map_lock);
//         return nullptr;
//     }
//     return node_it->second;
// }

bool ZooKeeperServer::HighAvailabilityInsert(Znode *cluster_node, const std::pair<std::string, std::string> &stat, const std::string &watcher_stat, const std::string &data)
{
    return false;
}

bool ZooKeeperServer::LoadBalanceInsert(Znode *cluster_node, const std::pair<std::string, std::string> &stat, const std::string &watcher_stat, const std::string &data)
{
    ZnodeLB *node = new ZnodeLB(cluster_node->GetCluster(), stat, data);
    node->SetWatcherStat(watcher_stat);
    cluster_node->Insert(node);

    return node;
}

bool ZooKeeperServer::HighPerformanceInsert(Znode *cluster_node, const std::pair<std::string, std::string> &stat, const std::string &Watcher_stat, const std::string &data)
{
    return false;
}

ZooKeeperServer::Znode *ZooKeeperServer::HighAvailabilityDelete(Znode *cluster_node, const std::pair<std::string, std::string> &stat)
{

    return nullptr;
}

ZooKeeperServer::Znode *ZooKeeperServer::LoadBalanceDelete(Znode *cluster_node, const std::pair<std::string, std::string> &stat)
{

    Znode *aim_node = cluster_node->Find(stat);
    if (aim_node) {
        cluster_node->Delete(aim_node);
    } else {
        return nullptr;
        throw std::exception(); // 未找到
    }

    return aim_node;
}

ZooKeeperServer::Znode *ZooKeeperServer::HighPerformanceDelete(Znode *cluster_node, const std::pair<std::string, std::string> &stat)
{

    return nullptr;
}

ZooKeeperServer::Znode::Znode(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &data) : cluster_name_(cluster_name), stat_(stat), data_(data){};

ZooKeeperServer::Znode::~Znode(){};

ZooKeeperServer::ZnodeHA::ZnodeHA(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &data) : Znode(cluster_name, stat, data)
{
    // this->pre=this;
    // this->next=this;
}

ZooKeeperServer::ZnodeLB::ZnodeLB(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &data) : Znode(cluster_name, stat, data)
{
    this->pre_ = this;
    this->next_ = this;
}

ZooKeeperServer::ZnodeHP::ZnodeHP(const std::string &cluster_name, const std::pair<std::string, std::string> &stat, const std::string &data) : Znode(cluster_name, stat, data)
{
    // this->pre=this;
    // this->next=this;
}

ZooKeeperServer::Znode *ZooKeeperServer::HighAvailabilityUpdate(Znode *cluster_node) const
{
    return nullptr;
}

ZooKeeperServer::Znode *ZooKeeperServer::LoadBalanceUpdate(Znode *cluster_node) const
{
    return cluster_node->Update();
}

ZooKeeperServer::Znode *ZooKeeperServer::HighPerformanceUpdate(Znode *cluster_node) const
{
    return nullptr;
}

ZooKeeperServer::ZnodeHA::~ZnodeHA()
{
}

ZooKeeperServer::ZnodeLB::~ZnodeLB()
{
}

ZooKeeperServer::ZnodeHP::~ZnodeHP()
{
}

std::string ZooKeeperServer::Znode::GetCluster() const
{
    return cluster_name_;
}

std::pair<std::string, std::string> ZooKeeperServer::Znode::GetStat() const
{
    return stat_;
}

std::string ZooKeeperServer::Znode::GetData() const
{
    return data_;
}

ZooKeeperServer::Znode *ZooKeeperServer::ZnodeLB::GetPre() const
{
    return this->pre_;
}

ZooKeeperServer::Znode *ZooKeeperServer::ZnodeLB::GetNext() const
{
    return this->next_;
}

ZooKeeperServer::Znode* ZooKeeperServer::ZnodeHA::Insert(Znode *next) { return this; }

ZooKeeperServer::Znode* ZooKeeperServer::ZnodeLB::Insert(Znode *next)
{
    // 在该节点之后插入一个新节点next_到双向循环链表

    ZnodeLB *next_lb = dynamic_cast<ZnodeLB *>(next);
    if (next_lb == nullptr) {
        IMAGINE_ZOOKEEPER_LOG("Insert exception!");
        throw std::exception();
    }

    next_lb->next_ = this->next_;
    this->next_ = next_lb;
    next_lb->next_->pre_ = next_lb;
    next_lb->pre_ = this;

    return this;
}

ZooKeeperServer::Znode* ZooKeeperServer::ZnodeHP::Insert(Znode *next) { return this; }

ZooKeeperServer::Znode* ZooKeeperServer::ZnodeHA::Delete(Znode *aim_node) { return this; }

ZooKeeperServer::Znode* ZooKeeperServer::ZnodeLB::Delete(Znode *aim_node)
{
    // 将该节点从双向循环链表中摘除

    ZnodeLB *node = this;
    while (node != aim_node) {
        node = node->next_;
        if (node == this) {
            // 未找到节点
            IMAGINE_ZOOKEEPER_LOG("Delete exception!");
            throw std::exception();
        }
    }

    if (node->next_ == node) {
        // 只有一个节点
        node->pre_ = node->next_ = nullptr;
    } else {
        node->pre_->next_ = node->next_;
        node->next_->pre_ = node->pre_;
    }

    return this;
}

ZooKeeperServer::Znode* ZooKeeperServer::ZnodeHP::Delete(Znode *aim_node) { return this; }

ZooKeeperServer::Znode *ZooKeeperServer::ZnodeHA::Find(const std::pair<std::string, std::string> &stat) { return nullptr; }

ZooKeeperServer::Znode *ZooKeeperServer::ZnodeLB::Find(const std::pair<std::string, std::string> &stat)
{
    ZnodeLB *aim_node = this;
    while (aim_node->GetStat() != stat) {
        aim_node = aim_node->next_;
        if (aim_node == this) {
            // 找不到
            IMAGINE_ZOOKEEPER_LOG("Find exception!");
            throw std::exception();
        }
    }

    return aim_node;
}

ZooKeeperServer::Znode *ZooKeeperServer::ZnodeHP::Find(const std::pair<std::string, std::string> &stat) { return nullptr; }

ZooKeeperServer::Znode *ZooKeeperServer::ZnodeHA::Update() { return nullptr; }

ZooKeeperServer::Znode *ZooKeeperServer::ZnodeLB::Update()
{
    return this->next_;
}

ZooKeeperServer::Znode *ZooKeeperServer::ZnodeHP::Update() { return nullptr; }

const ZooKeeperServer::Znode* ZooKeeperServer::Znode::Notify() const
{
    for (std::list<std::shared_ptr<Watcher>>::const_iterator it = watcher_list_.begin(); it != watcher_list_.end(); it++) {
        (*it)->Update(this->watcher_stat_);
    }

    return this;
}

ZooKeeperServer::Znode* ZooKeeperServer::Znode::AddWatcher(std::shared_ptr<Watcher> new_watcher)
{
    if (!new_watcher) {
        IMAGINE_ZOOKEEPER_LOG("AddWatcher exception!");
        throw std::exception();
    }

    watcher_list_.push_back(new_watcher);

    return this;
}

ZooKeeperServer::Znode* ZooKeeperServer::Znode::SetWatcherStat(const std::string &watcher_stat)
{
    this->watcher_stat_ = watcher_stat;

    return this;
}

std::pair<std::string, std::string> ZooKeeperServer::GetClusterZnodeStat(const std::string &cluster_name, bool update, std::shared_ptr<Watcher> new_watcher)
{
    pthread_mutex_lock(&map_lock_);
    std::unordered_map<std::string, ClusterType>::iterator type_it = type_map_.find(cluster_name);
    std::unordered_map<std::string, Znode *>::iterator node_it = node_map_.find(cluster_name);
    std::unordered_map<std::string, pthread_mutex_t *>::iterator lock_it = lock_map_.find(cluster_name);
    if (type_it == type_map_.end() || node_it == node_map_.end() || lock_it == lock_map_.end()) {
        IMAGINE_ZOOKEEPER_LOG("GetClusterZnodeStat exception!");
        pthread_mutex_unlock(&map_lock_);
        return std::make_pair("", "");
        throw std::exception(); // 异常
    }
    std::pair<std::string, std::string> stat = node_it->second->GetStat();
    if (new_watcher) {
        node_it->second->AddWatcher(new_watcher);
    }
    if (update) {
        node_it->second = UpdateClusterZnode(node_it->second, type_it->second);
    }
    pthread_mutex_unlock(&map_lock_);

    return stat;
}

std::string ZooKeeperServer::GetClusterZnodeData(const std::string &cluster_name, bool update, std::shared_ptr<Watcher> new_wacher)
{

    pthread_mutex_lock(&map_lock_);
    std::unordered_map<std::string, ClusterType>::iterator type_it = type_map_.find(cluster_name);
    std::unordered_map<std::string, Znode *>::iterator node_it = node_map_.find(cluster_name);
    std::unordered_map<std::string, pthread_mutex_t *>::iterator lock_it = lock_map_.find(cluster_name);
    if (type_it == type_map_.end() || node_it == node_map_.end() || lock_it == lock_map_.end()) {
        IMAGINE_ZOOKEEPER_LOG("GetClusterZnodeData exception!");
        throw std::exception(); // 异常
        // pthread_mutex_unlock(&map_lock);
        return "";
    }
    std::string data = node_it->second->GetData();
    if (new_wacher) {
        node_it->second->AddWatcher(new_wacher);
    }
    if (update) {
        node_it->second = UpdateClusterZnode(node_it->second, type_it->second);
    }
    pthread_mutex_unlock(&map_lock_);

    return data;
    // pthread_mutex_lock(&map_lock);
    // std::unordered_map<std::string,Znode*>::iterator node_it = node_map.find(cluster_name);
    // if (node_it == node_map.end()) {
    //     pthread_mutex_unlock(&map_lock);
    //     return "";
    //     throw std::exception();
    // } else {
    //     std::string data_ = node_it->second->GetData();
    //     pthread_mutex_unlock(&map_lock);

    //     return data_;
    // }
}

ZooKeeperServer::Znode *ZooKeeperServer::UpdateClusterZnode(Znode *cluster_node, ClusterType cluster_type) const
{
    switch (cluster_type)
    {
        case High_Availability:
            return HighAvailabilityUpdate(cluster_node);
            break;

        case Load_Balance:
            return LoadBalanceUpdate(cluster_node);
            break;

        case High_Performance:
            return HighPerformanceUpdate(cluster_node);
            break;

        default:
            break;
    }

    return nullptr;
}

} // namespace Imagine_ZooKeeper