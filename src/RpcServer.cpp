#include "RpcServer.h"

#include <sys/uio.h>
#include <cstdarg>

using namespace Imagine_Rpc;

RpcServer::RpcServer(const std::string &ip, const std::string &port, const std::string &keeper_ip, const std::string &keeper_port, int max_client_num)
                     : ip_(ip), port_(port), keeper_ip_(keeper_ip), keeper_port_(keeper_port)
{
    int temp_port = Rpc::StringToInt(port_);
    if (temp_port < 0) {
        throw std::exception();
    }

    if (pthread_mutex_init(&callback_lock_, nullptr) != 0) {
        throw std::exception();
    }

    if (pthread_mutex_init(&heart_map_lock_, nullptr) != 0) {
        throw std::exception();
    }

    // port = Rpc::IntToString(port_);

    SetDefaultReadCallback();

    SetDefaultWriteCallback();

    SetDefaultCommunicateCallback();

    SetDefaultTimerCallback();

    loop_ = new EventLoop(temp_port, max_client_num, read_callback_, write_callback_, communicate_callback_);
}

RpcServer::RpcServer(const std::string &ip, const std::string &port, std::unordered_map<std::string, RpcCallback> callbacks, const std::string &keeper_ip, const std::string &keeper_port, int max_client_num)
                     : ip_(ip), port_(port), keeper_ip_(keeper_ip), keeper_port_(keeper_port)
{
    int temp_port = Rpc::StringToInt(port_);
    if (temp_port <= 0) {
        throw std::exception();
    }

    if (pthread_mutex_init(&callback_lock_, nullptr) != 0) {
        throw std::exception();
    }

    // 设置本机IP+PORT(均用字符串进行表示和传输)
    // port = htons(port_);
    //  port = Rpc::IntToString(port_);
    // inet_pton(AF_INET,"192.168.83.129",&ip);

    SetDefaultReadCallback();

    SetDefaultWriteCallback();

    SetDefaultCommunicateCallback();

    SetDefaultTimerCallback();

    loop_ = new EventLoop(temp_port, max_client_num, read_callback_, write_callback_, communicate_callback_);

    callback_num_ = callbacks.size();
    pthread_mutex_lock(&callback_lock_);
    for (auto it = callbacks.begin(); it != callbacks.end(); it++) {
        // Callee(it->first,it->second);
        callbacks_.insert(std::make_pair(it->first, it->second));
        // callbacks_.insert({it->first,it->second});
        if (keeper_ip_.size() && keeper_port_.size())
            Register(it->first, keeper_ip_, keeper_port_);
    }
    pthread_mutex_unlock(&callback_lock_);
    // register_loop_=new EventLoop(0,max_register_num,)
    // loop->loop();
}

RpcServer::~RpcServer()
{
    delete loop_;
}

bool RpcServer::SetKeeper(const std::string &keeper_ip, const std::string &keeper_port)
{
    keeper_ip_ = keeper_ip;
    keeper_port_ = keeper_port;

    return true;
}

void RpcServer::SetDefaultReadCallback()
{
    read_callback_ = [this](const struct iovec *input_iovec)
    {
        printf("this is RpcServer : %s\n", &(ip_ + port_)[0]);

        std::string input = Rpc::GetIovec(input_iovec);

        int sockfd = *(int *)input_iovec[0].iov_base;

        // 解析数据
        std::vector<std::string> de_input = Rpc::Deserialize(input, 0);

        std::string content = de_input[1] + "\r\n";

        // 搜索函数
        auto func = this->SearchFunc(de_input[1]);
        if (!func) {
            // 函数未找到
            struct iovec *output_iovec = Rpc::SetIovec(Rpc::GenerateDefaultFailureMessage(), 12, false);
            return output_iovec;
        }
        Rpc::Unpack(de_input);

        // 执行并输出
        content += Rpc::Serialize(func(de_input));

        std::string head = Rpc::GenerateDefaultHead(content);

        // 暂时默认保持连接
        UpdatetUser(sockfd);

        struct iovec *output_iovec = Rpc::SetIovec(head + content, head.size() + content.size());

        return output_iovec;
    };
    if (loop_) {
        loop_->SetWriteCallback(write_callback_);
    }
}

void RpcServer::SetDefaultWriteCallback()
{
    write_callback_ = [](const struct iovec *input_iovec)
    {
        std::string input = Rpc::GetIovec(input_iovec);
        struct iovec *output_iovec = Rpc::SetIovec(input, input.size());
        *((char *)output_iovec[0].iov_base + 4) = '1';
        *((char *)output_iovec[0].iov_base + 5) = '0';

        return output_iovec;
    };
    if (loop_) {
        loop_->SetWriteCallback(write_callback_);
    }
}

void RpcServer::SetDefaultCommunicateCallback()
{
    communicate_callback_ = Rpc::DefaultCommunicateCallback;
    if (loop_) {
        loop_->SetWriteCallback(write_callback_);
    }
}

void RpcServer::SetDefaultTimerCallback()
{
    timer_callback_ = [this](int sockfd, double time_out)
    {
        // printf("RpcServer TimerCallback!\n");

        long long last_request_time;
        if (!GetHeartNodeInfo(sockfd, last_request_time)) {
            return;
        }

        if (TimeUtil::GetNow() > TimeUtil::MicroSecondsAddSeconds(last_request_time, time_out)) {
            // 已过期
            printf("RpcServer Timer Set offline!\n");
            this->loop_->Closefd(sockfd);
            this->DeleteUser(sockfd);
            return;
        } else {
            // 未过期,忽略
        }
    };
}

void RpcServer::SetDefaultTimeOutCallback()
{
    timeout_callback_ = [](void) -> void {

    };
}

RpcServerTimerCallback RpcServer::GetTimerCallback()
{
    return timer_callback_;
}

void RpcServer::loop()
{
    loop_->loop();
}

bool RpcServer::UpdatetUser(int sockfd)
{
    pthread_mutex_lock(&heart_map_lock_);
    std::unordered_map<int, RpcSHeart *>::iterator it = heart_map_.find(sockfd);
    if (it == heart_map_.end()) {
        RpcSHeart *new_heart = new RpcSHeart(SetTimer(5.0, 0.0, std::bind(timer_callback_, sockfd, time_out_)));
        heart_map_.insert(std::make_pair(sockfd, new_heart));
    } else {
        it->second->ReSetLastRequestTime();
    }
    pthread_mutex_unlock(&heart_map_lock_);

    return true;
}

bool RpcServer::DeleteUser(int sockfd)
{
    pthread_mutex_lock(&heart_map_lock_);
    std::unordered_map<int, RpcSHeart *>::iterator it = heart_map_.find(sockfd);
    if (it == heart_map_.end()) {
        return false; // 已删除
        throw std::exception();
    }
    RpcSHeart *heart_node = it->second;
    heart_map_.erase(it);
    RemoveTimer(heart_node->GetTimerfd());
    delete heart_node;
    pthread_mutex_unlock(&heart_map_lock_);

    return true;
}

void RpcServer::Callee(const std::string &method, RpcCallback callback)
{ // 服务器注册函数
    pthread_mutex_lock(&callback_lock_);
    if (keeper_ip_.size() && keeper_port_.size()) {
        Register(method, keeper_ip_, keeper_port_); // 在服务器上注册name:ip_port
    }
    callbacks_.insert(std::make_pair(method, callback));
    pthread_mutex_unlock(&callback_lock_);
}

bool RpcServer::Register(const std::string &method, const std::string &keeper_ip, const std::string &keeper_port)
{
    struct sockaddr_in addr = Rpc::PackIpPort(keeper_ip, keeper_port);
    // struct sockaddr_in addr=Rpc::PackIpPort(keeper_ip,keeper_port);
    std::string content = GenerateDefaultRpcKeeperContent("Register", method);
    std::string head = Rpc::GenerateDefaultHead(content);
    int sockfd;
    while (1) {
        if (Rpc::Connect(keeper_ip, keeper_port, &sockfd)) {
            if (Rpc::Deserialize(Rpc::Communicate(head + content, &sockfd))[1] == "Success") {
                printf("Server %s Register Success!\n", &(ip_ + port_)[0]);
                Rpc::DefaultKeepAliveClient(loop_, std::bind(&Rpc::DefaultClientTimerCallback, sockfd, method, nullptr));

                return true;
            } else {
                close(sockfd);
            }
        }
        printf("Register unsuccess!try again after 5 second!\n");
        sleep(5);
    }

    printf("Register exception!\n");
    throw std::exception();

    return false;
}

bool RpcServer::DeRegister(const std::string &method, const std::string &keeper_ip, const std::string &keeper_port)
{
    struct sockaddr_in addr = Rpc::PackIpPort(keeper_ip, keeper_port);
    // struct sockaddr_in addr=Rpc::PackIpPort(keeper_ip,keeper_port);

    std::string content = GenerateDefaultRpcKeeperContent("DeRegister", method);
    std::string head = Rpc::GenerateDefaultHead(content);
    if (Rpc::Deserialize(Rpc::Communicate(head + content, &addr, true), 0)[1] == "Success") {
        printf("Server %s Deregister Success!\n", &(ip_ + port_)[0]);
        return true;
    }

    printf("DeRegister exception!\n");
    throw std::exception();

    return false;
}

std::string RpcServer::GenerateDefaultRpcKeeperContent(const std::string &option, const std::string &method)
{
    return "RpcServer\r\n" + option + "\r\n" + method + "\r\n" + ip_ + "\r\n" + port_ + "\r\n";
}

RpcCallback RpcServer::SearchFunc(std::string method)
{
    pthread_mutex_lock(&callback_lock_);
    auto it = callbacks_.find(method);
    if (it == callbacks_.end()) {
        // 没找到
        printf("SearchFunc exception!\n");
        throw std::exception();
    }
    auto callback = it->second;
    pthread_mutex_unlock(&callback_lock_);

    return callback;
}

long long RpcServer::SetTimer(double interval, double delay, RpcTimerCallback callback)
{
    return loop_->SetTimer(callback, interval, delay);
}

bool RpcServer::RemoveTimer(long long timerfd)
{
    return loop_->CloseTimer(timerfd);
}

bool RpcServer::GetHeartNodeInfo(int sockfd, long long &last_request_time)
{
    pthread_mutex_lock(&heart_map_lock_);
    std::unordered_map<int, RpcSHeart *>::iterator it = heart_map_.find(sockfd);
    if (it == heart_map_.end()) {
        // 已删除
        pthread_mutex_unlock(&heart_map_lock_);
        return false;
    }
    last_request_time = it->second->GetLastRequestTime();
    pthread_mutex_unlock(&heart_map_lock_);

    return true;
}

long long RpcServer::GetHeartNodeLastRequestTime(int sockfd)
{
    pthread_mutex_lock(&heart_map_lock_);
    std::unordered_map<int, RpcSHeart *>::iterator it = heart_map_.find(sockfd);
    if (it == heart_map_.end()) {
        // 已删除
        pthread_mutex_unlock(&heart_map_lock_);
        return -1;
    }
    long long last_request_time = it->second->GetLastRequestTime();
    pthread_mutex_unlock(&heart_map_lock_);

    return last_request_time;
}