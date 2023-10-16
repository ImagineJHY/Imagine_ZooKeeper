cmake_minimum_required(VERSION 3.0)
project(IMAGINE_ZOOKEEPER)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g  -std=c++11 -Wall -Werror")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -Wall -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g  -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 定义全局参数 IS_MAKE_PROJECT
# 若已定义，则当前project作为subModule
# 若未定义, 则当前project作为mainModule
# 若当前的任务是编译动态库, 则构建TARGET: init, prepare, build
# 若当前的任务是生成项目, 则不构建上述TARGET
if(NOT DEFINED IS_MAKE_PROJECT)
    message("[Constructing LIB][Imagine ZooKeeper]: Starting Imagine_ZooKeeper lib building, lib name is ${PROJECT_NAME}...")
    set(TARGET_LIB "Imagine_ZooKeeper")
    set(IS_MAKE_PROJECT false)
    set(IMAGINE_SYSTEM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Imagine_System)
    set(IMAGINE_LIB_WORKER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Imagine_System/worker)
    set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

    add_custom_target(
        init
        COMMAND python3 init.py
        echo "[Constructing LIB][Imagine ZooKeeper]: Starting Imagine_ZooKeeper lib target init building..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} # 在哪个目录下执行命令
    )
    if(EXISTS ${IMAGINE_SYSTEM_DIR}/CMakeLists.txt)
        add_custom_target(
            prepare
            echo "[Constructing LIB][Imagine ZooKeeper]: thirdparty Imagine_System exists, Starting Imagine_ZooKeeper lib target prepare building..."
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} # 在哪个目录下执行命令
        )
        add_dependencies(prepare system_prepare)
    else()
        message("[Constructing LIB][Imagine ZooKeeper]: thirdparty Imagine_System NOT exists, cin make init to init it!")
    endif()

    add_custom_target(
        build
        echo "[Constructing LIB][Imagine ZooKeeper]: Starting Imagine_ZooKeeper lib target build building..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} # 在哪个目录下执行命令
    )
    add_dependencies(build imagine_zookeeper)
    
    if(EXISTS ${IMAGINE_SYSTEM_DIR}/CMakeLists.txt)
        add_subdirectory(${IMAGINE_SYSTEM_DIR})
        message("[Constructing LIB][Imagine ZooKeeper]: thirdparty Imagine_System exists, add subModule...")
    else()
        message("[Constructing LIB][Imagine ZooKeeper]: thirdparty Imagine_System NOT exists, cin make init to init it!")
    endif()
else()
    message("[Constructing PROJECT][Imagine ZooKeeper]: Starting Imagine_ZooKeeper lib building, lib name is ${PROJECT_NAME}...")
endif()

# 设置动态库的TARGET的头文件, 源文件, 依赖
file(GLOB IMAGINE_ZOOKEEPER_SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(imagine_zookeeper SHARED ${IMAGINE_ZOOKEEPER_SRC_LIST})
target_include_directories(imagine_zookeeper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(imagine_zookeeper pthread)

if(TARGET imagine_muduo)
    target_link_libraries(imagine_zookeeper imagine_muduo)
endif()