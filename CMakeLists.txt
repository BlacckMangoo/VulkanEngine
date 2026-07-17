cmake_minimum_required(VERSION 3.25)
project(gpuGems LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Vulkan REQUIRED)
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/spdlog)
add_subdirectory(external/VulkanMemoryAllocator)
add_subdirectory(external/fastgltf)
add_subdirectory(external/fmt)
add_subdirectory(external/tracy)
add_subdirectory(external/vk-bootstrap)
add_executable(gpuGems src/main.cpp src/stb_impl.cpp src/vma_impl.cpp src/window.cpp "src/vulkanContext.cpp" "src/swapchain.cpp" "src/camera.cpp" )
add_compile_options(/fsanitize=address)
#imgui
add_library(imgui
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/imgui_demo.cpp

        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_vulkan.cpp
)



#slang shader compilation 
find_program(SLANGC_EXECUTABLE slangc REQUIRED)

function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)

  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )

  set (SPV_OUTPUTS "")
  foreach (SHADER_SRC ${SHADER_SOURCES})
    get_filename_component (SHADER_NAME ${SHADER_SRC} NAME_WE)
    set (SPV_OUT ${SHADERS_DIR}/${SHADER_NAME}.spv)

    add_custom_command (
            OUTPUT  ${SPV_OUT}
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SRC} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SPV_OUT}
            WORKING_DIRECTORY ${SHADERS_DIR}
            DEPENDS ${SHADERS_DIR} ${SHADER_SRC}
            COMMENT "Compiling ${SHADER_NAME}.slang"
            VERBATIM
    )
    list (APPEND SPV_OUTPUTS ${SPV_OUT})
  endforeach()

  add_custom_target (${TARGET} DEPENDS ${SPV_OUTPUTS})
endfunction()



target_include_directories(imgui PUBLIC external/imgui external/imgui/backends)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)

target_link_libraries(gpuGems PRIVATE Vulkan::Vulkan imgui GPUOpen::VulkanMemoryAllocator glfw glm spdlog::spdlog fastgltf fmt Tracy::TracyClient vk-bootstrap )
target_include_directories(gpuGems PRIVATE external/stb src )

file (GLOB SHADER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.slang)

add_slang_shader_target(
    Shaders
    SOURCES ${SHADER_SOURCES}
)
add_dependencies(gpuGems Shaders) #ensures that shaders are compiled before building the gpuGems target