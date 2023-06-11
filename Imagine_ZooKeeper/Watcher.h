#ifndef IMAGINE_WATCHER_H
#define IMAGINE_WATCHER_H

#include<string>

namespace Imagine_ZooKeeper{

class Watcher{

public:
    virtual void Update(const std::string& send_)=0;

    virtual ~Watcher(){};
};


}



#endif