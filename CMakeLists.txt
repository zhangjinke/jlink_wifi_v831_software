cmake_minimum_required(VERSION 3.16)
include(GNUInstallDirs)

# 编译器配置
include(v831_setup.cmake)

# 工程配置
project(jlink VERSION 1.2.1 LANGUAGES C)
message(STATUS "PROJECT_VERSION=${PROJECT_VERSION}")
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# 设置编译标志
set(CMAKE_C_FLAGS "-fPIC -Wall -Wno-deprecated-declarations -Wno-format-truncation")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-O0 -g0 -s")
message(STATUS "CMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    message(STATUS "CMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "CMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}")
endif()
message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

# 设置安装路径
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/jlink")
message(STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")

# 设置运行时动态库搜索路径
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
message(STATUS "CMAKE_INSTALL_RPATH=${CMAKE_INSTALL_RPATH}")

# thread
find_package(Threads REQUIRED)
if(NOT Threads_FOUND)
    message(FATAL_ERROR "library pthread not found.")
endif()
list(APPEND LINK_LIBS ${CMAKE_THREAD_LIBS_INIT})


# libconfig
add_subdirectory(3rdparty/libconfig)
list(APPEND LINK_LIBS config)

# wpa_client
include(3rdparty/wpa_supplicant/CMakeLists.txt)
list(APPEND LINK_LIBS wpa_client)

# zlog
add_subdirectory(3rdparty/zlog/src)
list(APPEND LINK_LIBS zlog)

# jlink
configure_file(${CMAKE_SOURCE_DIR}/application/include/config.h.in
               ${CMAKE_BINARY_DIR}/config.h)
set(JLINK_SRC_FILES_C
    application/source/cfg.c
    application/source/jlink_ctl.c
    application/source/key.c
    application/source/led.c
    application/source/main.c
    application/source/udp_ctl.c
    application/source/web.c
    application/source/wifi_ctl.c
    utilities/source/c2000.c
    utilities/source/crc.c
    utilities/source/file.c
    utilities/source/filter.c
    utilities/source/gpio.c
    utilities/source/process.c
    utilities/source/str.c
    utilities/source/rngbuf.c
    utilities/source/systick.c
    utilities/source/utilities.c
)
add_executable(jlink ${JLINK_SRC_FILES_C})
target_include_directories(jlink PRIVATE application/include)
target_include_directories(jlink PRIVATE utilities/include)
target_include_directories(jlink PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(jlink PRIVATE ${LINK_LIBS})
message(STATUS "LINK_LIBS=${LINK_LIBS}")

install(TARGETS jlink
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(DIRECTORY etc
    DESTINATION .
)

install(DIRECTORY resource
    DESTINATION .
)

install(FILES start_jlink.sh
    DESTINATION .
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# 生成安装文件
add_custom_target(install_jlink ALL
    DEPENDS jlink
    COMMAND "rm" "-r" "jlink" "2>/dev/null;" "cmake" "--install" "."
)

# 显示大小
add_custom_target(show_size ALL
    DEPENDS install_jlink
    COMMAND "echo" "-ne" "\"\\e[0;33m\"" "&&" "size" "jlink/bin/jlink" "&&" "echo" "-ne" "\"\\e[0m\""
)

