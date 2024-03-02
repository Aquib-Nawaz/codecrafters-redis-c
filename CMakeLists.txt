cmake_minimum_required(VERSION 3.26)
project(codecrafters_redis_c C)

set(CMAKE_C_STANDARD 11)
add_executable(codecrafters_redis_c
        app/server.c
        app/hashset.c
        app/parser.c
        app/message.c)

