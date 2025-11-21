
cmake_minimum_required(VERSION 3.5)

project(googletest-download NONE)

include(ExternalProject)
ExternalProject_Add(googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           release-1.12.1
  SOURCE_DIR        "/home/runner/work/famfs/famfs/_codeql_build_dir/googletest-src"
  BINARY_DIR        "/home/runner/work/famfs/famfs/_codeql_build_dir/googletest-build"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
  )
