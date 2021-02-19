
cmake_minimum_required(VERSION 3.17)
project(pqtest)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_PREFIX_PATH /Users/huangming.huang/local)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS iostreams program_options)
find_package(PkgConfig REQUIRED)
pkg_check_modules(pq REQUIRED IMPORTED_TARGET libpq)

set(PostgreSQL_INCLUDE_DIR ${pq_INCLUDE_DIRS})
set(PostgreSQL_TYPE_INCLUDE_DIR ${pq_INCLUDE_DIRS}/libpq)
include(cmake/CPM.cmake)
set(SKIP_BUILD_TEST ON)
CPMAddPackage(
  NAME pqxx
  GITHUB_REPOSITORY jtv/libpqxx
  GIT_TAG 7.2.1
)



add_executable(exception_test exception_test.cpp)
target_link_libraries(exception_test pqxx)