cmake_minimum_required(VERSION 3.30)

include(GNUInstallDirs)

set(name ruis-render-opengl)

project(${name})

if(NOT TARGET ruis::ruis)
find_package(ruis CONFIG REQUIRED)
endif()

find_package(GLEW REQUIRED)

set(srcdir "${CMAKE_CURRENT_SOURCE_DIR}/../src/ruis")
file(GLOB_RECURSE src RELATIVE "${srcdir}" CONFIGURE_DEPENDS "${srcdir}/*.cpp" "${srcdir}/*.hpp")

set(srcs)
foreach(file ${src})
    get_filename_component(path "${file}" DIRECTORY)
    string(REPLACE "/" "\\" path "Source Files/${path}")
    source_group("${path}" FILES "${srcdir}/${file}")
    list(APPEND srcs "${srcdir}/${file}")
endforeach()

add_library(
    ${name}
    STATIC
        ${srcs}
)

target_compile_features(${name} PUBLIC cxx_std_20)
set_target_properties(${name} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(
    ${name}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(
    ${name}
    PUBLIC
        ruis::ruis
        GLEW::GLEW
)

if(NOT RUIS_RENDER_OPENGL_DISABLE_INSTALL)

# install library header files preserving directory hierarchy
install(
    DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/ruis"
    DESTINATION
        "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN
        "*.hpp"
)

install(
    TARGETS
        ${name}
    EXPORT # generate cmake configs
        ${name}-config
)

# install cmake configs
install(
    EXPORT
        ${name}-config
    FILE
        ${name}-config.cmake
    DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/${name}
    NAMESPACE
        ${name}::
)

endif()
