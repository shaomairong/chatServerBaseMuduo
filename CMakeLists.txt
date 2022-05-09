cmake_minimum_required(VERSION 3.0)#cmake要求最低版本号
project(chat)#工程名字

# 配置编译选项
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

# 配置最终的可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 配置头文件的搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/server)
include_directories(${PROJECT_SOURCE_DIR}/include/server/db)
include_directories(${PROJECT_SOURCE_DIR}/include/server/model)  
include_directories(${PROJECT_SOURCE_DIR}/include/server/redis)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)#第三方库json.hpp放在thirdparty文件夹里面，所以这里要配置头文件搜索路径

link_directories(/usr/lib64/mysql)
# link_directories(/usr/local/lib/)

# 加载子目录： 因为我们工程根目录下是没有代码需要编译的，要编译的源码都在src目录下面；
#所以这里需要加载子目录；当项目构建的时候，看到这里相应的配置项后，又去下面的子目录寻找CMakeLists.txt
add_subdirectory(src)