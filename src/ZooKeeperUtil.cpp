#include "Imagine_ZooKeeper/ZooKeeperUtil.h"

namespace Imagine_ZooKeeper
{

std::string ZooKeeperUtil::GenerateZNodeName(const std::string& cluster_name, const std::pair<std::string, std::string>& stat)
{
    return "/" + cluster_name + "/" + stat.first + ":" + stat.second;
}

std::pair<std::string, std::string> ZooKeeperUtil::GenerateZNodeStat(const std::string& ip, const std::string& port)
{
    return std::make_pair(ip, port);
}

} // namespace Imagine_ZooKeeper
