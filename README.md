# Boost asio UDT implementation

## How to build

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
 cmake -G "GENERATOR" ../
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
cmake -G "GENERATOR" -DCMAKE_BUILD_TYPE=Release|Debug ../
```

* Build project

```bash
cmake --build PROJECT_PATH/build -- -j
```
