# Imagine_ZooKeeper
Imagine_ZooKeeper is a server registration center provading load balance and watcher function.

## Any Problems
If you find a bug, post an [issue](https://github.com/ImagineJHY/Imagine_ZooKeeper/issues)! If you have questions about how to use Imagine_ZooKeeper, feel free to shoot me an email at imaginejhy@163.com

## How to Build
Imagine_ZooKeeper uses [CMake](http://www.cmake.org) to support building. Install CMake before proceeding.

#### 1. Navigating into the source directory

#### 2. If it's your first time to build Imagine_ZooKeeper, excuting command 'make init' to init the thirdparty

#### 3. Excuting command 'make prepare' in the source directory

**Note:** If you know what are you doing, you can Navigating into the thirdparty directory and using command 'git checkout' choosing the correct commitId of thirdparty before excuting command 'make prepare'

#### 4. Excuting command 'make build' and Imagine_ZooKeeper builds a shared library by default