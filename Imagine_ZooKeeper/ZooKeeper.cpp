#include"ZooKeeper.h"

using namespace Imagine_ZooKeeper;

ZooKeeper::ZooKeeper(int port_, int max_request_num_, EventCallback read_callback_, EventCallback write_callback_, EventCommunicateCallback communicate_callback_)
:read_callback(read_callback_),write_callback(write_callback_),communicate_callback(communicate_callback_)
{
    
    if(port<0){
        throw std::exception();
    }

    if(pthread_mutex_init(&map_lock,nullptr)!=0){
        throw std::exception();
    }

    port=port_;
    max_cluster_num=max_request_num_;
    loop_=nullptr;
}

ZooKeeper::~ZooKeeper(){
    if(loop_)delete loop_;
}

void ZooKeeper::loop(){
    loop_->loop();
}

EventLoop* ZooKeeper::GetLoop(){
    return loop_;
}

void ZooKeeper::LoadBalance(){
    //type=Load_Balance;
}

void ZooKeeper::SetReadCallback(EventCallback read_callback_){
    read_callback=read_callback_;
    loop_->SetReadCallback(read_callback);
}

void ZooKeeper::SetWriteCallback(EventCallback write_callback_){
    write_callback=write_callback_;
    loop_->SetWriteCallback(write_callback);
}

void ZooKeeper::SetCommunicateCallback(EventCommunicateCallback communicate_callback_){
    communicate_callback=communicate_callback_;
    loop_->SetWriteCallback(write_callback);
}

ZooKeeper::Znode* ZooKeeper::CreateZnode(const std::string& cluster_name, ClusterType cluster_type_, const std::string& stat_, const std::string& data_){
    
    switch (cluster_type_)
    {
    case High_Availability:
        return new ZnodeHA(cluster_name,stat_,data_);
        break;

    case Load_Balance:
        return new ZnodeLB(cluster_name,stat_,data_);
        break;

    case High_Performance:
        return new ZnodeHP(cluster_name,stat_,data_);
    
    default:
        throw std::exception();
        return nullptr;
        break;
    }
}

bool ZooKeeper::CreateClusterInMap(const std::string& cluster_name, Znode* root_node, const ClusterType cluster_type){
    
    pthread_mutex_t* cluster_lock_=new pthread_mutex_t;
    if(pthread_mutex_init(cluster_lock_,nullptr)!=0){
        printf("CreateClusterInMap exception!\n");
        throw std::exception();
    }

    node_map.insert(std::make_pair(cluster_name,root_node));
    type_map.insert(std::make_pair(cluster_name,cluster_type));
    lock_map.insert(std::make_pair(cluster_name,cluster_lock_));

    return true;
}

bool ZooKeeper::DeleteClusterInMap(const std::string& cluster_name){
    //pthreadd_mutex_t的删除得保证锁住map_lock!

    //pthread_mutex_lock(&map_lock);
    pthread_mutex_t* cluster_lock_=lock_map.find(cluster_name)->second;
    pthread_mutex_destroy(cluster_lock_);

    type_map.erase(type_map.find(cluster_name));
    node_map.erase(node_map.find(cluster_name));
    lock_map.erase(lock_map.find(cluster_name));
    //pthread_mutex_unlock(&map_lock);

    return true;
}

bool ZooKeeper::InsertZnode(const std::string& cluster_name_, const std::string& stat_, const ClusterType cluster_type_, const std::string& watcher_stat_, const std::string& data_)
{
    pthread_mutex_lock(&map_lock);
    if(unique_map.find(cluster_name_+stat_)==unique_map.end()){
        unique_map.insert(std::make_pair(cluster_name_+stat_,1));
    }else{
        printf("register repeat!\n");//拒绝重复的注册活动
        pthread_mutex_unlock(&map_lock);
        return false;
    }
    std::unordered_map<std::string,ClusterType>::iterator type_it=type_map.find(cluster_name_);
    std::unordered_map<std::string,Znode*>::iterator node_it=node_map.find(cluster_name_);
    std::unordered_map<std::string,pthread_mutex_t*>::iterator lock_it=lock_map.find(cluster_name_);
    if(type_it==type_map.end()&&node_it==node_map.end()&&lock_it==lock_map.end()){
        //未创建相应集群,自动创建
        // pthread_mutex_unlock(&map_lock);
        Znode* new_cluster=CreateZnode(cluster_name_,cluster_type_,stat_,data_);
        new_cluster->SetWatcherStat(watcher_stat_);
        CreateClusterInMap(cluster_name_,new_cluster,cluster_type_);
        pthread_mutex_unlock(&map_lock);

        return new_cluster;
    }
    pthread_mutex_lock(lock_it->second);
    //防止迭代器失效
    Znode* cluster_node=node_it->second;
    ClusterType cluster_type=type_it->second;
    pthread_mutex_t* cluster_lock=lock_it->second;
    pthread_mutex_unlock(&map_lock);

    bool flag=false;
    switch (cluster_type)
    {
    case High_Availability:
        flag=HighAvailabilityInsert(cluster_node,stat_,watcher_stat_,data_);
        break;

    case Load_Balance:
        flag=LoadBalanceInsert(cluster_node,stat_,watcher_stat_,data_);
        break;
    
    case High_Performance:
        flag=HighPerformanceInsert(cluster_node,stat_,watcher_stat_,data_);
        break;
    
    default:
        throw std::exception();
        break;
    }
    pthread_mutex_unlock(cluster_lock);

    return flag;
}

bool ZooKeeper::DeleteZnode(const std::string& cluster_name_, const std::string& stat_, const std::string& watcher_stat_)
{
    // printf("Delete Node Stat is %s\n",&stat_[0]);
    pthread_mutex_lock(&map_lock);
    if(unique_map.find(cluster_name_+stat_)==unique_map.end()){
        printf("delete unregister exception!\n");
        pthread_mutex_unlock(&map_lock);
        throw std::exception();
    }
    unique_map.erase(cluster_name_+stat_);
    std::unordered_map<std::string,ClusterType>::iterator type_it=type_map.find(cluster_name_);
    std::unordered_map<std::string,Znode*>::iterator node_it=node_map.find(cluster_name_);
    std::unordered_map<std::string,pthread_mutex_t*>::iterator lock_it=lock_map.find(cluster_name_);
    if(type_it==type_map.end()||node_it==node_map.end()){
        pthread_mutex_unlock(&map_lock);
        printf("DeleteZnode exception!\n");
        throw std::exception();//没有该集群
    }

    printf("Cluster Node Stat is %s\n",&node_it->second->GetStat()[0]);

    //若要删除节点为当前集群主节点,则应更新主节点
    bool is_lock_cluster=false;//标识是否由于主节点更新而上锁
    if(node_it->second->GetStat()==stat_){
        pthread_mutex_lock(lock_it->second);
        is_lock_cluster=true;
        Znode* update_node=UpdateClusterZnode(node_it->second,type_it->second);
        // printf("UpdateNode Stat is %s\n",&update_node->GetStat()[0]);
        if(update_node==node_it->second){
            //只有一个节点,删除集群
            DeleteClusterInMap(cluster_name_);
            pthread_mutex_unlock(&map_lock);
            update_node->SetWatcherStat(watcher_stat_);
            update_node->Notify();
            delete update_node;
            printf("delete cluster!\n");

            return true;
        }else{
            // printf("Update ClusterNode! ClusterNode Stat is %s\n",&update_node->GetStat()[0]);
            node_it->second=update_node;//更新主节点完毕
        }
    }

    if(!is_lock_cluster)pthread_mutex_lock(lock_it->second);//未上锁则上锁
    //防止迭代器失效
    Znode* cluster_node=node_it->second;
    ClusterType cluster_type=type_it->second;
    pthread_mutex_t* cluster_lock=lock_it->second;
    pthread_mutex_unlock(&map_lock);

    Znode* delete_node=nullptr;
    switch (cluster_type)
    {
    case High_Availability:
        delete_node=HighAvailabilityDelete(cluster_node,stat_);
        break;
    
    case Load_Balance:
    printf("1111111\n");
        delete_node=LoadBalanceDelete(cluster_node,stat_);
        printf("1111112\n");
        break;

    case High_Performance:
        delete_node=HighPerformanceDelete(cluster_node,stat_);
        break;

    default:
        return false;
        throw std::exception();
        break;
    }
    pthread_mutex_unlock(cluster_lock);
    delete_node->SetWatcherStat(watcher_stat_);
    delete_node->Notify();

    delete delete_node;

    return true;
}

// ZooKeeper::Znode* ZooKeeper::FindClusterZnode(const std::string& cluster_name, bool update_){

//     std::unordered_map<std::string,ClusterType>::iterator type_it=type_map.find(cluster_name);
//     std::unordered_map<std::string,Znode*>::iterator node_it=node_map.find(cluster_name);
//     std::unordered_map<std::string,pthread_mutex_t*>::iterator lock_it=cluster_lock.find(cluster_name);
//     if(type_it==type_map.end()||node_it==node_map.end()||lock_it==cluster_lock.end()){
//         throw std::exception();//异常
//         //pthread_mutex_unlock(&map_lock);
//         return nullptr;
//     }
//     return node_it->second;
// }

bool ZooKeeper::HighAvailabilityInsert(Znode* cluster_node_, const std::string& stat_, const std::string& watcher_stat_, const std::string& data_){
    return false;
}

bool ZooKeeper::LoadBalanceInsert(Znode* cluster_node_, const std::string& stat_, const std::string& watcher_stat_, const std::string& data_)
{
    ZnodeLB* node=new ZnodeLB(cluster_node_->GetCluster(),stat_,data_);
    node->SetWatcherStat(watcher_stat_);
    cluster_node_->Insert(node);

    return node;
}

bool ZooKeeper::HighPerformanceInsert(Znode* cluster_node_, const std::string& stat_, const std::string& Watcher_stat_, const std::string& data_){
    return false;
}

ZooKeeper::Znode* ZooKeeper::HighAvailabilityDelete(Znode* cluster_node_, const std::string& stat_){

    return nullptr;
}

ZooKeeper::Znode* ZooKeeper::LoadBalanceDelete(Znode* cluster_node_, const std::string& stat_){

    Znode* aim_node=cluster_node_->Find(stat_);
    if(aim_node){
        cluster_node_->Delete(aim_node);
    }else{
        return nullptr;
        throw std::exception();//未找到
    }

    return aim_node;
}

ZooKeeper::Znode* ZooKeeper::HighPerformanceDelete(Znode* cluster_node_, const std::string& stat_){

    return nullptr;
}


ZooKeeper::Znode::Znode(const std::string& cluster_name_, const std::string& stat_, const std::string& data_):cluster_name(cluster_name_),stat(stat_),data(data_){};

ZooKeeper::Znode::~Znode(){};

ZooKeeper::ZnodeHA::ZnodeHA(const std::string& cluster_name_, const std::string& stat_, const std::string& data_):Znode(cluster_name_,stat_,data_){
    // this->pre=this;
    // this->next=this;
}

ZooKeeper::ZnodeLB::ZnodeLB(const std::string& cluster_name_, const std::string& stat_, const std::string& data_):Znode(cluster_name_,stat_,data_){
    this->pre=this;
    this->next=this;
}

ZooKeeper::ZnodeHP::ZnodeHP(const std::string& cluster_name_, const std::string& stat_, const std::string& data_):Znode(cluster_name_,stat_,data_){
    // this->pre=this;
    // this->next=this;
}

ZooKeeper::Znode* ZooKeeper::HighAvailabilityUpdate(Znode* cluster_node){

    return nullptr;
}

ZooKeeper::Znode* ZooKeeper::LoadBalanceUpdate(Znode* cluster_node){
    return cluster_node->Update();
}

ZooKeeper::Znode* ZooKeeper::HighPerformanceUpdate(Znode* cluster_node){

    return nullptr;
}

ZooKeeper::ZnodeHA::~ZnodeHA(){};

ZooKeeper::ZnodeLB::~ZnodeLB(){};

ZooKeeper::ZnodeHP::~ZnodeHP(){};

std::string ZooKeeper::Znode::GetCluster(){
    return cluster_name;
}

std::string ZooKeeper::Znode::GetStat(){
    return stat;
}

std::string ZooKeeper::Znode::GetData(){
    return data;
}

ZooKeeper::Znode* ZooKeeper::ZnodeLB::GetPre(){
    return this->pre;
}

ZooKeeper::Znode* ZooKeeper::ZnodeLB::GetNext(){
    return this->next;
}

bool ZooKeeper::ZnodeHA::Insert(Znode* next_){return false;}

bool ZooKeeper::ZnodeLB::Insert(Znode* next_){
    //在该节点之后插入一个新节点next_到双向循环链表

    ZnodeLB* next_lb=dynamic_cast<ZnodeLB*>(next_);
    if(next_lb==nullptr){
        printf("Insert exception!\n");
        throw std::exception();
        return false;
    }

    next_lb->next=this->next;
    this->next=next_lb;
    next_lb->next->pre=next_lb;
    next_lb->pre=this;

    return true;
}

bool ZooKeeper::ZnodeHP::Insert(Znode* next_){return false;}

bool ZooKeeper::ZnodeHA::Delete(Znode* aim_node){return false;}

bool ZooKeeper::ZnodeLB::Delete(Znode* aim_node){
    //将该节点从双向循环链表中摘除

    ZnodeLB* node=this;
    while(node!=aim_node){
        node=node->next;
        if(node==this){
            //未找到节点
            printf("Delete exception!\n");
            throw std::exception();
        }
    }

    if(node->next==node){
        //只有一个节点
        node->pre=node->next=nullptr;
    }else{
        node->pre->next=node->next;
        node->next->pre=node->pre;
    }

    return true;
}

bool ZooKeeper::ZnodeHP::Delete(Znode* aim_node){return false;}

ZooKeeper::Znode* ZooKeeper::ZnodeHA::Find(const std::string& stat_){return nullptr;}

ZooKeeper::Znode* ZooKeeper::ZnodeLB::Find(const std::string& stat_){

    ZnodeLB* aim_node=this;
    while(aim_node->GetStat()!=stat_){
        aim_node=aim_node->next;
        if(aim_node==this){
            //找不到
            printf("Find exception!\n");
            throw std::exception();
        }
    }

    return aim_node;
}

ZooKeeper::Znode* ZooKeeper::ZnodeHP::Find(const std::string& stat_){return nullptr;}

ZooKeeper::Znode* ZooKeeper::ZnodeHA::Update(){return nullptr;}

ZooKeeper::Znode* ZooKeeper::ZnodeLB::Update(){
    return this->next;
}

ZooKeeper::Znode* ZooKeeper::ZnodeHP::Update(){return nullptr;}

void ZooKeeper::Znode::Notify(){

    for(std::list<std::shared_ptr<Watcher>>::iterator it=watcher_list.begin();it!=watcher_list.end();it++){
        (*it)->Update(this->watcher_stat);
    }
    
}

bool ZooKeeper::Znode::AddWatcher(std::shared_ptr<Watcher> new_watcher){

    if(!new_watcher){
        printf("AddWatcher exception!\n");
        throw std::exception();
        return false;
    }

    watcher_list.push_back(new_watcher);

    return true;
}

bool ZooKeeper::Znode::SetWatcherStat(const std::string& watcher_stat_){
    this->watcher_stat=watcher_stat_;
    return true;
}

std::string ZooKeeper::GetClusterZnodeStat(const std::string& cluster_name, bool update_, std::shared_ptr<Watcher> new_watcher)
{
    pthread_mutex_lock(&map_lock);
    std::unordered_map<std::string,ClusterType>::iterator type_it=type_map.find(cluster_name);
    std::unordered_map<std::string,Znode*>::iterator node_it=node_map.find(cluster_name);
    std::unordered_map<std::string,pthread_mutex_t*>::iterator lock_it=lock_map.find(cluster_name);
    if(type_it==type_map.end()||node_it==node_map.end()||lock_it==lock_map.end()){
        printf("GetClusterZnodeStat exception!\n");
        pthread_mutex_unlock(&map_lock);
        return "";
        throw std::exception();//异常
    }
    std::string stat_=node_it->second->GetStat();
    if(new_watcher)node_it->second->AddWatcher(new_watcher);
    if(update_)node_it->second=UpdateClusterZnode(node_it->second,type_it->second);
    pthread_mutex_unlock(&map_lock);

    return stat_;
}

std::string ZooKeeper::GetClusterZnodeData(const std::string& cluster_name, bool update_, std::shared_ptr<Watcher> new_wacher){

    pthread_mutex_lock(&map_lock);
    std::unordered_map<std::string,ClusterType>::iterator type_it=type_map.find(cluster_name);
    std::unordered_map<std::string,Znode*>::iterator node_it=node_map.find(cluster_name);
    std::unordered_map<std::string,pthread_mutex_t*>::iterator lock_it=lock_map.find(cluster_name);
    if(type_it==type_map.end()||node_it==node_map.end()||lock_it==lock_map.end()){
        printf("GetClusterZnodeData exception!\n");
        throw std::exception();//异常
        //pthread_mutex_unlock(&map_lock);
        return "";
    }
    std::string data_=node_it->second->GetData();
    if(new_wacher)node_it->second->AddWatcher(new_wacher);
    if(update_)node_it->second=UpdateClusterZnode(node_it->second,type_it->second);
    pthread_mutex_unlock(&map_lock);

    return data_;
    // pthread_mutex_lock(&map_lock);
    // std::unordered_map<std::string,Znode*>::iterator node_it=node_map.find(cluster_name);
    // if(node_it==node_map.end()){
    //     pthread_mutex_unlock(&map_lock);
    //     return "";
    //     throw std::exception();
    // }else{
    //     std::string data_=node_it->second->GetData();
    //     pthread_mutex_unlock(&map_lock);

    //     return data_;
    // }
}

ZooKeeper::Znode* ZooKeeper::UpdateClusterZnode(Znode* cluster_node, ClusterType cluster_type){

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