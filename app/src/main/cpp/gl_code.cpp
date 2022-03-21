/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <EGL/egl.h>
#include "../jniIncludes/cl_code.h"
#include "../jniIncludes/speckle_utils.h"
#include "../jniIncludes/cl_egl_sync.h"
#include "opencv2/opencv.hpp"
#include <opencv2/core/ocl.hpp>
#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

namespace abc {}
using namespace CL_EGL_SYNC;


//windows/texture dimensions
const int g_width = 1024, g_height=1024;
struct timeval startNDKKernel, endNDKKernel;
int g_iteration = 0;
//number of frames to average the fps
const int g_iterations_num = 255;

//GL objects

//OpenCL objects
cl_device_id     g_device = NULL;
cl_context       g_context = NULL;
cl_command_queue g_queue = NULL;
cl_mem           g_mem = NULL;
cl_kernel        g_kernel_image = NULL;
cl_program       g_program = NULL;

bool g_InitDone = false;
//if the OpenCL impl supports cl_khr_gl_event
//there is a guarantee that clEnqueueAcquireGLObjects/clEnqueueReleaseGLObjects
//implicitly synchronize with an OpenGL context bound in the same thread as the OpenCL context
bool g_bImplicitSync = false;


/*****OpenGL related ****/
GLuint programId;
GLuint aPosition;
GLuint aTexCoord;
GLuint rubyTexture;
GLuint texture_map;

int scnw, scnh, vw, vh;
char *gVs, *gFs;


static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

inline unsigned char saturate_cast_uchar(float val) {
    //val += 0.5; // to round the value
    return static_cast<unsigned char>(val < 0 ? 0 : (val > 0xff ? 0xff : val));
}

static const char* IMAGEFILL_KERNEL_SOURCE[] = {
        "__kernel void imagefill(const float4 val, __write_only image2d_t output)\n",
        "{\n",
        "    int2 coord = (int2)(get_global_id(0), get_global_id(1));\n",
        "    write_imagef(output, coord, val);\n",
        "}"
};
static cl_uint IMAGEFILL_KERNEL_SOURCE_LEN = sizeof(IMAGEFILL_KERNEL_SOURCE) / sizeof(const char*);

auto gVertexShader =
        "attribute vec4 aPosition;\n"
            "attribute vec2 aTexCoord;\n"
            "varying vec2 vTexCoord;\n"
            "void main() {\n"
            "   gl_Position = vec4(aPosition, 0.0, 1.0);\n"
            "   vTexCoord = aTexCoord;\n"
            "}";

auto gFragmentShader =
        "precision mediump float;\n"
            "uniform sampler2D rubyTexture;\n"
            "varying vec2 vTexCoord;\n"
            "void main() {\n"
            "   vec4 color = texture2D(rubyTexture, vTexCoord);\n"
            "   gl_FragColor = color;\n"
            "}";
bool CreateCLMemObject()
{
    cl_int res = CL_SUCCESS;
            //notice that since our OpenCL kernel overwrites the previous content of the texture, we specify CL_MEM_WRITE_ONLY
            //we can specify CL_MEM_READ_WRITE instead
            //(if we need the current texture contentin the OpenCL kernel, notice that qualifiers for the input OpenCL image should be chnaged accordingly)
            g_mem = clCreateFromGLTexture(g_context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture_map, &res);
            if (res != CL_SUCCESS)
            {
                DPRINTF(" Error clCreateFromGLTexture %d",res);
                return false;
            }
            return true;
}

cl_program make_program(const char **source, cl_uint source_len, const char *options = "")
{
    cl_int err = 0;
    cl_program program = clCreateProgramWithSource(g_context, source_len, source, NULL, &err);
    if (err != CL_SUCCESS)
    {
        DPRINTF("Error with clCreateProgramWithSource.", err );
    }

    err = clBuildProgram(program, 0, NULL, options, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        DPRINTF("Error with clBuildProgram.\n",err);
        static const size_t LOG_SIZE = 2048;
        char log[LOG_SIZE];
        log[0] = 0;
        err = clGetProgramBuildInfo(program, g_device, CL_PROGRAM_BUILD_LOG, LOG_SIZE, log, NULL);
        if (err == CL_INVALID_VALUE)
        {
            DPRINTF("There was a build error, but there is insufficient space allocated to show the build logs.\n");
        }
        else
        {
            DPRINTF("Build error: %s", log);
        }
        std::exit(EXIT_FAILURE);
    }

    return program;
}

void init_cl()
{

    cl_int cl_err = CL_SUCCESS;
    //for the interop with GL, the OpenCL context should be initialized AFTER OpenGL one
    cl_platform_id platform;
    cl_int statusGetPlatformID = clGetPlatformIDs(1, &platform, NULL);
    if (statusGetPlatformID != CL_SUCCESS) {
        DPRINTF("clGetPlatformIDs failed with error %d", statusGetPlatformID);
    }

    std::string device_type_name = "gpu";
    const cl_device_type device_type = 4;

    //here we describe the platform that features sharing with the specific OpenGL context, later we query for the devices this platform offers
    cl_context_properties properties[] =
            {
                    CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
                    CL_GL_CONTEXT_KHR,      (cl_context_properties) eglGetCurrentContext(),
                    CL_EGL_DISPLAY_KHR,     (cl_context_properties) eglGetCurrentDisplay(),
                    0
            };

    clGetGLContextInfoKHR_fn pclGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddressForPlatform(platform, "clGetGLContextInfoKHR");
//    DPRINTF("Failed to query proc address for clGetGLContextInfoKHR!");

    //this is important step - getting the CL device(s) capable of sharing with the curent GL context
    size_t devSizeInBytes = 0;
    //CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR returns only the device currently associated with the given OGL context
    //so it will return only the GPU device
    //in contrast CL_DEVICES_FOR_GL_CONTEXT_KHR returns all interopbable devices (e.g. CPU in addition to the GPU)
    //we use CL_DEVICES_FOR_GL_CONTEXT_KHR below so that we can potentially experiment by doing interop with CPU as well
    cl_err = pclGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 0, NULL, &devSizeInBytes);
    if (cl_err != CL_SUCCESS)
    {
        DPRINTF(" Error pclGetGLContextInfoKHR %d",cl_err);
//        std::exit(cl_err);
    }
    const size_t devNum = devSizeInBytes/sizeof(cl_device_id);

    if (devNum)
    {
        std::vector<cl_device_id> devices (devNum);
        cl_err = pclGetGLContextInfoKHR( properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, devSizeInBytes, &devices[0], NULL);
        if (cl_err != CL_SUCCESS)
        {
            DPRINTF("inside devNum check Error pclGetGLContextInfoKHR %d",cl_err);
//        std::exit(cl_err);
        }
        cl_err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &g_device, NULL);
        if (cl_err != CL_SUCCESS)
        {
            DPRINTF("Error with clGetDeviceIDs:[%d]",cl_err);
        }
#if 0
        for (size_t i=0;i<devNum; i++)
        {
            cl_device_type t;
            cl_err = clGetDeviceInfo(devices[i], CL_DEVICE_TYPE, sizeof(t), &t, NULL);
            if (cl_err != CL_SUCCESS)
            {
                DPRINTF("Error clGetDeviceInfo %d",cl_err);
//        std::exit(cl_err);
            }
            if(device_type==t)
            {
                g_device = devices[i];
                size_t device_name_size = 0;
                cl_err = clGetDeviceInfo(g_device, CL_DEVICE_NAME, 0, NULL, &device_name_size);
                if (cl_err != CL_SUCCESS)
                {
                    DPRINTF("inside device_type check Error clGetDeviceInfo %d",cl_err);
                }
                std::vector<char> device_name (device_name_size);
                cl_err = clGetDeviceInfo(g_device, CL_DEVICE_NAME, device_name_size, (void*)&device_name[0], NULL);
                DPRINTF("Selecting %s device: %s\n", device_type_name.c_str(), &device_name[0]);
                if (cl_err != CL_SUCCESS)
                {
                    DPRINTF("after vector for device_name Error clGetDeviceInfo %d",cl_err);
                }

                size_t ext_string_size = 0 ;
                cl_err = clGetDeviceInfo(g_device,CL_DEVICE_EXTENSIONS, NULL, NULL, &ext_string_size );
                if (cl_err != CL_SUCCESS)
                {
                    DPRINTF("after vector for extensions size Error clGetDeviceInfo %d",cl_err);
                }
                std::vector<char> extensions(ext_string_size);
                cl_err = clGetDeviceInfo(g_device,CL_DEVICE_EXTENSIONS,ext_string_size,(void*)&extensions[0], NULL);
                if (cl_err != CL_SUCCESS)
                {
                    DPRINTF("after vector for extensions list Error clGetDeviceInfo %d",cl_err);
                }
                if(!strstr(&extensions[0], "cl_khr_gl_sharing"))
                {
                    DPRINTF("The selected device doesn't support cl_khr_gl_sharing!\n");
                    continue;
                }
                if(strstr(&extensions[0], "cl_khr_gl_event"))
                {
                    g_bImplicitSync = true;
                    DPRINTF("\nThe selected device supports cl_khr_gl_event, so clEnqueueAcquireGLObjects and clEnqueueReleaseGLObjects implicitly guarantee synchronization with an OpenGL context bound in the same thread as the OpenCL context. This saves on the expensive glFinish/clFinish() calls\n\n");
                }
                break;
            }
        }
#endif //for loop
    }
    if(!g_device)
    {
        DPRINTF("Cannot find OpenCL device of the desired type (%s) in the GL-shared context!\n", device_type == CL_DEVICE_TYPE_CPU ? "CPU" : device_type == CL_DEVICE_TYPE_GPU ?"GPU": device_type == CL_DEVICE_TYPE_ACCELERATOR ?"ACC": "unknown");
//        return FALSE;
    }

    cl_int res = CL_SUCCESS;
    g_context = clCreateContext(properties,1,&g_device,0,0,&res);
    if (res != CL_SUCCESS)
    {
        DPRINTF("clCreateContext fail Error  %d",res);
    }

    g_queue = clCreateCommandQueue(g_context,g_device,CL_QUEUE_PROFILING_ENABLE,&res);
    if (res != CL_SUCCESS)
    {
        DPRINTF("clCreateCommandQueue fail Error  %d",res);
    }

    if(!CreateCLMemObject())
        DPRINTF("CreateCLMemObject fail.");

    g_program = make_program(IMAGEFILL_KERNEL_SOURCE, IMAGEFILL_KERNEL_SOURCE_LEN);

    //eModeBufferTexture, in contrast operates with images, so it needs slightly different kernel
    g_kernel_image  = clCreateKernel(g_program,"imagefill", &res);
    if (res != CL_SUCCESS)
    {
        DPRINTF("clCreateKernel fail Error  %d",res);
    }
}
bool CallCLKernel()
{
    DPRINTF("CallCLKernel start");
    DPRINTF("CallCLKernel g_iteration:[%d]",g_iteration);
    cl_int cl_err = CL_SUCCESS;
    gettimeofday(&startNDKKernel, NULL);
    if(g_iteration == g_iterations_num)
        g_iteration = 0;
    const cl_float4 pattern = {(float)g_iteration/g_iterations_num,0.5f,0.f,1.f};//float required for write_imagef
    cl_err = clSetKernelArg(g_kernel_image ,0, sizeof(pattern), &pattern);
    if (cl_err != CL_SUCCESS) {
        DPRINTF("clSetKernelArg for 0 with error %d", cl_err);
    }
    cl_err =  clSetKernelArg(g_kernel_image ,1, sizeof(g_mem), &g_mem);
    if (cl_err != CL_SUCCESS) {
        DPRINTF("clSetKernelArg for 1 with error %d", cl_err);
    }
    const size_t gs[2] = {g_width, g_height};
    //eModeBufferTexture, operates with images, so it needs kernel that processes images
    cl_err = clEnqueueNDRangeKernel(g_queue,g_kernel_image, 2, 0, gs, NULL, NULL, NULL, NULL);
    if (cl_err != CL_SUCCESS) {
        DPRINTF("clEnqueueNDRangeKernel with error %d", cl_err);
    }
    clFinish(g_queue);
    gettimeofday(&endNDKKernel, NULL);
    DPRINTF("time taken frame  :%ld",((endNDKKernel.tv_sec * 1000000 + endNDKKernel.tv_usec)-(startNDKKernel.tv_sec * 1000000 + startNDKKernel.tv_usec)));

    DPRINTF("CallCLKernel end");
    return true;
}

bool UpdateGLObjectTexture()
{
    DPRINTF("UpdateGLObjectTexture start");
    //for texture(image) based  sharing this is really easy, as the texture is already connected with the CL mem object (via clCreateFromGLTexture)
    //we just need to acquire it
    //the acquire step is the same as for buffer based sharing (below)
    //first, make sure the GL commands are finished
    cl_int cl_err = CL_SUCCESS;
    if(!g_bImplicitSync)
    {
        glFinish();
    }

    cl_err = clEnqueueAcquireGLObjects(g_queue, 1,  &g_mem, 0, 0, NULL);
    if (cl_err != CL_SUCCESS) {
        DPRINTF("clEnqueueAcquireGLObjects with error %d", cl_err);
    }

    CallCLKernel();

    cl_err = clEnqueueReleaseGLObjects(g_queue, 1, &g_mem, 0, 0, NULL);
    if (cl_err != CL_SUCCESS) {
        DPRINTF("clEnqueueReleaseGLObjects with error %d", cl_err);
    }
    //all OpenCL operations should be finished before OGL proceeds
    if(!g_bImplicitSync)
    {
        cl_err = clFinish(g_queue);
        if (cl_err != CL_SUCCESS) {
            DPRINTF("clFinish with error %d", cl_err);
        }
    }
    DPRINTF("UpdateGLObjectTexture end");
    return true;
}


GLuint loadShader(GLenum shaderType, const char *pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char *buf = (char *) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n",
                         shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }

    return shader;
}

GLuint createProgram(const char *pVertexSource, const char *pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char *buf = (char *) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}



bool initProgram() {
//    LOGI("initProgram vs=%s fs=%s", gVs, gFs);
    if(!gVs)
        gVs = (char *)gVertexShader;
    if(!gFs)
        gFs = (char *)gFragmentShader;
    programId = createProgram(gVs, gFs);
    if (!programId) {
        LOGE("Could not create program.");
        return false;
    }

    aPosition = glGetAttribLocation(programId, "aPosition");
    aTexCoord = glGetAttribLocation(programId, "aTexCoord");
    rubyTexture = glGetUniformLocation(programId, "rubyTexture");

    return true;
}

bool setupGraphics(int w, int h) {
    scnw = w;
    scnh = h;
    LOGI("setupGraphics(%d, %d)", w, h);
    glViewport(0, 0, scnw, scnh);
    return true;
}

const GLfloat gTriangleVertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f, 1.0f,

        -1.0f, 1.0f,
        1.0f,-1.0f,
        1.0f,1.0f
};

const GLfloat gTexVertices[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,

        0.0f, 0.0f,
        1.0f,1.0f,
        1.0f,0.0f
};

void renderFrame() // 16.6ms
{
    DPRINTF("render frame start");
    float grey;
    grey = 0.00f;

    glClearColor(grey, grey, grey, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    UpdateGLObjectTexture();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_map);

    glUseProgram(programId);

    glVertexAttribPointer(aPosition, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
    glEnableVertexAttribArray(aPosition);

    glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 0, gTexVertices);
    glEnableVertexAttribArray(aTexCoord);

    glUniform1i(rubyTexture, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    g_iteration++;
    DPRINTF("render frame end");
}

void initGL(JNIEnv *env, jobject bmp) {

    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    glGenTextures(1, &texture_map);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_map);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glViewport(0, 0, g_width, g_height);
    glFinish();

}

void _loadShader(JNIEnv *env, jstring jvs, jstring jfs) {
    jboolean b;
    if(jvs) {
        const char *vs = env->GetStringUTFChars(jvs, &b);
        gVs = vs? strdup(vs) : NULL;
        env->ReleaseStringUTFChars(jvs, vs);
    } else
        gVs = (char *)gVertexShader;

    if(jfs) {
        const char *fs = env->GetStringUTFChars(jfs, &b);
        gFs = fs? strdup(fs) : NULL;
        env->ReleaseStringUTFChars(jfs, fs);
    } else
        gFs = (char *)gFragmentShader;

    initProgram();
}


extern "C" {
JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv *env, jobject obj, jobject bmp)
{
    initGL(env, bmp);
    init_cl();
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_resize(JNIEnv *env, jobject obj, jint width, jint height)
{
    setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv *env, jobject obj)
{
    renderFrame();
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadShader(JNIEnv *env, jobject obj, jstring vs, jstring fs)
{
    _loadShader(env, vs, fs);
}
};
