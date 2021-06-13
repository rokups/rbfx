#
# Copyright (c) 2017-2020 the rbfx project.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
project(TheForge)

file (GLOB_RECURSE SOURCE_FILES_COMMON
    OS/Core/*
    OS/Interfaces/*
    OS/Logging/*
    OS/Math/*
    OS/FileSystem/*
    OS/MemoryTracking/*
)
file (GLOB SOURCE_FILES_COMMON_2
    OS/*
	Renderer/*
)

add_library(TheForge STATIC ${SOURCE_FILES_COMMON} ${SOURCE_FILES_COMMON_2} ${SOURCE_FILES_PLATFORM} ${SOURCE_FILES_RMEM})
target_link_libraries(TheForge PUBLIC EASTL)
target_include_directories(TheForge SYSTEM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
if (WIN32)
    file (GLOB_RECURSE SOURCE_FILES_PLATFORM
        #Renderer/Direct3D11/*
        Renderer/Direct3D12/*
        Renderer/Vulkan/*
        OS/Windows/WindowsThread.cpp
        OS/Windows/WindowsTime.cpp
    )
    if (MSVC AND NOT UWP)
        list (APPEND SOURCE_FILES_PLATFORM OS/Windows/WindowsStackTraceDump.cpp OS/Windows/WindowsStackTraceDump.h)
    endif ()
    target_compile_definitions(TheForge PRIVATE -D_WINDOWS=1 -DIRECT3D12 -DVULKAN) # -DDIRECT3D11 
    if (NOT "$ENV{VK_SDK_PATH}" STREQUAL "")
        target_include_directories(TheForge PUBLIC "$ENV{VK_SDK_PATH}/include")
        if (NOT URHO3D_64BIT)
            set(VK_SDK_BITS 32)
        endif ()
        target_link_libraries(TheForge PRIVATE "$ENV{VK_SDK_PATH}/Lib${VK_SDK_BITS}/vulkan-1.lib")
    endif ()
elseif (LINUX)
    file (GLOB_RECURSE SOURCE_FILES_PLATFORM
        Renderer/Vulkan/*
        OS/Linux/*
    )
    target_compile_definitions(TheForge PRIVATE -DVULKAN)
elseif (ANDROID)
    file (GLOB_RECURSE SOURCE_FILES_PLATFORM
        Renderer/OpenGLES/*
        Renderer/Vulkan/*
        OS/Android/*
    )
    target_compile_definitions(TheForge PRIVATE -DGLES -DVULKAN)
elseif (IOS)
    file (GLOB_RECURSE SOURCE_FILES_PLATFORM
        Renderer/Metal/*
        OS/Darwin/*
        OS/Linux/LinuxTime.cpp
    )
    target_compile_definitions(TheForge PUBLIC -DTARGET_IOS=1 PRIVATE -DMETAL)
elseif (APPLE)
    file (GLOB_RECURSE SOURCE_FILES_PLATFORM
        Renderer/Metal/*
        OS/Darwin/*
        OS/Linux/LinuxTime.cpp
    )
    target_compile_definitions(TheForge PRIVATE -DMETAL)
elseif (WEB)
    message(FATAL_ERROR "Not supported.")
endif ()
target_sources(TheForge PRIVATE ${SOURCE_FILES_PLATFORM})

if (NOT APPLE)
    target_sources(TheForge PRIVATE
        SpirvTools/SpirvTools.h
        SpirvTools/SpirvTools.cpp
    )
endif ()

if (NOT URHO3D_MERGE_STATIC_LIBS)
    install(TARGETS TheForge EXPORT Urho3D ARCHIVE DESTINATION ${DEST_ARCHIVE_DIR_CONFIG})
endif ()
