# CMakeList.txt : CMake project for LHF, include source and define
# project specific logic here.
#

cmake_minimum_required(VERSION 3.11)

add_library(pipePacket "pipePacket.cpp" "pipePacket.hpp")
add_library(basePipe "basePipe.cpp" "basePipe.hpp")
add_library(distMatrixPipe "distMatrixPipe.cpp" "distMatrixPipe.hpp")
add_library(neighGraphPipe "neighGraphPipe.cpp" "neighGraphPipe.hpp")
add_library(ripsPipe "ripsPipe.cpp" "ripsPipe.hpp")
add_library(upscalePipe "upscalePipe.cpp" "upscalePipe.hpp")
add_library(persistencePairs "persistencePairs.cpp" "persistencePairs.hpp")
add_library(slidingWindow "slidingWindow.cpp" "slidingWindow.hpp")
add_library(fastPersistence "fastPersistence.cpp" "fastPersistence.hpp")
add_library(incrementalPersistence "incrementalPersistence.cpp" "incrementalPersistence.hpp")
add_library(naiveWindow "naiveWindow.cpp" "naiveWindow.hpp")
add_library(qhullPipe "qhullPipe.cpp" "qhullPipe.hpp")

include_directories(${PROJECT_SOURCE_DIR}/Complex)
link_directories(${PROJECT_SOURCE_DIR}/Complex)

include_directories(${PROJECT_SOURCE_DIR}/Pipes)
link_directories(${PROJECT_SOURCE_DIR}/Pipes)

include_directories(${PROJECT_SOURCE_DIR}/Utils)
link_directories(${PROJECT_SOURCE_DIR}/Utils)

# Use external project for qhull integration
include(FetchContent)

FetchContent_Declare(qhullext
    GIT_REPOSITORY "https://github.com/qhull/qhull.git"
    #BUILD_STATIC_LIBS OFF
    #BUILD_SHARED_LIBS ON
)

if(${CMAKE_VERSION} VERSION_LESS 3.14)
    FetchContent_GetProperties(qhullext)
    if(NOT qhullext_POPULATED)
        FetchContent_Populate(qhullext)
        add_subdirectory(${qhullext_SOURCE_DIR} ${qhullext_BINARY_DIR})
    endif()
else()
    FetchContent_MakeAvailable(qhullext)
endif()
    
    
include_directories(${PROJECT_SOURCE_DIR}/_deps/qhullext-src/src/)

# target_link_libraries(LHF PipeFunctions)
target_link_libraries(basePipe utils simplexBase pipePacket distMatrixPipe neighGraphPipe ripsPipe upscalePipe persistencePairs slidingWindow fastPersistence incrementalPersistence naiveWindow qhullcpp qhullstatic_r qhullPipe)

INSTALL (TARGETS basePipe
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib)

# TODO: Add tests and install targets if needed.

MESSAGE (STATUS "CMAKE_INCLUDE_PATH: " ${CMAKE_INCLUDE_PATH})
MESSAGE (STATUS "FLAGS: " ${CMAKE_CXX_FLAGS})
