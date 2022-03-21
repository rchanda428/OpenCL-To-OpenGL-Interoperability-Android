cmake_minimum_required(VERSION 3.4.1)

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
include_directories(jniIncludes)
add_library(gl2jni SHARED
            cpp/gl_code.cpp cpp/cl_wrapper.cpp cpp/libopencl.c cpp/util.cpp cpp/cl_egl_sync.cpp )
add_library( lib_opencv SHARED IMPORTED)
message("androidabi is ${ANDROID_ABI}")
set_target_properties(lib_opencv PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/jniLibs/${ANDROID_ABI}/libopencv_java4.so )

# add lib dependencies
target_link_libraries(gl2jni
                      android
                      jnigraphics
                      log 
                      EGL
                      GLESv2
                    lib_opencv
        )

