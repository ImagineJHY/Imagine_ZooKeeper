#include "Imagine_ZooKeeper/ZooKeeperUtil.h"

namespace Imagine_ZooKeeper
{

std::string ZooKeeperUtil::GenerateZNodeName(std::string cluster_name, std::pair<std::string, std::string> stat)
{
    return "/" + cluster_name + "/" + stat.first + ":" + stat.second;
}

} // namespace Imagine_ZooKeeper
