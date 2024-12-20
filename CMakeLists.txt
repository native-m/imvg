cmake_minimum_required (VERSION 3.15)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("imvg")

include(FindVulkan)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)

add_library(imvg "imvg.cpp" "imvg.h" "imvg_misc.cpp" "imvg_misc.h")
add_library(imvg-vulkan "imvg_vulkan.cpp" "imvg_vulkan.h" "imvg_vulkan_shaders.cpp")
target_link_libraries(imvg-vulkan PUBLIC imvg Vulkan::Headers)

add_executable(imvg-demo "imvg_demo.cpp")
target_link_libraries(imvg-demo PRIVATE imvg-vulkan)
