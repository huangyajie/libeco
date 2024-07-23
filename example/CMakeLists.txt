
include_directories(${PROJECT_SOURCE_DIR}/eco)



add_executable(example_eco example_eco.c)
add_executable(example_eco_echocli example_eco_echocli.c)
add_executable(example_eco_echosrv example_eco_echosrv.c)
add_executable(example_eco_echosrv_poll example_eco_echosrv_poll.c)

target_link_libraries(example_eco eco)
target_link_libraries(example_eco_echocli eco)
target_link_libraries(example_eco_echosrv eco)
target_link_libraries(example_eco_echosrv_poll eco)

INSTALL(TARGETS example_eco RUNTIME DESTINATION bin)
INSTALL(TARGETS example_eco_echocli RUNTIME DESTINATION bin)
INSTALL(TARGETS example_eco_echosrv RUNTIME DESTINATION bin)
INSTALL(TARGETS example_eco_echosrv_poll RUNTIME DESTINATION bin)
