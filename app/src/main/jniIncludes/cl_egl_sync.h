//
// Created by rchanda on 7/2/2021.
//

#ifndef ANDROID_SHADER_DEMO_JNI_LOCALDISPLAY_QCCLSDK_CL_EGL_SYNC_CL_EGL_SYNC_H
#define ANDROID_SHADER_DEMO_JNI_LOCALDISPLAY_QCCLSDK_CL_EGL_SYNC_CL_EGL_SYNC_H


//--------------------------------------------------------------------------------------
// File: cl_egl_sync.cpp
// Desc:
//
// Author: QUALCOMM
//
//          Copyright (c) 2021 QUALCOMM Technologies, Inc.
//                         All Rights Reserved.
//                      QUALCOMM Proprietary/GTDR
//--------------------------------------------------------------------------------------

/* This example demonstrates the usage of sync between cl and gl/egl
 * Create a RGB format 2D texture as a source image on GL side.
 * Create image targets in CL for RGB formats
 * In the kernel, apply the grayscale algorithm and copy to destination image.
 * Pass the destination image back to GL side for drawing on to off screen buffer.
 */

// OpenCL Headers
#include "CL/cl.h"
#include "CL/cl_ext.h"
#include "CL/cl_ext_qcom.h"
#include "CL/opencl.h"
#include "CL/cl_egl.h"
#include "CL/cl.hpp"
// OpenGL Headers
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

// Project includes
#include "cl_wrapper.h"

// Std includes
#include <iostream>

namespace CL_EGL_SYNC {
    static const int IMAGE_WIDTH = 3840;
    static const int IMAGE_HEIGHT = 2160;
    static const int FRAME_COUNT = 60;
    static const int BYTES_PER_PIXEL = 4;

    struct rgb_unit {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };
    static const uint8_t ALPHA_MAX = 255;
    static const rgb_unit RGB[3] = {{255, 0,   0},
                                    {0,   0,   255},
                                    {0,   255, 0}};
/*
float vertices[] = {
        // first triangle
        1.0f,  1.0f, 0.0f,  // top right
        1.0f, -1.0f, 0.0f,  // bottom right
        -1.0f,  1.0f, 0.0f,  // top left
        // second triangle
        1.0f, -1.0f, 0.0f,  // bottom right
        -1.0f, -1.0f, 0.0f,  // bottom left
        -1.0f,  1.0f, 0.0f   // top left
};*/

    static const GLint FLOAT_SIZE_BYTES = 4;
    static const GLint TRIANGLE_VERTICES_DATA_STRIDE_BYTES = 5 * FLOAT_SIZE_BYTES;
    static const GLfloat TRIANGLE_VERTICES_DATA[] = {
            // X, Y, Z, U, V
            -1.0f, -1.0f, 0.0f, 0.f, 0.f,
            1.0f, -1.0f, 0.0f, 1.f, 0.f,
            -1.0f, 1.0f, 0.0f, 0.f, 1.f,
            1.0f, 1.0f, 0.0f, 1.f, 1.f,
    };

    static const char VERTEX_SHADER_SOURCE[] =
            "attribute vec4 position;\n"
            "attribute vec2 texCoords;\n"
            "varying vec2 outTexCoords;\n"
            "\nvoid main(void) {\n"
            "    outTexCoords = texCoords;\n"
            "    gl_Position = position;\n"
            "}\n\n";

    static const char FRAGMENT_SHADER_SOURCE[] =
            "precision mediump float;\n\n"
            "varying vec2 outTexCoords;\n"
            "uniform sampler2D texture;\n"
            "\nvoid main(void) {\n"
            "    gl_FragColor = texture2D(texture, outTexCoords);\n"
            "}\n\n";

    static const char *PROGRAM_SOURCE[] = {
            "const sampler_t sampler = CLK_FILTER_NEAREST | CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_FALSE;\n",
            "__kernel void rgba_grayscale(__read_only image2d_t in, __write_only image2d_t out) {\n",
            "uint i = get_global_id(0);\n",
            "uint j = get_global_id(1);\n",
            "int2 incoord = (int2)(i,j);\n",
            "float4 inValue = read_imagef(in, sampler, incoord);\n",
            "float4 outValue = inValue;\n",
            "outValue.x = 0.299f*inValue.x + 0.587f*inValue.y + 0.114f*inValue.z;\n",
            "outValue.y = 0.299f*inValue.x + 0.587f*inValue.y + 0.114f*inValue.z;\n",
            "outValue.z = 0.299f*inValue.x + 0.587f*inValue.y + 0.114f*inValue.z;\n",
            "write_imagef(out, incoord, outValue);\n",
            "}\n",
    };

    static const cl_uint PROGRAM_SOURCE_LEN = sizeof(PROGRAM_SOURCE) / sizeof(const char *);

    class cleglsync {
    public:
        cleglsync(){

        }
        ~cleglsync(){

        }
        void load_data(uint32_t width, uint32_t height, char *p_dest);
        GLuint cl_egl_sync_load_shader(GLenum shader_type, const char *p_source);
        int startCLGLMapTest();
    };
}
#endif //ANDROID_SHADER_DEMO_JNI_LOCALDISPLAY_QCCLSDK_CL_EGL_SYNC_CL_EGL_SYNC_H
