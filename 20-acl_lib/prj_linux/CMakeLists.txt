#############################################################################
####filename:--->CMakeLists.txt_binary_exec								 ####
####function:--->cmake file template for 			 					 ####
####			 generating .so or .a lib file		  					 ####
####author	:--->eric lau								    			 ####
####created	:--->2018.07.17												 ####
####version	:--->v1.0													 ####
#############################################################################

#CMake help  cmake -DPROCESSOR_TYPE=x3399 -DCMAKE_BUILD_TYPE=Debug .
#set projectname
PROJECT(acl_lib_prj)

#minimun requirement of cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

#add -D OPTIONS
ADD_DEFINITIONS(-DARMLINUX)


if(PROCESSOR_TYPE STREQUAL "x3399")
    SET(CMAKE_C_COMPILER "aarch64-linux-gnu-gcc")
    SET(CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++")
	
	#set debug version options
	SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -g")
	
	#set release version options
	SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS}")
else()
	if(NOT CMAKE_BUILD_TYPE)
		SET(CMAKE_BUILD_TYPE "Release")
	endif()
    SET(CMAKE_C_COMPILER "gcc")
    SET(CMAKE_C_COMPILER "g++")
	
	#set debug version options
	SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -std=c++11")
	
	#set release version options
	SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -fPIC -std=c++11")
endif()


#set lib .so or .a file build store path

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../10-common/lib/debug/linux)
else()
	SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../10-common/lib/release/linux)
endif()

#header include file path, similar to -I/usr/include in Makefile
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../../10-common/include)

#link lib .so file dir, similar to -L/usr/lib/ in Makefile
#LINK_DIRECTORIES(${CMAKE_CURRENT_LIST_DIR}/sqlite3/lib ${CMAKE_CURRENT_SOURCE_DIR}/../10-common/lib/release/linux)

#fetch source files in specific dir to variable 
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../src DIR_SRC)

#generate .so or .a lib file in the previous specifix path of LIBRARY_OUTPUT_PATH
add_library(acl_lib STATIC ${DIR_SRC})




