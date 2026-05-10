cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

project(qdma_benchmark LANGUAGES CXX)
add_compile_options(-O3 -mavx512f -march=native)

add_executable(throughput
        throughput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/dma_tool.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/throughput_tool.cpp
)
target_include_directories(throughput PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_link_libraries(throughput PRIVATE fmt::fmt-header-only)
target_link_libraries(throughput PRIVATE qdma pthread)


add_executable(random
        random.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/dma_tool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/random_tool.cpp
)
target_include_directories(random PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_link_libraries(random PRIVATE fmt::fmt-header-only)
target_link_libraries(random PRIVATE qdma pthread)


add_executable(latency
	latency.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/dma_tool.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/latency_tool.cpp
)
target_include_directories(latency PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_link_libraries(latency PRIVATE fmt::fmt-header-only)
target_link_libraries(latency PRIVATE qdma pthread)


add_executable(mmio_bridge
        mmio_bridge.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/mmio.cpp
)
target_include_directories(mmio_bridge PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_link_libraries(mmio_bridge PRIVATE fmt::fmt-header-only)
target_link_libraries(mmio_bridge PRIVATE qdma pthread)