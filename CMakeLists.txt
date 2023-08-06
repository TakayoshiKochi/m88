cmake_minimum_required(VERSION 3.15)

project(m88 CXX)

# MSVC or Ninja on Windows
if (WIN32)
    set(LIB_TYPE STATIC)
    include_directories(${CMAKE_SOURCE_DIR}/third_party/gtest/googletest/include)
    link_directories(${CMAKE_SOURCE_DIR}/third_party/gtest/lib/x64)
endif (WIN32)

if (MSVC)
    set(CMAKE_CXX_STANDARD 17)
    add_compile_definitions(NOMINMAX)
    add_compile_options(/utf-8)

    # https://gitlab.kitware.com/cmake/community/wikis/FAQ#how-can-i-build-my-msvc-application-with-a-static-runtime
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG          ${CMAKE_CXX_FLAGS_DEBUG})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_MINSIZEREL     ${CMAKE_CXX_FLAGS_MINSIZEREL})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE        ${CMAKE_CXX_FLAGS_RELEASE})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
endif (MSVC)

#add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glog)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/gtest)
#add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/abseil)

add_library(common ${LIB_TYPE}
        src/common/device.h
        src/common/device.cpp
        src/common/diag.h
        src/common/diag.cpp
        src/common/draw.h
        src/common/error.h
        src/common/error.cpp
        src/common/io_bus.h
        src/common/io_bus.cpp
        src/common/memory_manager.h
        src/common/memory_manager.cpp
        src/common/memory_bus.h
        src/common/memory_bus.cpp
        src/common/misc.h
        src/common/scheduler.h
        src/common/scheduler.cpp
        src/common/soundsrc.h
        src/common/srcbuf.h
        src/common/srcbuf.h)

target_include_directories(common
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

add_library(pc88 ${LIB_TYPE}
        src/pc88/base.h
        src/pc88/base.cpp
        src/pc88/beep.h
        src/pc88/beep.cpp
)

target_include_directories(pc88
        PUBLIC ${CMAKE_SOURCE_DIR}/src
        PUBLIC ${CMAKE_SOURCE_DIR}/third_party)

add_executable(common_unittests
        src/common/device_test.cc
        src/common/scheduler_test.cc)

target_link_libraries(common_unittests
        PUBLIC gtest gtest_main
        common pc88)
