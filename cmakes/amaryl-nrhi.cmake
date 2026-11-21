project(amaryl-nrhi)
set(TARGET amaryl-nrhi)

find_package(Vulkan REQUIRED)
if(NOT DEFINED ENV{VULKAN_SDK})
  message(FATAL_ERROR "VULKAN_SDK environment variable not defined.")
endif()

set(VULKAN_SDK_DIR "$ENV{VULKAN_SDK}")

find_program(AMARYL_SLANGC_EXECUTABLE NAMES slangc slangc.exe
  HINTS "${VULKAN_SDK_DIR}/Bin" REQUIRED)

set(AMARYL_NRHI_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/nrhi)
set(AMARYL_NRHI_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/nrhi)

set(AMARYL_NRHI_SOURCES
  ${AMARYL_NRHI_INCLUDE_ROOT}/config.h
  ${AMARYL_NRHI_INCLUDE_ROOT}/definitions.h
  ${AMARYL_NRHI_INCLUDE_ROOT}/nrhi.h
  ${AMARYL_NRHI_INCLUDE_ROOT}/resource_tracker.h

  ${AMARYL_NRHI_SOURCE_ROOT}/nrhi.cpp
  ${AMARYL_NRHI_SOURCE_ROOT}/definitions.cpp
  ${AMARYL_NRHI_SOURCE_ROOT}/resource_tracker.cpp
)

set(AMARYL_NRHI_VK_INCLUDE_ROOT ${AMARYL_NRHI_INCLUDE_ROOT}/vulkan)
set(AMARYL_NRHI_VK_SOURCE_ROOT ${AMARYL_NRHI_SOURCE_ROOT}/vulkan)
set(AMARYL_NRHI_VK_SOURCES
  ${AMARYL_NRHI_VK_INCLUDE_ROOT}/vma/vk_mem_alloc.h
  ${AMARYL_NRHI_VK_INCLUDE_ROOT}/vma/vma_usage.h
  ${AMARYL_NRHI_VK_INCLUDE_ROOT}/volk/volk.h

  ${AMARYL_NRHI_VK_SOURCE_ROOT}/vma/vma_usage.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/volk/volk.c

  ${AMARYL_NRHI_VK_SOURCE_ROOT}/buffer.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/command_buffer.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/context.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/image.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/immediate_command.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/nvk.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/pipeline.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/staging_device.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/swapchain.cpp
  ${AMARYL_NRHI_VK_SOURCE_ROOT}/utils.cpp

  ${VULKAN_SDK_DIR}/Source/SPIRV-Reflect/spirv_reflect.c
)

add_library(${TARGET} SHARED ${AMARYL_NRHI_SOURCES} ${AMARYL_NRHI_VK_SOURCES})
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/nrhi")
generate_export_header(${TARGET}
  EXPORT_MACRO_NAME NRHI_API
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/nrhi/nrhi_export.h"
)
target_include_directories(${TARGET}
  PUBLIC
  ${AMARYL_INCLUDE_ROOT}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
  ${Vulkan_INCLUDE_DIR}
  SYSTEM PRIVATE
  ${VULKAN_SDK_DIR}/Source/SPIRV-Reflect
)
target_link_libraries(${TARGET}
  PUBLIC
  amaryl-core
  SDL3::SDL3
  plog::plog
  Vulkan::Headers
  pystring
)
# The vold should define `VK_USE_PLATFORM_*_KHR`.
if(WIN32)
  target_compile_definitions(${TARGET} PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(ANDROID)
  target_compile_definitions(${TARGET} PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
elseif(IOS)
  target_compile_definitions(${TARGET} PUBLIC VK_USE_PLATFORM_IOS_MVK)
endif()
