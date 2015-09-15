# Boost asio UDT implementation

This project is a an implementation of the [UDT protocol](http://udt.sourceforge.net)
based on Boost asio components and philosophy (socket, timer, asynchronous model).

## How to use

The library complies with asio's API.

As you would have used TCP socket :
  * boost::asio::ip::tcp::socket
  * boost::asio::ip::tcp::acceptor
  * boost::asio::ip::tcp::resolver
  * boost::asio::ip::tcp::resolver::query
  
The library provides UDT socket :
  * ip::udt<>::socket
  * ip::udt<>::acceptor
  * ip::udt<>::resolver
  * ip::udt<>::resolver::query
  
[UDT protocol](/src/udt/ip/udt.h) can be customized with template parameters :
  * The first one is a Logger. By default there is no logging but it is possible
  to use a FileLogger which log internal variables for statistics. A [python 
  script](/tools/plot.py) is available to display logs as graphs.
  * The second one is the congestion algorithm. By default, this is the standard 
  congestion algorithm. Feel free to suggest and implement your own algorithm.

At the moment, this library does not support the rendez-vous connection feature.

Examples :
  * [client](./src/udt_client/main.cpp)
  * [server](./src/udt_server/main.cpp)

Feel free to contribute, leave feedbacks or report issues !

## How to build examples (client, server) or unit tests

### Requirements

  * Winrar >= 5.2.1 (Third party builds on windows)
  * Boost >= 1.56.0
  * Google Test >= 1.7.0
  * CMake >= 2.8.11
  * C++11 compiler (Visual Studio 2013, Clang, g++, etc.)
  
### Build test executables on Windows

* Go in project directory

```bash
cd PROJECT_PATH
```

* Copy [Boost archive](http://www.boost.org/users/download/) in ``third_party/boost``

```bash
cp boost_1_XX_Y.tar.bz2 PROJECT_PATH/third_party/boost
```

* CopyCopy [GTest archive](http://code.google.com/p/googletest/downloads/list) in ``third_party/gtest``

```bash
cp gtest-1.X.Y.zip PROJECT_PATH/third_party/gtest
```

* Generate project

```bash
mkdir PROJECT_PATH/build
cd PROJECT_PATH/build
cmake ../
```

* Build project

```bash
cd PROJECT_PATH/build
cmake --build . --config Debug|Release
```

### Build test executables on Linux

* Go in project directory

```bash
cd PROJECT_PATH
```

* Copy Boost archive in ``third_party/boost``

```bash
cp boost_1_XX_Y.tar.bz2 PROJECT_PATH/third_party/boost
```

* Copy GTest archive in ``third_party/gtest``

```bash
cp gtest-1.X.Y.zip PROJECT_PATH/third_party/gtest
```

* Generate project

```bash
mkdir PROJECT_PATH/build
cd PROJECT_PATH/build
cmake -DCMAKE_BUILD_TYPE=Release|Debug ../
```

* Build project

```bash
cmake --build PROJECT_PATH/build -- -j
```