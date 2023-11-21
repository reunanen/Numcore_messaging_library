cmake_minimum_required(VERSION 3.13)
project(samples)

add_subdirectory(.. NumcoreMessagingLibrary_build)

include_directories(
  ..
  ../Numcore_messaging_library
  )

add_executable(disk-space-logger disk-space-logger/disk-space-logger.cpp)
add_executable(influx-writer     influx-writer/influx-writer.cpp)

target_link_libraries(disk-space-logger NumcoreMessagingLibrary)
target_link_libraries(influx-writer     NumcoreMessagingLibrary curl)

target_compile_options(disk-space-logger PRIVATE -Wextra -Werror)
target_compile_options(influx-writer     PRIVATE -Wextra -Werror)
