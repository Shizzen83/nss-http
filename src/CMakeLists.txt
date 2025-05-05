cmake_minimum_required(VERSION 3.5)
project(nss-http VERSION 1.0.0)

# Language-specific settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -O2")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb3")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")

# Project structure
set(SOURCE_DIR  ${PROJECT_SOURCE_DIR}/src)

set(SOURCES
    nss_http-passwd.c
    nss_http-group.c
    nss_http-shadow.c
    nss_http.c
)

set(LIBRARY_NAME "nss_http")

add_library(
    ${LIBRARY_NAME}
    SHARED ${SOURCES}
)

set_target_properties(
    ${LIBRARY_NAME}
    PROPERTIES SUFFIX ".so.2"
)

find_package(OpenSSL REQUIRED)

target_link_libraries(
    ${LIBRARY_NAME}
    curl
    jansson
    ${OPENSSL_LIBRARIES}
)

install(
    TARGETS ${LIBRARY_NAME}
    DESTINATION .
)
