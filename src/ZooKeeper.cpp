#include "Imagine_ZooKeeper/ZooKeeper.h"

namespace Imagine_ZooKeeper
{

ZooKeeper::ZooKeeper(int port, int max_request_num, Imagine_Muduo::EventCallback read_callback, Imagine_Muduo::EventCallback write_callback, Imagine_Muduo::EventCommunicateCallback communicate_callback)
    : read_callback_(read_callback), write_callback_(write_callback), communicate_callback_(communicate_callback)
{
    if (port < 0) {
        throw std::exception();
    }

    if (pthread_mutex_init(&map_lock_, nullptr) != 0) {
        throw std::exception();
    }

    port_ = port;
    max_cluster_num_ = max_request_num;
    loop_ = nullptr;
}

ZooKeeper::~ZooKeeper()
{
    if (loop_) {
        delete loop_;
    }
}

void ZooKeeper::loop()
{
    loop_->loop();
}

Imagine_Muduo::EventLoop *ZooKeeper::GetLoop()
{
    return loop_;
}

void ZooKeeper::LoadBalance()
{
    // type=Load_Balance;
}

void ZooKeeper::SetReadCallback(Imagine_Muduo::EventCallback read_callback)
{
    read_callback_ = read_callback;
    loop_->SetReadCallback(read_callback_);
}

void ZooKeeper::SetWriteCallback(Imagine_Muduo::EventCallback write_callback)
{
    write_callback_ = write_callback;
    loop_->SetWriteCallback(write_callback_);
}

void ZooKeeper::SetCommunicateCallback(Imagine_Muduo::EventCommunicateCallback communicate_callback)
{
    communicate_callback_ = communicate_callback;
    loop_->SetWriteCallback(write_callback_);
}

ZooKeeper::Znode *ZooKeeper::CreateZnode(const std::string &cluster_name, ClusterType cluster_type, const std::string &stat, const std::string &data)
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

bool ZooKeeper::CreateClusterInMap(const std::string &cluster_name, Znode *root_node, const ClusterType cluster_type)
{

    pthread_mutex_t *cluster_lock = new pthread_mutex_t;
    if (pthread_mutex_init(cluster_lock, nullptr) != 0) {
        LOG_INFO("CreateClusterInMap exception!");
        throw std::exception();
    }

    node_map_.insert(std::make_pair(cluster_name, root_node));
    type_map_.insert(std::make_pair(cluster_name, cluster_type));
    lock_map_.insert(std::make_pair(cluster_name, cluster_lock));

    return true;
}

bool ZooKeeper::DeleteClusterInMap(const std::string &cluster_name)
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

bool ZooKeeper::InsertZnode(const std::string &name, const std::string &stat, const ClusterType type, const std::string &watcher_stat, const std::string &data)
{
    pthread_mutex_lock(&map_lock_);
    if (unique_map_.find(name + stat) == unique_map_.end()) {
        unique_map_.insert(std::make_pair(name + stat, 1));
    } else {
        LOG_INFO("register repeat!"); // 拒绝重复的注册活动
        pthread_mutex_unlock(&map_lock_);
        return false;
    }
    std::unordered_map<std::string, ClusterType>::iterator type_it = type_map_.find(name);
    std::unordered_map<std::string, Znode *>::iterator node_it = node_map_.find(name);
    std::unordered_map<std::string, pthread_mutex_t *>::iterator lock_it = lock_map_.find(name);
    if (type_it == type_map_.end() && node_it == node_map_.end() && lock_it == lock_map_.end()) {
        // 未创建相应集群,自动创建
        //  pthread_mutex_unlock(&map_lock);
        Znode *new_cluster = CreateZnode(name, type, stat, data);
        new_cluster->SetWatcherStat(watcher_stat);
        CreateClusterInMap(name, new_cluster, type);
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

bool ZooKeeper::DeleteZnode(const std::string &cluster_name, const std::string &stat, const std::string &watcher_stat)
{
    // printf("Delete Node Stat is %s\n",&stat_[0]);
    pthread_mutex_lock(&map_lock_);
    if (unique_map_.find(cluster_name + stat) == unique_map_.end()) {
        LOG_INFO("delete unregister exception!");
        pthread_mutex_unlock(&map_lock_);
        throw std::exception();
    }
    unique_map_.erase(cluster_name + stat);
    std::unordered_map<std::string, ClusterType>::iterator type_it = type_map_.find(cluster_name);
    std::unordered_map<std::string, Znode *>::iterator node_it = node_map_.find(cluster_name);
    std::unordered_map<std::string, pthread_mutex_t *>::iterator lock_it = lock_map_.find(cluster_name);
    if (type_it == type_map_.end() || node_it == node_map_.end()) {
        pthread_mutex_unlock(&map_lock_);
        LOG_INFO("DeleteZnode exception!");
        throw std::exception(); // 没有该集群
    }

    LOG_INFO("Cluster Node Stat is %s", &node_it->second->GetStat()[0]);

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
            LOG_INFO("delete cluster!");

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
            LOG_INFO("1111111");
            delete_node = LoadBalanceDelete(cluster_node, stat);
            LOG_INFO("1111112");
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

// ZooKeeper::Znode* ZooKeeper::FindClusterZnode(const std::string& cluster_name, bool update){

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

bool ZooKeeper::HighAvailabilityInsert(Znode *cluster_node, const std::string &stat, const std::string &watcher_stat, const std::string &data)
{
    return false;
}

bool ZooKeeper::LoadBalanceInsert(Znode *cluster_node, const std::string &stat, const std::string &watcher_stat, const std::string &data)
{
    ZnodeLB *node = new ZnodeLB(cluster_node->GetCluster(), stat, data);
    node->SetWatcherStat(watcher_stat);
    cluster_node->Insert(node);

    return node;
}

bool ZooKeeper::HighPerformanceInsert(Znode *cluster_node, const std::string &stat, const std::string &Watcher_stat, const std::string &data)
{
    return false;
}

ZooKeeper::Znode *ZooKeeper::HighAvailabilityDelete(Znode *cluster_node, const std::string &stat)
{

    return nullptr;
}

ZooKeeper::Znode *ZooKeeper::LoadBalanceDelete(Znode *cluster_node, const std::string &stat)
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

ZooKeeper::Znode *ZooKeeper::HighPerformanceDelete(Znode *cluster_node, const std::string &stat)
{

    return nullptr;
}

ZooKeeper::Znode::Znode(const std::string &cluster_name, const std::string &stat, const std::string &data) : cluster_name_(cluster_name), stat_(stat), data_(data){};

ZooKeeper::Znode::~Znode(){};

ZooKeeper::ZnodeHA::ZnodeHA(const std::string &cluster_name, const std::string &stat, const std::string &data) : Znode(cluster_name, stat, data)
{
    // this->pre=this;
    // this->next=this;
}

ZooKeeper::ZnodeLB::ZnodeLB(const std::string &cluster_name, const std::string &stat, const std::string &data) : Znode(cluster_name, stat, data)
{
    this->pre_ = this;
    this->next_ = this;
}

ZooKeeper::ZnodeHP::ZnodeHP(const std::string &cluster_name, const std::string &stat, const std::string &data) : Znode(cluster_name, stat, data)
{
    // this->pre=this;
    // this->next=this;
}

ZooKeeper::Znode *ZooKeeper::HighAvailabilityUpdate(Znode *cluster_node)
{
    return nullptr;
}

ZooKeeper::Znode *ZooKeeper::LoadBalanceUpdate(Znode *cluster_node)
{
    return cluster_node->Update();
}

ZooKeeper::Znode *ZooKeeper::HighPerformanceUpdate(Znode *cluster_node)
{
    return nullptr;
}

ZooKeeper::ZnodeHA::~ZnodeHA() {}

ZooKeeper::ZnodeLB::~ZnodeLB() {}

ZooKeeper::ZnodeHP::~ZnodeHP() {}

std::string ZooKeeper::Znode::GetCluster()
{
    return cluster_name_;
}

std::string ZooKeeper::Znode::GetStat()
{
    return stat_;
}

std::string ZooKeeper::Znode::GetData()
{
    return data_;
}

ZooKeeper::Znode *ZooKeeper::ZnodeLB::GetPre()
{
    return this->pre_;
}

ZooKeeper::Znode *ZooKeeper::ZnodeLB::GetNext()
{
    return this->next_;
}

bool ZooKeeper::ZnodeHA::Insert(Znode *next) { return false; }

bool ZooKeeper::ZnodeLB::Insert(Znode *next)
{
    // 在该节点之后插入一个新节点next_到双向循环链表

    ZnodeLB *next_lb = dynamic_cast<ZnodeLB *>(next);
    if (next_lb == nullptr) {
        LOG_INFO("Insert exception!");
        throw std::exception();
        return false;
    }

    next_lb->next_ = this->next_;
    this->next_ = next_lb;
    next_lb->next_->pre_ = next_lb;
    next_lb->pre_ = this;

    return true;
}

bool ZooKeeper::ZnodeHP::Insert(Znode *next) { return false; }

bool ZooKeeper::ZnodeHA::Delete(Znode *aim_node) { return false; }

bool ZooKeeper::ZnodeLB::Delete(Znode *aim_node)
{
    // 将该节点从双向循环链表中摘除

    ZnodeLB *node = this;
    while (node != aim_node) {
        node = node->next_;
        if (node == this) {
            // 未找到节点
            LOG_INFO("Delete exception!");
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

    return true;
}

bool ZooKeeper::ZnodeHP::Delete(Znode *aim_node) { return false; }

ZooKeeper::Znode *ZooKeeper::ZnodeHA::Find(const std::string &stat) { return nullptr; }

ZooKeeper::Znode *ZooKeeper::ZnodeLB::Find(const std::string &stat)
{
    ZnodeLB *aim_node = this;
    while (aim_node->GetStat() != stat) {
        aim_node = aim_node->next_;
        if (aim_node == this) {
            // 找不到
            LOG_INFO("Find exception!");
            throw std::exception();
        }
    }

    return aim_node;
}

ZooKeeper::Znode *ZooKeeper::ZnodeHP::Find(const std::string &stat) { return nullptr; }

ZooKeeper::Znode *ZooKeeper::ZnodeHA::Update() { return nullptr; }

ZooKeeper::Znode *ZooKeeper::ZnodeLB::Update()
{
    return this->next_;
}

ZooKeeper::Znode *ZooKeeper::ZnodeHP::Update() { return nullptr; }

void ZooKeeper::Znode::Notify()
{
    for (std::list<std::shared_ptr<Watcher>>::iterator it = watcher_list_.begin(); it != watcher_list_.end(); it++) {
        (*it)->Update(this->watcher_stat_);
    }
}

bool ZooKeeper::Znode::AddWatcher(std::shared_ptr<Watcher> new_watcher)
{
    if (!new_watcher) {
        LOG_INFO("AddWatcher exception!");
        throw std::exception();
        return false;
    }

    watcher_list_.push_back(new_watcher);

    return true;
}

bool ZooKeeper::Znode::SetWatcherStat(const std::string &watcher_stat)
{
    this->watcher_stat_ = watcher_stat;
    return true;
}

std::string ZooKeeper::GetClusterZnodeStat(const std::string &cluster_name, bool update, std::shared_ptr<Watcher> new_watcher)
{
    pthread_mutex_lock(&map_lock_);
    std::unordered_map<std::string, ClusterType>::iterator type_it = type_map_.find(cluster_name);
    std::unordered_map<std::string, Znode *>::iterator node_it = node_map_.find(cluster_name);
    std::unordered_map<std::string, pthread_mutex_t *>::iterator lock_it = lock_map_.find(cluster_name);
    if (type_it == type_map_.end() || node_it == node_map_.end() || lock_it == lock_map_.end()) {
        LOG_INFO("GetClusterZnodeStat exception!");
        pthread_mutex_unlock(&map_lock_);
        return "";
        throw std::exception(); // 异常
    }
    std::string stat_ = node_it->second->GetStat();
    if (new_watcher) {
        node_it->second->AddWatcher(new_watcher);
    }
    if (update) {
        node_it->second = UpdateClusterZnode(node_it->second, type_it->second);
    }
    pthread_mutex_unlock(&map_lock_);

    return stat_;
}

std::string ZooKeeper::GetClusterZnodeData(const std::string &cluster_name, bool update, std::shared_ptr<Watcher> new_wacher)
{

    pthread_mutex_lock(&map_lock_);
    std::unordered_map<std::string, ClusterType>::iterator type_it = type_map_.find(cluster_name);
    std::unordered_map<std::string, Znode *>::iterator node_it = node_map_.find(cluster_name);
    std::unordered_map<std::string, pthread_mutex_t *>::iterator lock_it = lock_map_.find(cluster_name);
    if (type_it == type_map_.end() || node_it == node_map_.end() || lock_it == lock_map_.end()) {
        LOG_INFO("GetClusterZnodeData exception!");
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

ZooKeeper::Znode *ZooKeeper::UpdateClusterZnode(Znode *cluster_node, ClusterType cluster_type)
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