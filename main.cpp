#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "example_tracer/plenoxels.h"
#include "Image2d.h"

#ifdef USE_VULKAN
#include "example_tracer/plenoxels_gpu.h"
#endif // USE_VULKAN

int main (int argc, const char** argv) {
  const uint WIN_WIDTH  = 256;
  const uint WIN_HEIGHT = 256;

  std::shared_ptr <Plenoxels> pImpl = nullptr;
#ifdef USE_VULKAN
  bool onGPU = true;
  pImpl = std::make_shared <PlenoxelsGPU> ();
#else // USE_VULKAN
  bool onGPU = false;
  pImpl = std::make_shared <Plenoxels> ();
#endif // USE_VULKAN

  pImpl->InitGrid (128);
  pImpl->LoadGrid ("./model.dat");
  pImpl->SetWorldViewProjMatrix (perspectiveMatrix (45, 1, 0.1, 100));

  for (int cam_id = 4; cam_id < 5; ++cam_id) {
    float4x4 viewMat = lookAt (
        float3 (0.0, 0.0, 1.3),
        float3 (0.0, 0.0, 0.0),
        float3 (0.0, 1.0, 0.0)
        )
      * rotate4x4Y (-float (360.0 / 7 * cam_id) * DEG_TO_RAD)
      * translate4x4 (float3 (-0.5, -0.5, -0.5));
    pImpl->SetWorldViewMatrix (viewMat);

    std::vector <uint32_t> ref = pImpl->RayMarch (WIN_WIDTH, WIN_HEIGHT);
    std::vector <uint32_t> gen = pImpl->Forward (WIN_WIDTH, WIN_HEIGHT);

    float timings [4] = {0, 0, 0, 0};
    pImpl->GetExecutionTime ("RayMarch", timings);

    std::stringstream refFilename;
    refFilename << std::fixed << std::setprecision (2) << "input/" << ((onGPU) ? "gpu" : "cpu") << "_" << cam_id << ".bmp";
    LiteImage::SaveBMP (refFilename.str ().c_str (), ref.data (), WIN_WIDTH, WIN_HEIGHT);

    std::stringstream genFilename;
    genFilename << std::fixed << std::setprecision (2) << "output/" << ((onGPU) ? "gpu" : "cpu") << "_" << cam_id << ".bmp";
    LiteImage::SaveBMP (genFilename.str ().c_str (), gen.data (), WIN_WIDTH, WIN_HEIGHT);

    std::cout << "img no. = " << cam_id << ", timeRender = " << timings [0] << " ms, timeCopy = " <<  timings [1] + timings [2] << " ms " << std::endl;
  }

  pImpl = nullptr;
  return 0;
}

