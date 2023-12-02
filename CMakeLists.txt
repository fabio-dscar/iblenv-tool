cmake_minimum_required(VERSION 3.20)

project(iblenv VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

##################################
## Third party libs
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext)

message(STATUS ${GLFW_INCLUDE_DIR})

##################################
## OpenGL
find_package(OpenGL REQUIRED)

set(IBLENV_SOURCES
  src/util.cpp
  src/shader.cpp
  ${GLAD_SOURCES}
)

add_executable(iblenv src/main.cpp ${IBLENV_SOURCES})
target_include_directories(iblenv PUBLIC
  src
  ext  
  ext/tinyexr
  ${GLAD_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
)
target_link_libraries(iblenv PRIVATE
  OpenGL::GL
  glad
  ${ZLIB_LIBRARIES}
  ${GLFW_LIBRARIES}
)

add_custom_command(TARGET iblenv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/glsl ${CMAKE_BINARY_DIR}/glsl
)

set(RELEASE_FLAGS "-march=native" "-mtune=native") # "-flto")
target_compile_options(iblenv PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_FLAGS}>")