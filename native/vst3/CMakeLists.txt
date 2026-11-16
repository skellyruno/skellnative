cmake_minimum_required(VERSION 3.25)
project(SkellNativeGain VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "SkellNative direct VST3 prototype targets Windows x64 only")
endif()

option(SKELLNATIVE_BUILD_MINI "Build the Skell Mini Native DEV pilot" OFF)

# Pin the MIT-licensed official Steinberg 3.8.1 SDK with its submodules.
# Existing local SDK source avoids network on subsequent builds.
set(SKELLNATIVE_STEINBERG_SDK "" CACHE PATH "Local complete VST3 SDK checkout")
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE) # official validator
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

if(SKELLNATIVE_STEINBERG_SDK)
  if(NOT EXISTS "${SKELLNATIVE_STEINBERG_SDK}/CMakeLists.txt" OR
     NOT EXISTS "${SKELLNATIVE_STEINBERG_SDK}/public.sdk/source/vst/vstaudioeffect.h" OR
     NOT EXISTS "${SKELLNATIVE_STEINBERG_SDK}/pluginterfaces/vst/ivstaudioprocessor.h" OR
     NOT EXISTS "${SKELLNATIVE_STEINBERG_SDK}/base/source/fobject.h" OR
     NOT EXISTS "${SKELLNATIVE_STEINBERG_SDK}/cmake/modules/SMTG_AddVST3Library.cmake")
    message(FATAL_ERROR "SDK path must be a COMPLETE recursive VST3 SDK checkout")
  endif()
  set(SDK_ROOT "${SKELLNATIVE_STEINBERG_SDK}")
  add_subdirectory("${SDK_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/steinberg")
else()
  include(FetchContent)
  FetchContent_Declare(
    skell_steinberg_sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
    GIT_SHALLOW FALSE
    GIT_SUBMODULES_RECURSE TRUE
  )
  FetchContent_MakeAvailable(skell_steinberg_sdk)
  set(SDK_ROOT "${skell_steinberg_sdk_SOURCE_DIR}")
endif()

add_library(SkellNativeDSP INTERFACE)
target_include_directories(SkellNativeDSP INTERFACE
  "${CMAKE_CURRENT_LIST_DIR}/../include")
target_compile_features(SkellNativeDSP INTERFACE cxx_std_17)

# Shared host-neutral SkellDraw semantic controls + Win32 drawing adapter.
add_library(SkellDrawControls INTERFACE)
target_include_directories(SkellDrawControls INTERFACE
  "${CMAKE_CURRENT_LIST_DIR}/../skelldraw/include")
target_compile_features(SkellDrawControls INTERFACE cxx_std_17)

smtg_add_vst3plugin(SkellNativeGain
  Source/NativeGainProcessor.cpp
  Source/NativeGainController.cpp
  Source/NativeGainFactory.cpp
  Source/NativeGainIDs.h
  Source/NativeGainStateBridge.h
  Source/SkellDrawWin32.h
  Source/SkellDrawWin32.cpp
)
target_compile_features(SkellNativeGain PRIVATE cxx_std_17)
target_include_directories(SkellNativeGain PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Source")
target_link_libraries(SkellNativeGain PRIVATE sdk SkellNativeDSP SkellDrawControls user32 gdi32)

# Independent actual HWND attach, paint, resizing and input smoke.
add_executable(SkellDrawUiSmoke
  tests/SkellDrawUiSmoke.cpp
  Source/SkellDrawWin32.h
  Source/SkellDrawWin32.cpp
)
target_compile_features(SkellDrawUiSmoke PRIVATE cxx_std_17)
# Only profiling smoke pays for high-resolution timers and counter updates.
# The real SkellNativeGain VST3 deliberately omits this definition.
target_compile_definitions(SkellDrawUiSmoke PRIVATE SKELLDRAW_PROFILE=1)
target_include_directories(SkellDrawUiSmoke PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Source")
target_link_libraries(SkellDrawUiSmoke PRIVATE sdk SkellDrawControls user32 gdi32)



# Actual VST3 IAudioProcessor adapter (not merely the DSP kernel), compiled
# from the exact same source as the native VST3 binary. Separate executable
# models process data / sample-offset queues / meter outputs in memory.
add_executable(SkellNativeVst3AdapterBench
  tests/SkellNativeVst3AdapterBench.cpp
  Source/NativeGainProcessor.cpp
  Source/NativeGainIDs.h
  Source/NativeGainStateBridge.h
  "${SDK_ROOT}/public.sdk/source/vst/hosting/parameterchanges.cpp"
)
target_compile_features(SkellNativeVst3AdapterBench PRIVATE cxx_std_17)
target_include_directories(SkellNativeVst3AdapterBench PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Source")
target_link_libraries(SkellNativeVst3AdapterBench PRIVATE sdk SkellNativeDSP)
if(MSVC)
  target_compile_options(SkellNativeVst3AdapterBench PRIVATE /O2 /W4)
endif()

# Host validator and real FL Studio session tests are separate release gates.

# The upstream Steinberg moduleinfo generator currently emits trailing commas
# that fail strict RFC 8259 JSON parsers. Re-serialize the *generated factory
# metadata* after compilation, not a hand-maintained second class registry.
# Verify IDs and other semantics before writing to avoid hiding bad manifests.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
add_custom_command(TARGET SkellNativeGain POST_BUILD
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../tools/normalize_moduleinfo.py" "${CMAKE_BINARY_DIR}"
  COMMENT "Ensure Steinberg moduleinfo.json is strict validated JSON"
  VERBATIM
)

option(SKELLNATIVE_BUILD_SPACER "Build the separate Skell Spacer Native DEV pilot" OFF)
set(SKELLNATIVE_SPACER_SOURCE "" CACHE PATH "Pinned Skell Spacer reference checkout")
if(SKELLNATIVE_BUILD_SPACER)
  if(NOT SKELLNATIVE_SPACER_SOURCE)
    include(FetchContent)
    FetchContent_Declare(skell_spacer_reference
      GIT_REPOSITORY https://github.com/skellyruno/skell-spacer-vst.git
      GIT_TAG 05373baa6fb87ed870cb572d1fff888b1747daf7
      SOURCE_SUBDIR native-headers-only)
    FetchContent_MakeAvailable(skell_spacer_reference)
    set(SKELLNATIVE_SPACER_SOURCE "${skell_spacer_reference_SOURCE_DIR}")
  endif()
  foreach(header SpacerDSP.h ZeroLatencyEngine.h)
    if(NOT EXISTS "${SKELLNATIVE_SPACER_SOURCE}/Source/${header}")
      message(FATAL_ERROR "Missing Spacer reference DSP header: ${header}")
    endif()
  endforeach()
  # V16 keeps these header names but removes the Studio DSP. Fail at configure
  # time with an actionable error if a V16 checkout is provided accidentally.
  file(STRINGS "${SKELLNATIVE_SPACER_SOURCE}/Source/SpacerDSP.h"
    SKELLNATIVE_SPACER_STUDIO_DECLARATION REGEX "^class SpacerEngine ")
  if(NOT SKELLNATIVE_SPACER_STUDIO_DECLARATION)
    message(FATAL_ERROR
      "The two-engine Spacer DEV target needs Spacer reference 05373baa. "
      "The supplied checkout has no Studio engine. Omit SKELLNATIVE_SPACER_SOURCE to fetch the pinned source.")
  endif()
  smtg_add_vst3plugin(SkellSpacerNative PACKAGE_NAME "Skell Spacer Native DEV"
    Spacer/Factory.cpp Spacer/VstProcessor.cpp Spacer/VstController.cpp Spacer/Editor.cpp)
  target_include_directories(SkellSpacerNative PRIVATE "${SKELLNATIVE_SPACER_SOURCE}/Source")
  target_link_libraries(SkellSpacerNative PRIVATE sdk SkellNativeDSP SkellDrawControls user32 gdi32)
endif()

if(SKELLNATIVE_BUILD_SPACER OR SKELLNATIVE_BUILD_MINI)
  add_executable(SpacerVst3Parity tests/SpacerVst3Parity.cpp
    "${SDK_ROOT}/public.sdk/source/common/memorystream.cpp"
    "${SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
    "${SDK_ROOT}/public.sdk/source/vst/hosting/plugprovider.cpp")
  target_compile_features(SpacerVst3Parity PRIVATE cxx_std_17)
  target_link_libraries(SpacerVst3Parity PRIVATE sdk_hosting user32 gdi32)
endif()
if(SKELLNATIVE_BUILD_MINI)
  include(FetchContent)
  FetchContent_Declare(skellmini_reference
    GIT_REPOSITORY https://github.com/skellyruno/skelltune-mini.git
    GIT_TAG 6a1c0267967cab696f8d3b3371e9c40a4ed75a78
    SOURCE_SUBDIR native-headers-only)
  set(SKELLNATIVE_SKELLLANG_SOURCE "" CACHE PATH "Local SkellLang compiler checkout override")
  if(SKELLNATIVE_SKELLLANG_SOURCE)
    set(FETCHCONTENT_SOURCE_DIR_SKELLLANG_COMPILER "${SKELLNATIVE_SKELLLANG_SOURCE}")
  endif()
  FetchContent_Declare(skelllang_compiler
    GIT_REPOSITORY https://github.com/skellyruno/skellLang.git
    GIT_TAG 93871860d9d2b5b70ecfa6a9f5ddcd444e463431
    SOURCE_SUBDIR compiler-only)
  FetchContent_MakeAvailable(skellmini_reference skelllang_compiler)
  set(mini_controls "${CMAKE_CURRENT_BINARY_DIR}/MiniControls.inc")
  add_custom_command(OUTPUT "${mini_controls}"
    COMMAND "${Python3_EXECUTABLE}" -B -m skelllang.native_controls
      "${CMAKE_CURRENT_SOURCE_DIR}/Mini/Mini.skell" "${mini_controls}"
    WORKING_DIRECTORY "${skelllang_compiler_SOURCE_DIR}"
    DEPENDS Mini/Mini.skell "${skelllang_compiler_SOURCE_DIR}/skelllang/native_controls.py"
    VERBATIM)
  # Keep official validation as a CTest gate. The SDK logs ERROR for deliberately
  # rejected unsupported rates even when its test passes; MSBuild interprets that
  # console text as a compiler error. CTest preserves the full validator output.
  set(saved_run_validator "${SMTG_RUN_VST_VALIDATOR}")
  set(SMTG_RUN_VST_VALIDATOR OFF)
  smtg_add_vst3plugin(SkellMiniNative PACKAGE_NAME "Skell Mini Native DEV"
    Mini/Mini.cpp Mini/MiniEditor.h "${mini_controls}")
  set(SMTG_RUN_VST_VALIDATOR "${saved_run_validator}")
  add_dependencies(SkellMiniNative validator)
  enable_testing()
  add_test(NAME SkellMiniOfficialValidator COMMAND $<TARGET_FILE:validator>
    "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/Skell Mini Native DEV.vst3")
  add_test(NAME SkellMiniBinaryAudio COMMAND $<TARGET_FILE:SpacerVst3Parity> --mini
    "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/Skell Mini Native DEV.vst3")
  target_include_directories(SkellMiniNative PRIVATE
    "${skellmini_reference_SOURCE_DIR}/dsp" "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(SkellMiniNative PRIVATE sdk SkellNativeDSP SkellDrawControls user32 gdi32)
  add_custom_command(TARGET SkellMiniNative POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../tools/normalize_moduleinfo.py"
      "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>" --product "Skell Mini Native DEV"
    VERBATIM)

endif()

# Native parameter/state/automation contracts, exercised against real SDK queues.
enable_testing()
add_executable(SkellNativeControlTests tests/ControlRuntimeTests.cpp
  "${SDK_ROOT}/public.sdk/source/common/memorystream.cpp")
target_link_libraries(SkellNativeControlTests PRIVATE sdk_hosting sdk SkellNativeDSP)
target_compile_features(SkellNativeControlTests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(SkellNativeControlTests PRIVATE /W4 /WX)
endif()
add_test(NAME SkellNativeControlTests COMMAND SkellNativeControlTests)
