#include "../jniIncludes/cl_egl_sync.h"
using namespace CL_EGL_SYNC;
namespace CL_EGL_SYNC {
    void cleglsync::load_data(uint32_t width, uint32_t height, char *p_dest) {
        uint32_t row = 0;
        uint32_t col = 0;

        uint32_t r_bound = height / 3;
        if ((r_bound % 2) != 0) {
            r_bound = r_bound - 1;
        }
        uint32_t b_bound = 2 * height / 3;
        if ((b_bound % 2) != 0) {
            b_bound = b_bound - 1;
        }

        int index = 0;
        for (row = 0; row < height; ++row) {
            for (col = 0; col < width; ++col) {
                if (row < r_bound) {
                    p_dest[index++] = (char) RGB[0].r;  //Red
                    p_dest[index++] = (char) RGB[0].g;  //Green
                    p_dest[index++] = (char) RGB[0].b;  //Blue
                    p_dest[index++] = (char) ALPHA_MAX; //Alpa
                } else if ((row >= r_bound) && (row < b_bound)) {
                    p_dest[index++] = (char) RGB[1].r;  //Red
                    p_dest[index++] = (char) RGB[1].g;  //Green
                    p_dest[index++] = (char) RGB[1].b;  //Blue
                    p_dest[index++] = (char) ALPHA_MAX; //Alpa
                } else {
                    p_dest[index++] = (char) RGB[2].r;  //Red
                    p_dest[index++] = (char) RGB[2].g;  //Green
                    p_dest[index++] = (char) RGB[2].b;  //Blue
                    p_dest[index++] = (char) ALPHA_MAX; //Alpa
                }
            }
        }
    }
    GLuint cleglsync::cl_egl_sync_load_shader(GLenum shader_type, const char *p_source) {
        GLuint shader = glCreateShader(shader_type);
        if (shader) {
            glShaderSource(shader, 1, &p_source, NULL);
            glCompileShader(shader);
            GLint compiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (!compiled) {
                GLint info_len = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
                if (info_len) {
                    char *buf = (char *) malloc(info_len);
                    if (buf) {
                        glGetShaderInfoLog(shader, info_len, NULL, buf);
                        printf("Error: Could not compile shader %d:\n%s\n",
                               shader_type, buf);
                        free(buf);
                    }
                    glDeleteShader(shader);
                    shader = 0;
                }
            }
        }
        return shader;
    };
    int cleglsync::startCLGLMapTest() {
        /*
         * Step 0: Check if EGL 1.5 is supported
         */
        EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (egl_display == EGL_NO_DISPLAY) {
            std::cerr << "eglGetDisplay returned EGL_NO_DISPLAY." << "\n";
            return EXIT_FAILURE;
        }
        EGLint egl_version_major, egl_version_minor;
        EGLBoolean return_value = eglInitialize(egl_display, &egl_version_major,
                                                &egl_version_minor);
        if (return_value != EGL_TRUE) {
            std::cerr << "eglInitialize failed" << "\n";
            return EXIT_FAILURE;
        }
        if (!((egl_version_major >= 1) && (egl_version_minor >= 5))) {
            std::cerr << "Minimum required EGL version to run this sample is 1.5" << "\n";
            return EXIT_FAILURE;
        }

        /*
         * Step 1: Set up EGL
         */
        typedef EGLSync (EGLAPIENTRYP PFNEGLCREATESYNCPROC)(EGLDisplay dpy, EGLenum type,
                                                            const EGLAttrib *attrib_list);
        typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYSYNCPROC)(EGLDisplay dpy, EGLSync sync);
        typedef EGLBoolean (EGLAPIENTRYP PFNEGLWAITSYNCPROC)(EGLDisplay dpy, EGLSync sync,
                                                             EGLint flags);
        PFNEGLCREATESYNCPROC p_eglCreateSync = (PFNEGLCREATESYNCPROC) eglGetProcAddress(
                "eglCreateSync");
        PFNEGLWAITSYNCPROC p_eglWaitSync = (PFNEGLWAITSYNCPROC) eglGetProcAddress("eglWaitSync");
        PFNEGLDESTROYSYNCPROC p_eglDestroySync = (PFNEGLDESTROYSYNCPROC) eglGetProcAddress(
                "eglDestroySync");

        if (!p_eglCreateSync || !p_eglWaitSync || !p_eglDestroySync) {
            std::cerr << "Could not dynamically load needed EGL functions." << "\n";
            return EXIT_FAILURE;
        }

        EGLConfig egl_config;
        EGLint num_configs;
        static EGLint config_attribs[] = {
                EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_DEPTH_SIZE, 16,
                EGL_STENCIL_SIZE, 8,
                EGL_NONE
        };
        return_value = eglChooseConfig(egl_display, config_attribs, &egl_config, 1, &num_configs);
        if (return_value != EGL_TRUE) {
            std::cerr << "eglChooseConfig failed" << "\n";
            return EXIT_FAILURE;
        }

        static EGLint p_buf_attrs[] = {
                EGL_WIDTH, (int) IMAGE_WIDTH,
                EGL_HEIGHT, (int) IMAGE_HEIGHT,
                EGL_NONE
        };
        EGLSurface egl_surface = eglCreatePbufferSurface(egl_display, egl_config, p_buf_attrs);
        if (egl_surface == EGL_NO_SURFACE) {
            std::cerr << "eglCreatePbufferSurface failed" << "\n";
            return EXIT_FAILURE;
        }
        static EGLint context_attribs[] = {
                EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL_NONE
        };
        EGLContext egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT,
                                                  context_attribs);
        if (egl_context == EGL_NO_CONTEXT) {
            std::cerr << "eglCreateContext failed" << "\n";
            return EXIT_FAILURE;
        }
        return_value = eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

        /*
         * Step 2: Set up OpenCL
         */
        cl_platform_id platform;
        clGetPlatformIDs(1, &platform, NULL);
        static const cl_context_properties CONTEXT_PROPERTIES[] =
                {
                        CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
                        CL_GL_CONTEXT_KHR, (cl_context_properties) eglGetCurrentContext(),
                        CL_EGL_DISPLAY_KHR, (cl_context_properties) eglGetCurrentDisplay(),
                        0
                };
        cl_wrapper wrapper(CONTEXT_PROPERTIES);
        cl_program program = wrapper.make_program(PROGRAM_SOURCE, PROGRAM_SOURCE_LEN);
        cl_kernel kernel = wrapper.make_kernel("rgba_grayscale", program);
        cl_context context = wrapper.get_context();
        cl_command_queue queue = wrapper.get_command_queue();

        /*
         * Step 3: Set up GL
         */
        GLuint vertex_shader = cl_egl_sync_load_shader(GL_VERTEX_SHADER, VERTEX_SHADER_SOURCE);
        GLuint pixel_shader = cl_egl_sync_load_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SOURCE);
        GLuint shader_program = glCreateProgram();
        if (shader_program) {
            glAttachShader(shader_program, vertex_shader);
            glAttachShader(shader_program, pixel_shader);
            glLinkProgram(shader_program);
            GLint linkStatus = GL_FALSE;
            glGetProgramiv(shader_program, GL_LINK_STATUS, &linkStatus);
            if (linkStatus != GL_TRUE) {
                GLint buf_length = 0;
                glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &buf_length);
                if (buf_length) {
                    char *buf = (char *) malloc(buf_length);
                    if (buf) {
                        glGetProgramInfoLog(shader_program, buf_length, NULL, buf);
                        std::cerr << "Error: Could not link shader_program " << buf << "\n";
                        free(buf);
                    }
                }
                glDeleteProgram(shader_program);
                shader_program = 0;
            }
        }
        GLuint position_handle = glGetAttribLocation(shader_program, "position");
        glVertexAttribPointer(position_handle, 3, GL_FLOAT, GL_FALSE,
                              TRIANGLE_VERTICES_DATA_STRIDE_BYTES, TRIANGLE_VERTICES_DATA);
        GLuint texture_handle = glGetAttribLocation(shader_program, "texCoords");
        glVertexAttribPointer(texture_handle, 2, GL_FLOAT, GL_FALSE,
                              TRIANGLE_VERTICES_DATA_STRIDE_BYTES, &TRIANGLE_VERTICES_DATA[3]);

        /*
         * Step 3: Create a input and ouput 2D texture. Upload data to input texture
         */
        GLuint input_texture, output_texture;
        glGenTextures(1, &input_texture);
        glBindTexture(GL_TEXTURE_2D, input_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLubyte *input = (GLubyte *) malloc(IMAGE_WIDTH * IMAGE_HEIGHT * BYTES_PER_PIXEL);
        if (input == NULL) {
            std::cerr << "malloc failed" << "\n";
            return EXIT_FAILURE;
        }
        load_data(IMAGE_WIDTH, IMAGE_HEIGHT, (char *) input);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMAGE_WIDTH, IMAGE_HEIGHT, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, input);

        glGenTextures(1, &output_texture);
        glBindTexture(GL_TEXTURE_2D, output_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMAGE_WIDTH, IMAGE_HEIGHT, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);

        glViewport(0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnableVertexAttribArray(position_handle);
        glEnableVertexAttribArray(texture_handle);

        glUseProgram(shader_program);
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glBindTexture(GL_TEXTURE_2D, output_texture);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /*
         * Step 4: Create cl memory from GL textures
         */
        cl_int creturn = CL_SUCCESS;
        cl_mem shared_mem[2];
        shared_mem[0] = clCreateFromGLTexture(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0,
                                              input_texture, &creturn);
        if (creturn != 0) {
            std::cerr << "clCreateFromGLTexture failed with error " << creturn << "\n";
        }
        shared_mem[1] = clCreateFromGLTexture(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0,
                                              output_texture, &creturn);
        if (creturn != 0) {
            std::cerr << "clCreateFromGLTexture failed with error " << creturn << "\n";
        }

        /*
         * Step 5: Set kernel argument of input and output memory
         */
        creturn = clSetKernelArg(kernel, 0, sizeof(cl_mem), &shared_mem[0]);
        creturn |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &shared_mem[1]);
        if (creturn != CL_SUCCESS) {
            std::cerr << "clSetKernelArg failed with error " << creturn << "\n";
            return EXIT_FAILURE;
        }

        /*
         * Step 6: Draw on off screen buffer with the output generated by CL kernels.
         * Synchronization is done using CL events and EGL syncs
         */
        unsigned int frame_num;
        cl_event cl_event_cl_done = NULL, cl_event_gl_done = NULL;
        EGLSyncKHR EGLSync_cl_done, EGLSync_gl_done;
        size_t wg_size[] = {(size_t) IMAGE_WIDTH, (size_t) IMAGE_HEIGHT};
        for (frame_num = 0; frame_num < FRAME_COUNT; frame_num++) {
            if (cl_event_cl_done) {
                EGLAttrib fence_attribs[] =
                        {
                                EGL_CL_EVENT_HANDLE_KHR,
                                static_cast<EGLAttrib>((EGLAttrib) cl_event_cl_done),
                                EGL_NONE
                        };

                EGLSync_cl_done = p_eglCreateSync(egl_display, EGL_SYNC_CL_EVENT_KHR,
                                                  fence_attribs);
//            eglWaitSync(egl_display, EGLSync_cl_done, 0);

                p_eglDestroySync(egl_display, EGLSync_cl_done);
                clReleaseEvent(cl_event_cl_done);

                cl_event_cl_done = NULL;
            }

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            EGLSync_gl_done = p_eglCreateSync(egl_display, EGL_SYNC_FENCE, nullptr);
            cl_event_gl_done = clCreateEventFromEGLSyncKHR(context, EGLSync_gl_done, egl_display,
                                                           &creturn);
            creturn |= clEnqueueAcquireGLObjects(queue, 2, shared_mem, 1, &cl_event_gl_done, NULL);
            creturn |= clEnqueueNDRangeKernel(queue, kernel, 2, NULL, wg_size, NULL, 0, NULL, NULL);
            creturn |= clEnqueueReleaseGLObjects(queue, 2, shared_mem, 0, NULL, &cl_event_cl_done);
            if (creturn != CL_SUCCESS) {
                std::cerr << "CL processng failed with error " << creturn << "\n";
                return EXIT_FAILURE;
            }
            p_eglDestroySync(egl_display, EGLSync_gl_done);
            clReleaseEvent(cl_event_gl_done);
        }
        std::cout << "cl_egl_sync sample executed successfully." << "\n";

        /*
         * Step 7: Clean up resources that aren't automatically handled by cl_wrapper
         */
        clReleaseMemObject(shared_mem[0]);
        clReleaseMemObject(shared_mem[1]);

        glDeleteShader(vertex_shader);
        glDeleteShader(pixel_shader);
        glDeleteProgram(shader_program);
        glDeleteTextures(1, &input_texture);
        glDeleteTextures(1, &output_texture);

        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(egl_display, egl_context);
        eglDestroySurface(egl_display, egl_surface);
        eglTerminate(egl_display);
        free(input);

        return 0;
    }
}