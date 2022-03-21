//
// Created by dhruva on 12-05-2021.
//

#ifndef ANDROID_SHADER_DEMO_JNI_CL_CODE_H
#define ANDROID_SHADER_DEMO_JNI_CL_CODE_H
#include <fstream>
#include "cl_wrapper.h"
#include "CL/cl.hpp"
#define LOG_TAG    "cl_code.hpp"

#define DPRINTF(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)

#define IPRINTF(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

#define EPRINTF(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)



#if 0
void test_cl_image()
{
   static const char* PROGRAM_IMAGE_2D_COPY_SOURCE[] = {
           "__constant sampler_t imageSampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;\n",
           /* Copy input 2D image to output 2D image */
           "__kernel void image2dCopy(__read_only image2d_t input,\n",
           " __write_only image2d_t output )\n",
           "{\n",
           "    int2 coord = (int2)(get_global_id(0), get_global_id(1));\n",
           "    uint4 temp = read_imageui(input, imageSampler, coord);\n",
           "    write_imageui(output, coord, temp);\n",
           "}"
   };

   cl_wrapper       wrapper;
   static const cl_uint PROGRAM_IMAGE_2D_COPY_SOURCE_LEN = sizeof(PROGRAM_IMAGE_2D_COPY_SOURCE) / sizeof(const char*);
   cl_context       context = wrapper.get_context();
   cl_command_queue command_queue = wrapper.get_command_queue();
   cl_program       program_image_2d_program = wrapper.make_program(PROGRAM_IMAGE_2D_COPY_SOURCE, PROGRAM_IMAGE_2D_COPY_SOURCE_LEN);
   cl_kernel        program_image_2d_kernel = wrapper.make_kernel("image2dCopy", program_image_2d_program);

   std::string src_filename = std::string("/storage/emulated/0/opencvTesting/out148Yonly.yuv");
   std::ifstream fin(src_filename, std::ios::binary);
   if (!fin) {
      DPRINTF("Couldn't open file %s", src_filename.c_str());
      std::exit(EXIT_FAILURE);
   }
   const auto        fin_begin = fin.tellg();
   fin.seekg(0, std::ios::end);
   const auto        fin_end = fin.tellg();
   const size_t      buf_size = static_cast<size_t>(fin_end - fin_begin);
   std::vector<char> buf(buf_size);
   fin.seekg(0, std::ios::beg);
   fin.read(buf.data(), buf_size);

   cl_int status = CL_SUCCESS;
   // Create and initialize image objects
   cl_image_desc imageDesc;
   memset(&imageDesc, '\0', sizeof(cl_image_desc));
   imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
   imageDesc.image_width = 1440;
   imageDesc.image_height = 1080;

   cl_image_format imageFormat;
   imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
   imageFormat.image_channel_order = CL_R;
   // Create 2D input image
   cl_mem inputImage2D;
   cl_mem outputImage2D;

   inputImage2D = clCreateImage(context,
                                CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                &imageFormat,
                                &imageDesc,
                                buf.data(),
                                &status);

   // Create 2D output image
   outputImage2D = clCreateImage(context,
                                 CL_MEM_WRITE_ONLY,
                                 &imageFormat,
                                 &imageDesc,
                                 0,
                                 &status);


   status = clSetKernelArg(
           program_image_2d_kernel,
           0,
           sizeof(cl_mem),
           &inputImage2D);

   status = clSetKernelArg(
           program_image_2d_kernel,
           1,
           sizeof(cl_mem),
           &outputImage2D);

   size_t globalThreads[] = { 1440, 1080 };

   struct timeval startNDKKernel, endNDKKernel;
   gettimeofday(&startNDKKernel, NULL);
   status = clEnqueueNDRangeKernel(
           command_queue,
           program_image_2d_kernel,
           2,
           NULL,
           globalThreads,
           0,
           0,
           NULL,
           0);
   clFinish(command_queue);
   gettimeofday(&endNDKKernel, NULL);
   DPRINTF("time taken :%ld",((endNDKKernel.tv_sec * 1000000 + endNDKKernel.tv_usec)- (startNDKKernel.tv_sec * 1000000 + startNDKKernel.tv_usec)));
   // Enqueue Read Image
   size_t origin[] = { 0, 0, 0 };
   size_t region[] = { 1440, 1080, 1 };

   unsigned char *outputImageData2D = (unsigned char*)malloc(1440* 1080);
   // Read output of 2D copy
   status = clEnqueueReadImage(command_queue,
                               outputImage2D,
                               1,
                               origin,
                               region,
                               0,
                               0,
                               outputImageData2D,
                               0, 0, 0);

   std::string filename("/storage/emulated/0/opencvTesting/output_copy2.yuv");
   std::ofstream fout(filename, std::ios::binary);
   if (!fout)
   {
      std::cerr << "Couldn't open file " << filename << "\n";
      std::exit(EXIT_FAILURE);
   }

   char* output_image_U8 = new char[buf_size];

   for (int pix_i = 0; pix_i < buf_size; pix_i++)
      output_image_U8[pix_i] = (unsigned char)outputImageData2D[pix_i];

   fout.write(output_image_U8, buf_size);
   delete[] output_image_U8;
   free(outputImageData2D);
   fout.close();
   clReleaseMemObject(outputImage2D);
   clReleaseMemObject(inputImage2D);
}
#endif
#endif //ANDROID_SHADER_DEMO_JNI_CL_CODE_H
