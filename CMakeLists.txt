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

# --- Examples (conditional on optional event-loop libraries) ---
option(BUILD_EXAMPLES "Build example programs (requires optional event loop libraries when present)" ON)
if(BUILD_EXAMPLES)
  set(EXAMPLE_TARGETS "")
  # Helper macro to add warning relax for example code
  macro(relax_example_warnings target)
    target_compile_options(${target} PRIVATE
      -Wno-error=unused-parameter
      -Wno-error=maybe-uninitialized
      -Wno-error=uninitialized
      -Wno-error=unused-variable
      -Wno-error=unused-function
    )
  endmacro()

  # OpenSSL + kTLS support (optional)
  find_package(OpenSSL)
  if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found - TLS/kTLS examples will be fully functional")
  else()
    message(STATUS "OpenSSL not found - TLS examples will run without encryption")
  endif()

  # epoll (native on Linux, header always present)
  if(UNIX AND NOT APPLE)
    add_executable(epoll_http2_kv examples/epoll_http2_kv.c)
    target_include_directories(epoll_http2_kv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(epoll_http2_kv PRIVATE http2)
    if(OpenSSL_FOUND)
      target_link_libraries(epoll_http2_kv PRIVATE OpenSSL::SSL OpenSSL::Crypto)
      target_compile_definitions(epoll_http2_kv PRIVATE HAVE_OPENSSL=1)
    endif()
    relax_example_warnings(epoll_http2_kv)
    list(APPEND EXAMPLE_TARGETS epoll_http2_kv)
    message(STATUS "Added example: epoll_http2_kv")
  endif()

  # liburing
  find_library(URING_LIB uring)
  find_path(URING_INCLUDE liburing.h)
  if(URING_LIB AND URING_INCLUDE)
    add_executable(liburing_http2_kv examples/liburing_http2_kv.c)
    target_include_directories(liburing_http2_kv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${URING_INCLUDE})
    target_link_libraries(liburing_http2_kv PRIVATE http2 ${URING_LIB})
    if(OpenSSL_FOUND)
      target_link_libraries(liburing_http2_kv PRIVATE OpenSSL::SSL OpenSSL::Crypto)
      target_compile_definitions(liburing_http2_kv PRIVATE HAVE_OPENSSL=1)
    endif()
    relax_example_warnings(liburing_http2_kv)
    list(APPEND EXAMPLE_TARGETS liburing_http2_kv)
    message(STATUS "Added example: liburing_http2_kv")
  else()
    message(STATUS "Skipping liburing_http2_kv: liburing not found")
  endif()

  # libev
  find_library(EV_LIB ev)
  find_path(EV_INCLUDE ev.h)
  if(EV_LIB AND EV_INCLUDE)
    add_executable(libev_http2_kv examples/libev_http2_kv.c)
    target_include_directories(libev_http2_kv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${EV_INCLUDE})
    target_link_libraries(libev_http2_kv PRIVATE http2 ${EV_LIB})
    if(OpenSSL_FOUND)
      target_link_libraries(libev_http2_kv PRIVATE OpenSSL::SSL OpenSSL::Crypto)
      target_compile_definitions(libev_http2_kv PRIVATE HAVE_OPENSSL=1)
    endif()
    relax_example_warnings(libev_http2_kv)
    list(APPEND EXAMPLE_TARGETS libev_http2_kv)
    message(STATUS "Added example: libev_http2_kv")
  else()
    message(STATUS "Skipping libev_http2_kv: libev not found")
  endif()

  # libuv
  find_library(UV_LIB NAMES uv libuv)
  find_path(UV_INCLUDE uv.h)
  if(UV_LIB AND UV_INCLUDE)
    add_executable(libuv_http2_kv examples/libuv_http2_kv.c)
    target_include_directories(libuv_http2_kv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${UV_INCLUDE})
    target_link_libraries(libuv_http2_kv PRIVATE http2 ${UV_LIB})
    target_compile_definitions(libuv_http2_kv PRIVATE _GNU_SOURCE)
    if(OpenSSL_FOUND)
      target_link_libraries(libuv_http2_kv PRIVATE OpenSSL::SSL OpenSSL::Crypto)
      target_compile_definitions(libuv_http2_kv PRIVATE HAVE_OPENSSL=1)
    endif()
    relax_example_warnings(libuv_http2_kv)
    list(APPEND EXAMPLE_TARGETS libuv_http2_kv)
    message(STATUS "Added example: libuv_http2_kv")
  else()
    message(STATUS "Skipping libuv_http2_kv: libuv not found")
  endif()

  if(EXAMPLE_TARGETS)
    add_custom_target(examples DEPENDS ${EXAMPLE_TARGETS})
    message(STATUS "Created 'examples' target")
  endif()
endif()