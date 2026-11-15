cmake_minimum_required(VERSION 3.20)
project(SkellNative VERSION 0.1.0 LANGUAGES CXX)

option(SKELLNATIVE_BUILD_TESTS "Build SkellNative regression tests" ON)

add_library(skellnative STATIC src/audio.cpp)
add_library(SkellNative::Native ALIAS skellnative)
target_include_directories(skellnative PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(skellnative PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(skellnative PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(skellnative PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING AND SKELLNATIVE_BUILD_TESTS)
  add_executable(skellnative_audio_tests tests/audio_tests.cpp)
  target_link_libraries(skellnative_audio_tests PRIVATE SkellNative::Native)
  if(MSVC)
    target_compile_options(skellnative_audio_tests PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(skellnative_audio_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME skellnative_audio_tests COMMAND skellnative_audio_tests)
endif()
