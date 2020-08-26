cmake_minimum_required(VERSION 3.17)

project(libactors VERSION 0.1 LANGUAGES C)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

set(CMAKE_C_STANDARD 11)
add_compile_definitions(_GNU_SOURCE)
add_compile_definitions(_FORTIFY_SOURCE=2)
add_compile_options(
	-O1 -Wall -Wextra -pedantic -Werror
 	-g -fno-omit-frame-pointer -fsanitize=address
)
add_link_options(-lurcu -lurcu-cds -lurcu-common)
add_link_options(-fsanitize=address)

add_library(actors src/actors.c)
target_include_directories(actors PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(actors Threads::Threads)
target_compile_definitions(actors
  INTERFACE libactors_VERSION_MAJOR=${libactors_VERSION_MAJOR}
  INTERFACE libactors_VERSION_MINOR=${libactors_VERSION_MINOR})

enable_testing()

function(actors_test name)
  add_executable(${name}-tests tests/${name}.c)
  target_link_libraries(${name}-tests actors)
  add_test(${name} ${name}-tests)
endfunction(actors_test)

actors_test(actors)
actors_test(filter)
actors_test(ring)
actors_test(sleepers)
actors_test(assert)

set_tests_properties(assert PROPERTIES WILL_FAIL 1)

install(TARGETS actors DESTINATION lib EXPORT libactorsTargets)
install(FILES include/actors.h DESTINATION include)

install(EXPORT libactorsTargets FILE libactorsTargets.cmake
  DESTINATION lib/cmake/libactors)

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/libactorsConfig.cmake"
  INSTALL_DESTINATION "lib/cmake/libactors"
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/libactorsConfigVersion.cmake"
  VERSION "${libactors_VERSION_MAJOR}.${libactors_VERSION_MINOR}"
  COMPATIBILITY AnyNewerVersion)

export(
  EXPORT libactorsTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/libactorsTargets.cmake")

export(PACKAGE libactors)
