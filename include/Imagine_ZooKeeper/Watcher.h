#ifndef IMAGINE_ZOOKEEPER_WATCHER_H
#define IMAGINE_ZOOKEEPER_WATCHER_H

#include <string>

namespace Imagine_ZooKeeper
{

class Watcher
{
 public:
    Watcher();

    virtual ~Watcher();

    virtual void Update(const std::string &send_content) const = 0;
};

} // namespace Imagine_ZooKeeper

#endif