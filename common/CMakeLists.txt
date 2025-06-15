cmake_minimum_required(VERSION 3.22)
project(common)
set(CMAKE_CXX_STANDARD 17)


link_directories(/usr/local/lib)


#取出当前目录中所有的源文件，存储到一个变量中
aux_source_directory(./ SRC)
add_library(common STATIC ${SRC})
target_link_libraries(common jsoncpp)
