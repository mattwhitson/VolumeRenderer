cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 20)
project(VolumeRenderer CXX)

add_executable(VolumeRenderer 
	
	Types.h 
	DescriptorHeap.h 
	Queue.h
	Window.h 
	Device.h 
	Application.h 
	
	DescriptorHeap.cpp 
	Queue.cpp 
	Window.cpp 
	Device.cpp 
	Application.cpp 
	Main.cpp
)

#set_source_files_properties(Shader.hlsl PROPERTIES VS_SHADER_TYPE "Library" VS_SHADER_MODEL "6.3" VS_SHADER_VARIABLE_NAME "compiledShader" VS_SHADER_OUTPUT_HEADER_FILE "shader.fxh")

#target_include_directories(VolumeRenderer PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(
	VolumeRenderer PUBLIC
	d3d12
	dxgi
	user32
)

target_precompile_headers(VolumeRenderer PRIVATE stdafx.h)

# D3D12MA
add_subdirectory(ThirdParty/D3D12MemoryAllocator EXCLUDE_FROM_ALL)
target_link_libraries(VolumeRenderer PUBLIC D3D12MemoryAllocator)