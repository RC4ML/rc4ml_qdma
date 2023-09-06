cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)

project(qdma LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC")


add_subdirectory(fmt)

set(SOURCE_FILES
	QDMAController.cpp
)

set(HEAD_FILES 
	QDMAController.h
	QDMAController.hpp
)

SET(COMPILE_CUDA ON)
if (${COMPILE_CUDA})
	enable_language(CUDA)
	set_source_files_properties(${SOURCE_FILES} PROPERTIES LANGUAGE CUDA)
	add_compile_options(-Xcompiler=-mavx512f)
	add_definitions(-DGPU_ENABLE)
else ()
	add_compile_options(-mavx512f)
endif ()

add_library(qdma SHARED ${SOURCE_FILES})
target_link_libraries(qdma PRIVATE fmt::fmt-header-only)

if (${COMPILE_CUDA})
	target_link_libraries(qdma PRIVATE cuda cudart gdrapi)
endif ()

install(
	TARGETS qdma
	DESTINATION /usr/lib64
)

install(
	FILES ${HEAD_FILES}
	DESTINATION /usr/include)