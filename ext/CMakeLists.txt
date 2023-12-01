cmake_minimum_required (VERSION 3.20)

include(FetchContent)

#########################################
## zlib
FetchContent_Declare(
  ZLIB
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG        09155eaa2f9270dc4ed1fa13e2b4b2613e6e4851 # v1.3
  FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(ZLIB)

if (ZLIB_FOUND)
  set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIRS} PARENT_SCOPE)
  set(ZLIB_LIBRARIES ${ZLIB_LIBRARIES} PARENT_SCOPE)
else()
  set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} PARENT_SCOPE)
  set(ZLIB_LIBRARIES zlibstatic PARENT_SCOPE)
endif()

#########################################
## glad
set(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glad/include PARENT_SCOPE)
add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/src/glad.c)

target_include_directories(glad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)

set_property(TARGET glad PROPERTY FOLDER "ext")
