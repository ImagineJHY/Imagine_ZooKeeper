#ifndef IMAGINE_ZOOKEEPER_WATCHER_H
#define IMAGINE_ZOOKEEPER_WATCHER_H

#include <string>

namespace Imagine_ZooKeeper
{

class Watcher
{
 public:
    virtual void Update(const std::string &send_content) = 0;

    virtual ~Watcher(){};
};

} // namespace Imagine_ZooKeeper

#endif