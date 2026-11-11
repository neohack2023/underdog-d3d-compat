cmake_minimum_required(VERSION 3.20)
project(UnderdogRuntime VERSION 0.1.0 LANGUAGES CXX)
if(NOT WIN32)
 message(FATAL_ERROR "UnderdogRuntime currently requires Windows and the D3D12 SDK")
endif()
include(CTest)
add_library(UnderdogRuntime STATIC src/Runtime.cpp)
add_library(Underdog::Runtime ALIAS UnderdogRuntime)
target_compile_features(UnderdogRuntime PUBLIC cxx_std_17)
target_include_directories(UnderdogRuntime PUBLIC include)
target_link_libraries(UnderdogRuntime PUBLIC d3d12 dxgi)
if(MSVC)
 target_compile_options(UnderdogRuntime PRIVATE /W4)
endif()
if(BUILD_TESTING)
 add_executable(underdog_runtime_policy_tests tests/runtime_policy_tests.cpp)
 target_link_libraries(underdog_runtime_policy_tests PRIVATE UnderdogRuntime)
 add_test(NAME runtime_policy_no_gpu COMMAND underdog_runtime_policy_tests)
endif()
option(UNDERDOG_BUILD_EXAMPLES "Build selected native examples" ON)
if(UNDERDOG_BUILD_EXAMPLES)
 add_executable(underdog_sample_present examples/present/independent_present_v01.cpp)
 target_link_libraries(underdog_sample_present PRIVATE UnderdogRuntime user32)
 add_executable(underdog_sample_bcn examples/texture_stream/bcn_sample_v02.cpp)
 target_link_libraries(underdog_sample_bcn PRIVATE UnderdogRuntime d3dcompiler)
endif()

# Built but not run in hosted CI: needs an actual D3D12 device.
add_executable(underdog_runtime_hardware_smoke tests/runtime_hardware_smoke.cpp)
target_link_libraries(underdog_runtime_hardware_smoke PRIVATE UnderdogRuntime)

# Owned 1-pixel typed/structured UAV root versus descriptor table regression. Build only in hosted CI.
add_executable(underdog_typed_uav_ab examples/diagnostics/typed_uav_bindings_v01.cpp)
target_link_libraries(underdog_typed_uav_ab PRIVATE UnderdogRuntime d3dcompiler)

# Original interactive game-like BVH-to-swapchain integration fixture
add_executable(underdog_arena examples/games/underdog_arena_v02.cpp)

# V03 keeps the verified V02 binary and adds an opt-in legal typed UAV texture path.
add_executable(underdog_arena_v03 examples/games/underdog_arena_v03.cpp)
target_link_libraries(underdog_arena_v03 PRIVATE UnderdogRuntime d3dcompiler user32)
target_link_libraries(underdog_arena PRIVATE UnderdogRuntime d3dcompiler user32)

# PS2 GS 4 MiB VRAM transport feasibility test: compile in CI, execute only on
# explicitly selected hardware/WARP adapters. No game code or ISO required.
add_executable(underdog_gs_vram_roundtrip tests/gs_vram_gpu_roundtrip_v01.cpp)
target_compile_features(underdog_gs_vram_roundtrip PRIVATE cxx_std_17)
target_link_libraries(underdog_gs_vram_roundtrip PRIVATE d3d12 dxgi)

# A hardware-only, offscreen D3D11 sprite raster test against the separately
# generated PS2Recomp GS CPU oracle. CI compiles but never auto-executes it.
# Runtime expects gs_texture_8x8.rgba and gs_cpu_64x64.rgba in its workdir.
add_executable(underdog_gs_sprite_d3d11 tests/gs_sprite_d3d11_v02.cpp)
target_compile_features(underdog_gs_sprite_d3d11 PRIVATE cxx_std_17)
target_link_libraries(underdog_gs_sprite_d3d11 PRIVATE d3d11 d3dcompiler dxgi)

# Pure-Python reproducibility gate: checks pixel checksum against the pinned
# external CPU GS oracle witness without importing GPL source or needing a GPU.
if(BUILD_TESTING)
 find_package(Python3 COMPONENTS Interpreter QUIET)
 if(Python3_Interpreter_FOUND)
  add_test(NAME gs_sprite_fixture_no_gpu
   COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_gs_sprite_fixture_v02.py
           --output ${CMAKE_CURRENT_BINARY_DIR}/gs_fixture_v02)
 endif()
endif()
