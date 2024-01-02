cmake_minimum_required (VERSION 3.20)

include(FetchContent)

# ---------------------------------------------------------------------------------------
#     zlib
# ---------------------------------------------------------------------------------------
FetchContent_Declare(
  ZLIB
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG        09155eaa2f9270dc4ed1fa13e2b4b2613e6e4851
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

# ---------------------------------------------------------------------------------------
#     OpenGL
# ---------------------------------------------------------------------------------------
find_package(OpenGL REQUIRED)

set(OPENGL_LIBRARIES ${OPENGL_LIBRARIES} PARENT_SCOPE)

# ---------------------------------------------------------------------------------------
#     glad
# ---------------------------------------------------------------------------------------
set(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glad/include PARENT_SCOPE)
set(GLAD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/glad/src/glad.c PARENT_SCOPE)

add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/src/glad.c)

target_include_directories(glad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)

set_property(TARGET glad PROPERTY FOLDER "ext")

# ---------------------------------------------------------------------------------------
#     glfw
# ---------------------------------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL " " FORCE)
set(GLFW_INSTALL OFF CACHE BOOL " " FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        d097e35743ee49d38811a89ea29c7f2525dc8d6a
)

FetchContent_MakeAvailable(glfw)

set(GLFW_INCLUDE_DIR ${glfw_SOURCE_DIR}/include PARENT_SCOPE)
set(GLFW_LIBRARIES glfw PARENT_SCOPE)

# ---------------------------------------------------------------------------------------
#     glm
# ---------------------------------------------------------------------------------------
set(BUILD_TESTING OFF CACHE BOOL " " FORCE)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        586a402397dd35d66d7a079049856d1e2cbab300
  FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(glm)

if (GLM_FOUND)
  set(GLM_INCLUDE_DIR ${GLM_INCLUDE_DIRS} PARENT_SCOPE)
  set(GLM_LIBRARIES ${GLM_LIBRARIES} PARENT_SCOPE)
else()
  set(GLM_INCLUDE_DIR ${glm_SOURCE_DIR} PARENT_SCOPE)
  set(GLM_LIBRARIES glm PARENT_SCOPE)
endif()