cmake_minimum_required(VERSION 3.20)
project(libhttp2 LANGUAGES C VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_library(http2 STATIC src/http2.c)
add_library(http1 STATIC src/http1.c)
add_library(websocket STATIC src/websocket.c)

target_include_directories(http2 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_include_directories(http1 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_include_directories(websocket PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

include(GNUInstallDirs)
install(TARGETS http2 http1 websocket EXPORT http2-targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT http2-targets FILE http2-config.cmake NAMESPACE http2:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/http2)

enable_testing()
add_executable(http2_test tests/test_http2.c)
target_link_libraries(http2_test PRIVATE http2)
add_test(NAME http2_basic COMMAND http2_test)

add_executable(http1_websocket_test tests/test_http1_websocket.c)
target_link_libraries(http1_websocket_test PRIVATE http1 websocket)
add_test(NAME http1_websocket_dialectic COMMAND http1_websocket_test)

add_executable(websocket_ping_pong_test tests/test_websocket_ping_pong.c)
target_link_libraries(websocket_ping_pong_test PRIVATE websocket)
add_test(NAME websocket_ping_pong_dialectic COMMAND websocket_ping_pong_test)

add_executable(websocket_close_test tests/test_websocket_close.c)
target_link_libraries(websocket_close_test PRIVATE websocket)
add_test(NAME websocket_close_dialectic COMMAND websocket_close_test)

add_executable(websocket_fragmentation_test tests/test_websocket_fragmentation.c)
target_link_libraries(websocket_fragmentation_test PRIVATE websocket)
add_test(NAME websocket_fragmentation_dialectic COMMAND websocket_fragmentation_test)

add_executable(websocket_interleaved_test tests/test_websocket_interleaved.c)
target_link_libraries(websocket_interleaved_test PRIVATE websocket)
add_test(NAME websocket_interleaved_dialectic COMMAND websocket_interleaved_test)

add_executable(websocket_errors_test tests/test_websocket_errors.c)
target_link_libraries(websocket_errors_test PRIVATE websocket)
add_test(NAME websocket_errors_dialectic COMMAND websocket_errors_test)

add_executable(websocket_fragmentation_edges_test tests/test_websocket_fragmentation_edges.c)
target_link_libraries(websocket_fragmentation_edges_test PRIVATE websocket)
add_test(NAME websocket_fragmentation_edges COMMAND websocket_fragmentation_edges_test)

add_executable(websocket_fragmentation_precise_test tests/test_websocket_fragmentation_precise.c)
target_link_libraries(websocket_fragmentation_precise_test PRIVATE websocket)
add_test(NAME websocket_fragmentation_precise COMMAND websocket_fragmentation_precise_test)

add_executable(http1_errors_test tests/test_http1_errors.c)
target_link_libraries(http1_errors_test PRIVATE http1)
add_test(NAME http1_errors_dialectic COMMAND http1_errors_test)