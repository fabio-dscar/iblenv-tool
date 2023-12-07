cmake_minimum_required(VERSION 3.20)

project(iblenv VERSION 0.1 LANGUAGES CXX C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

##################################
## Third party libs
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext)

set(IBLENV_SOURCES
  src/iblcli.cpp
  src/util.cpp
  src/shader.cpp
  src/geometry.cpp
  src/texture.cpp
  src/image.cpp
  src/parser.cpp
  ${GLAD_SOURCES}
)

add_executable(iblenv src/main.cpp ${IBLENV_SOURCES})
target_compile_features(iblenv PUBLIC cxx_std_20)
target_include_directories(iblenv PUBLIC
  src
  ext  
  ext/tinyexr
  ${GLAD_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
  ${GLM_INCLUDE_DIR}
)
target_link_libraries(iblenv PRIVATE
  glad
  ${OPENGL_LIBRARIES}
  ${ZLIB_LIBRARIES}
  ${GLFW_LIBRARIES}
)

add_custom_command(TARGET iblenv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/glsl ${CMAKE_BINARY_DIR}/glsl
)

set(RELEASE_FLAGS "-march=native" "-mtune=native") # "-flto")
target_compile_options(iblenv PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_FLAGS}>")