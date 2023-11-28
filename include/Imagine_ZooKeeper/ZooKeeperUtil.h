#ifndef IMAGINE_ZOOKEEPER_ZOOKEEPERUTIL_H
#define IMAGINE_ZOOKEEPER_ZOOKEEPERUTIL_H

#include <string>

namespace Imagine_ZooKeeper
{

class ZooKeeperUtil
{
 public:
    static std::string GenerateZNodeName(const std::string& cluster_name, const std::pair<std::string, std::string>& stat);

    static std::pair<std::string, std::string> GenerateZNodeStat(const std::string& ip, const std::string& port);
};


} // namespace Imagine_ZooKeeper


#endif