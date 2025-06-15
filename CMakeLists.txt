cmake_minimum_required(VERSION 3.22)
project(ddz_server)

set(CMAKE_CXX_STANDARD 17)

#添加头文件路径
include_directories(/usr/include/mysql)
include_directories(
        ${PROJECT_SOURCE_DIR}/common
        ${PROJECT_SOURCE_DIR}/http
        ${PROJECT_SOURCE_DIR}/reactor
        ${PROJECT_SOURCE_DIR}/tcp
        ${PROJECT_SOURCE_DIR}/thread
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/serialize
        ${PROJECT_SOURCE_DIR}/crypto
        ${PROJECT_SOURCE_DIR}/game
        ${PROJECT_SOURCE_DIR}/database
        "/usr/local/ssl/include/"
)


# 添加子目录
add_subdirectory(common)
add_subdirectory(http)
add_subdirectory(reactor)
add_subdirectory(tcp)
add_subdirectory(thread)
add_subdirectory(serialize)
add_subdirectory(crypto)
add_subdirectory(game)
add_subdirectory(database)

add_subdirectory(test)

#link_directories(/usr/local/ssl/lib64/)

#链接子目录中生成的静态库文件
link_libraries(tcp http reactor thread test serialize mycrypto game database common)

#指定可执行文件生成对应的目录中
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(ddz_server main.cpp)

#target_link_libraries(ddz_server
#        tcp
#        http
#        reactor
#        thread
#        common
#)
