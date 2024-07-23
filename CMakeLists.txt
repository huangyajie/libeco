cmake_minimum_required(VERSION 3.12)

project(eco C)


ADD_SUBDIRECTORY(libeloop)

include_directories(libeloop)
set(SOURCE_LIST_ECO eco/eco.c eco/eco_socket.c )
add_library(eco SHARED ${SOURCE_LIST_ECO})
target_link_libraries(eco eloop)

ADD_SUBDIRECTORY(example)

install(TARGETS eco LIBRARY DESTINATION lib)
install(FILES eco/eco.h eco/eco_socket.h  DESTINATION include)
