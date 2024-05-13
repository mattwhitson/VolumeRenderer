cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 20)
project(VolumeRenderer CXX)

add_executable(VolumeRenderer 
	VertexShader.hlsl
	PixelShader.hlsl

	Camera.h 
	Types.h 
	DescriptorHeap.h 
	Queue.h
	Window.h 
	Device.h 
	Application.h 
	
	Camera.cpp 
	DescriptorHeap.cpp 
	Queue.cpp 
	Window.cpp 
	Device.cpp 
	Application.cpp 
	Main.cpp
)

set_source_files_properties(VertexShader.hlsl PROPERTIES VS_SHADER_TYPE "Vertex" VS_SHADER_MODEL "6.6" VS_SHADER_ENTRYPOINT "VSMain" VS_SHADER_DISABLE_OPTIMIZATIONS true VS_SHADER_ENABLE_DEBUG true
	VS_SHADER_OBJECT_FILE_NAME "${CMAKE_BINARY_DIR}/VertexShader.cso")
set_source_files_properties(PixelShader.hlsl PROPERTIES VS_SHADER_TYPE "Pixel" VS_SHADER_MODEL "6.6" VS_SHADER_ENTRYPOINT "PSMain" VS_SHADER_DISABLE_OPTIMIZATIONS true VS_SHADER_ENABLE_DEBUG true
	VS_SHADER_OBJECT_FILE_NAME "${CMAKE_BINARY_DIR}/PixelShader.cso")

target_include_directories(VolumeRenderer PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(
	VolumeRenderer PUBLIC
	d3d12
	d3dcompiler
	dxgi
	user32
)

target_precompile_headers(VolumeRenderer PRIVATE stdafx.h)

# D3D12MA
add_subdirectory(ThirdParty/D3D12MemoryAllocator EXCLUDE_FROM_ALL)
target_link_libraries(VolumeRenderer PUBLIC D3D12MemoryAllocator)