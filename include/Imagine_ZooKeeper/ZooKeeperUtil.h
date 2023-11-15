#ifndef IMAGINE_ZOOKEEPER_ZOOKEEPERUTIL_H
#define IMAGINE_ZOOKEEPER_ZOOKEEPERUTIL_H

#include <string>

namespace Imagine_ZooKeeper
{

class ZooKeeperUtil
{
 public:
    static std::string GenerateZNodeName(std::string cluster_name, std::pair<std::string, std::string> stat);
};


} // namespace Imagine_ZooKeeper


#endif