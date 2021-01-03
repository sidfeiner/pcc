cmake_minimum_required(VERSION 3.17)
project(pcc C)

set(CMAKE_C_STANDARD 11)

add_executable(client pcc_client.c)
add_executable(pcc_server.c)