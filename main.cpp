#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "example_tracer/example_tracer.h"
#include "Image2d.h"

#ifdef USE_VULKAN
  #include "vk_context.h"
  std::shared_ptr <Plenoxels> CreatePlenoxels_Generated (vk_utils::VulkanContext a_ctx, size_t a_maxThreadsGenerated);
  #ifndef NDEBUG
    const bool enableValidationLayers = true;
  #else // NDEBUG
    const bool enableValidationLayers = false;
  #endif // NDEBUG
  const bool onGPU = true;
#else // USE_VULKAN
  const bool onGPU = false;
#endif // USE_VULKAN

int main (int argc, const char** argv) {
  const uint WIN_WIDTH  = 256;
  const uint WIN_HEIGHT = 256;
  std::vector <uint> pixelData (WIN_WIDTH * WIN_HEIGHT);

  std::shared_ptr <Plenoxels> pImpl = nullptr;
#ifdef USE_VULKAN
  auto ctx = vk_utils::globalContextGet (enableValidationLayers, 0);
  pImpl = CreatePlenoxels_Generated (ctx, WIN_WIDTH * WIN_HEIGHT);
#else // USE_VULKAN
  pImpl = std::make_shared <Plenoxels> ();
#endif // USE_VULKAN

  pImpl->CommitDeviceData ();
  pImpl->InitGrid (128);
  pImpl->LoadGrid ("../model.dat");
  pImpl->SetBoundingBox (float3 (0, 0, 0), float3 (1, 1, 1));
  pImpl->SetWorldViewMProjatrix (perspectiveMatrix (45, 1, 0.1, 100));

  for (int cam_id = 0; cam_id < 7; ++cam_id) {
    float4x4 viewMat = lookAt (
        float3 (0.0, 0.0, 1.3),
        float3 (0.0, 0.0, 0.0),
        float3 (0.0, 1.0, 0.0)
        )
      * rotate4x4Y (-float (360.0 / 7 * cam_id) * DEG_TO_RAD)
      * translate4x4 (float3 (-0.5, -0.5, -0.5));
    pImpl->SetWorldViewMatrix (viewMat);
    pImpl->UpdateMembersPlainData ();

    pImpl->Forward (pixelData.data (), WIN_WIDTH, WIN_HEIGHT);

    float timings [4] = {0, 0, 0, 0};
    pImpl->GetExecutionTime ("RayMarch", timings);

    std::stringstream strOut;
    strOut << std::fixed << std::setprecision (2) << ((onGPU) ? "out_gpu_" : "out_cpu_") << cam_id << ".bmp";
    LiteImage::SaveBMP (strOut.str ().c_str (), pixelData.data (), WIN_WIDTH, WIN_HEIGHT);
    std::cout << "img no. = " << cam_id << ", timeRender = " << timings [0] << " ms, timeCopy = " <<  timings [1] + timings [2] << " ms " << std::endl;
  }

  pImpl = nullptr;
  return 0;
}

