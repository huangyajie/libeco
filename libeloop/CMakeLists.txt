
ADD_DEFINITIONS(-Os -Wall  --std=gnu99 -g3 -Wmissing-declarations)

add_library(eloop SHARED eloop.c)

install(TARGETS eloop LIBRARY DESTINATION lib)
install(FILES eloop.h list.h  DESTINATION include)


