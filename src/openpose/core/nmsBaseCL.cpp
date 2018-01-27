#include <openpose/core/common.hpp>
#include <openpose/core/nmsBase.hpp>

#ifdef USE_OPENCL
    #include <openpose/core/clManager.hpp>
#endif

#include <opencv2/opencv.hpp>
#include <bitset>

namespace op
{
    const std::string nmsOclCommonFunctions = MULTI_LINE_STRING(
        void nmsAccuratePeakPosition(__global const Type* sourcePtr, const int peakLocX, const int peakLocY,
                                     const int width, const int height, Type* fx, Type* fy, Type* fscore)
        {
            Type xAcc = 0.f;
            Type yAcc = 0.f;
            Type scoreAcc = 0.f;
            const int dWidth = 3;
            const int dHeight = 3;
            for (auto dy = -dHeight ; dy <= dHeight ; dy++)
            {
                const int y = peakLocY + dy;
                if (0 <= y && y < height) // Default height = 368
                {
                    for (int dx = -dWidth ; dx <= dWidth ; dx++)
                    {
                        const int x = peakLocX + dx;
                        if (0 <= x && x < width) // Default width = 656
                        {
                            const Type score = sourcePtr[y * width + x];
                            if (score > 0)
                            {
                                xAcc += (Type)x*score;
                                yAcc += (Type)y*score;
                                scoreAcc += score;
                            }
                        }
                    }
                }
            }

            *fx = xAcc / scoreAcc;
            *fy = yAcc / scoreAcc;
            *fscore = sourcePtr[peakLocY*width + peakLocX];
        }

        union DS {
          struct {
            short x;
            short y;
            float score;
          } ds;
          double dbl;
        };
    );

    typedef cl::KernelFunctor<cl::Buffer, cl::Buffer, int, int, float, int> NMSRegisterKernelFunctor;
    const std::string nmsRegisterKernel = MULTI_LINE_STRING(
        __kernel void nmsRegisterKernel(__global double* kernelPtr, __global const Type* sourcePtr,
                                           const int w, const int h, const Type threshold, const int debug)
        {
            int x = get_global_id(0);
            int y = get_global_id(1);
            int index = y*w + x;

            if (0 < x && x < (w-1) && 0 < y && y < (h-1))
            {
                const Type value = sourcePtr[index];
                if (value > threshold)
                {
                    const Type topLeft     = sourcePtr[(y-1)*w + x-1];
                    const Type top         = sourcePtr[(y-1)*w + x];
                    const Type topRight    = sourcePtr[(y-1)*w + x+1];
                    const Type left        = sourcePtr[    y*w + x-1];
                    const Type right       = sourcePtr[    y*w + x+1];
                    const Type bottomLeft  = sourcePtr[(y+1)*w + x-1];
                    const Type bottom      = sourcePtr[(y+1)*w + x];
                    const Type bottomRight = sourcePtr[(y+1)*w + x+1];

                    if (value > topLeft && value > top && value > topRight
                        && value > left && value > right
                        && value > bottomLeft && value > bottom && value > bottomRight)
                    {
                        Type fx = 0; Type fy = 0; Type fscore = 0;
                        nmsAccuratePeakPosition(sourcePtr, x, y, w, h, &fx, &fy, &fscore);
                        kernelPtr[index] = 255;
                        union DS ds;
                        ds.ds.x = (short)(fx+0.5);
                        ds.ds.y = (short)(fy+0.5);
                        ds.ds.score = fscore;
                        kernelPtr[index] = ds.dbl;
                    }
                    else
                        kernelPtr[index] = 0;
                }
                else
                    kernelPtr[index] = 0;
            }
            else if (x == 0 || x == (w-1) || y == 0 || y == (h-1))
                kernelPtr[index] = 0;
        }
    );

    template <typename T>
    void nmsOcl(T* targetPtr, double* kernelPtr, const T* const sourcePtr, const T threshold,
                const std::array<int, 4>& targetSize, const std::array<int, 4>& sourceSize, int gpuID)
    {
        try
        {
            //Forward_cpu(bottom, top);
            const auto num = sourceSize[0];
            const auto height = sourceSize[2];
            const auto width = sourceSize[3];
            const auto channels = targetSize[1];
            const auto targetPeaks = targetSize[2]; // 97
            const auto targetPeakVec = targetSize[3]; // 3
            const auto maxPeaks = targetSize[2]-1;
            const auto imageOffset = height * width;
            const auto offsetTarget = (maxPeaks+1)*targetSize[3];
            const auto targetChannelOffset = targetPeaks * targetPeakVec;

            // Get Kernel
            cl::Buffer sourcePtrBuffer = cl::Buffer((cl_mem)(sourcePtr), true);
            cl::Buffer kernelPtrBuffer = cl::Buffer((cl_mem)(kernelPtr), true);
            //cl::Buffer targetPtrBuffer = cl::Buffer((cl_mem)(targetPtr), true);
            auto nmsRegisterKernel = op::CLManager::getInstance(gpuID)->getKernelFunctorFromManager<op::NMSRegisterKernelFunctor, T>(
                        "nmsRegisterKernel",op::nmsOclCommonFunctions + op::nmsRegisterKernel);

            // log("num_b: " + std::to_string(bottom->shape(0)));       // = 1
            // log("channel_b: " + std::to_string(bottom->shape(1)));   // = 57 = 18 body parts + bkg + 19x2 PAFs
            // log("height_b: " + std::to_string(bottom->shape(2)));    // = 368 = height
            // log("width_b: " + std::to_string(bottom->shape(3)));     // = 656 = width
            // log("num_t: " + std::to_string(top->shape(0)));       // = 1
            // log("channel_t: " + std::to_string(top->shape(1)));   // = 18 = numberParts
            // log("height_t: " + std::to_string(top->shape(2)));    // = 97 = maxPeople + 1
            // log("width_t: " + std::to_string(top->shape(3)));     // = 3 = [x, y, score]
            // log("");

            // Temp DS
            cv::Mat kernelCPU(cv::Size(width, height),CV_64FC1,cv::Scalar(0));
            union DS {
              struct {
                short x;
                short y;
                float score;
              } ds;
              double dbl;
            };

            for (auto n = 0; n < num; n++)
            {
                for (auto c = 0; c < channels; c++)
                {
                    // log("channel: " + std::to_string(c));
                    const auto offsetChannel = (n * channels + c);

                    // CL Data
                    cl_buffer_region kernelRegion = op::CLManager::getBufferRegion<double>(offsetChannel * imageOffset, imageOffset);
                    cl::Buffer kernelBuffer = kernelPtrBuffer.createSubBuffer(CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &kernelRegion);
                    cl_buffer_region sourceRegion = op::CLManager::getBufferRegion<T>(offsetChannel * imageOffset, imageOffset);
                    cl::Buffer sourceBuffer = sourcePtrBuffer.createSubBuffer(CL_MEM_READ_ONLY, CL_BUFFER_CREATE_TYPE_REGION, &sourceRegion);

                    // Run Kernel
                    bool debug = false;
                    nmsRegisterKernel(cl::EnqueueArgs(op::CLManager::getInstance(gpuID)->getQueue(), cl::NDRange(width, height)),
                                      kernelBuffer, sourceBuffer, width, height, threshold, debug);
                    op::CLManager::getInstance(gpuID)->getQueue().enqueueReadBuffer(kernelBuffer, CL_TRUE, 0, sizeof(double) *  width * height, &kernelCPU.at<double>(0));

                    // Find Locations
                    std::vector<cv::Point> locations;
                    double* currKernelPtr = &kernelCPU.at<double>(0);
                    for(int y=0; y<height; y++){
                        for(int x=0; x<width; x++){
                            int index = y*width +x;
                            if(currKernelPtr[index]){
                                locations.push_back(cv::Point(x,y));
                            }
                        }
                    }

                    // Save to Map
                    auto currentPeakCount = 1;
                    auto* currTargetPtr = &targetPtr[c*targetChannelOffset];
                    for(auto point : locations){
                        int index = point.y*width + point.x;
                        if (currentPeakCount < targetPeaks)
                        {
                            DS ds;
                            ds.dbl = currKernelPtr[index];
                            T* output = &currTargetPtr[currentPeakCount*3];
                            output[0] = ds.ds.x; output[1] = ds.ds.y; output[2] = ds.ds.score;
                            currentPeakCount++;
                        }
                    }
                    currTargetPtr[0] = currentPeakCount-1;
                }
            }
        }
        catch (const std::exception& e)
        {
            error(e.what(), __LINE__, __FUNCTION__, __FILE__);
        }
    }

    template void nmsOcl(float* targetPtr, double* kernelPtr, const float* const sourcePtr, const float threshold,
                         const std::array<int, 4>& targetSize, const std::array<int, 4>& sourceSize, int gpuID);
    template void nmsOcl(double* targetPtr, double* kernelPtr, const double* const sourcePtr, const double threshold,
                         const std::array<int, 4>& targetSize, const std::array<int, 4>& sourceSize, int gpuID);
}