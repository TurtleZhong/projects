add_executable( vo0_3 vo0_3.cpp )
target_link_libraries( vo0_3 myslam )

add_executable( test_function test_function.cpp )
target_link_libraries( test_function myslam )
