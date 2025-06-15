cmake_minimum_required(VERSION 3.22)
project(serialize)
set(CMAKE_CXX_STANDARD 17)


#取出当前目录中所有的源文件，存储到一个变量中
aux_source_directory(./ SRC)
add_library(serialize STATIC ${SRC})