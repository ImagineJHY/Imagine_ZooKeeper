#ifndef IMAGINE_ZOOKEEPER_LOG_MACRO_H
#define IMAGINE_ZOOKEEPER_LOG_MACRO_H

#ifdef OPEN_IMAGINE_ZOOKEEPER_LOG
#define IMAGINE_MUDUO_LOG(LOG_MESSAGE...) \
    do { \
        LOG_INFO(LOG_MESSAGE); \
    } while(0)
#else
#define IMAGINE_ZOOKEEPER_LOG(LOG_MESSAGE...) do { } while(0)
#endif

#endif