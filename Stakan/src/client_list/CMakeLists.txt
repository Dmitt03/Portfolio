set(CLIENT_LIST client_list.cpp client_list.h)
add_library(client_list STATIC
    ${CLIENT_LIST}
)

target_include_directories(client_list PUBLIC 
${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_SOURCE_DIR}/src/snapshots_broadcaster)

target_link_libraries(client_list PUBLIC
    interface_bid_ask
    Boost::headers
)
