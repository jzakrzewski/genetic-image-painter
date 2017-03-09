cmake_minimum_required( VERSION 3.6 FATAL_ERROR )

project( "Genetic Image Painter" CXX )
set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake" ${CMAKE_MODULE_PATH} )

find_package( SFML 2 REQUIRED graphics system window )

set( SOURCES
     main.cpp
     )

add_executable( monalisa ${SOURCES} )
set_target_properties( monalisa PROPERTIES
                       CXX_STANDARD 17
                       CXX_STANDARD_REQUIRED ON )
target_include_directories( monalisa SYSTEM PRIVATE ${SFML_INCLUDE_DIR} )
target_link_libraries( monalisa ${SFML_LIBRARIES} )

if( MSVC )
    target_compile_options( monalisa PRIVATE "/W4" )
else()
    target_compile_options( monalisa PRIVATE
                            -Wall
                            -Werror
                            -Wextra
                            -Wmissing-include-dirs
                            -Wno-long-long
                            -Wpedantic
                            -Wuninitialized )
    target_link_libraries(monalisa stdc++fs)
endif()
