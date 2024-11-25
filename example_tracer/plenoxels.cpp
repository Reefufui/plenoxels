#include <vector>
#include <chrono>
#include <string>

#include "plenoxels.h"

extern "C" float __enzyme_autodiff(void*, ...);

// From Mitsuba 3
void sh_eval_2 (const float3 &d, float *out) {
    float x = d.x, y = d.y, z = d.z, z2 = z * z;
    float c0, c1, s0, s1, tmp_a, tmp_b, tmp_c;

    out[0] = 0.28209479177387814;
    out[2] = z * 0.488602511902919923;
    out[6] = z2 * 0.94617469575756008 + -0.315391565252520045;
    c0 = x;
    s0 = y;

    tmp_a = -0.488602511902919978;
    out[3] = tmp_a * c0;
    out[1] = tmp_a * s0;
    tmp_b = z * -1.09254843059207896;
    out[7] = tmp_b * c0;
    out[5] = tmp_b * s0;
    c1 = x * c0 - y * s0;
    s1 = x * s0 + y * c0;

    tmp_c = 0.546274215296039478;
    out [8] = tmp_c * c1;
    out [4] = tmp_c * s1;
}

float eval_sh (float* sh, float3 rayDir) {
  float sh_coeffs [SH_WIDTH];
  sh_eval_2 (rayDir, sh_coeffs);

  float sum = 0.0f;
  for (int i = 0; i < SH_WIDTH; i++) {
    sum += sh [i] * sh_coeffs [i];
  }

  return sum;
}

float2 RayBoxIntersection (float3 ray_pos, float3 ray_dir, float3 boxMin, float3 boxMax) {
  ray_dir.x = 1.0f / ray_dir.x; // may precompute if intersect many boxes
  ray_dir.y = 1.0f / ray_dir.y; // may precompute if intersect many boxes
  ray_dir.z = 1.0f / ray_dir.z; // may precompute if intersect many boxes

  float lo = ray_dir.x * (boxMin.x - ray_pos.x);
  float hi = ray_dir.x * (boxMax.x - ray_pos.x);
  
  float tmin = std::min (lo, hi);
  float tmax = std::max (lo, hi);

  float lo1 = ray_dir.y * (boxMin.y - ray_pos.y);
  float hi1 = ray_dir.y * (boxMax.y - ray_pos.y);

  tmin = std::max (tmin, std::min (lo1, hi1));
  tmax = std::min (tmax, std::max (lo1, hi1));

  float lo2 = ray_dir.z * (boxMin.z - ray_pos.z);
  float hi2 = ray_dir.z * (boxMax.z - ray_pos.z);

  tmin = std::max (tmin, std::min (lo2, hi2));
  tmax = std::min (tmax, std::max (lo2, hi2));
  
  return float2 (tmin, tmax);
}

static inline float3 EyeRayDir (float x, float y, float4x4 a_mViewProjInv) {
  float4 pos = float4 (2.0f * x - 1.0f, 2.0f * y - 1.0f, 0.0f, 1.0f);
  pos = a_mViewProjInv * pos;
  pos /= pos.w;
  return normalize (to_float3 (pos));
}

static inline void transform_ray3f (float4x4 a_mWorldViewInv, float3* ray_pos, float3* ray_dir) {
  float4 rayPosTransformed = a_mWorldViewInv * to_float4 (*ray_pos, 1.0f);
  float4 rayDirTransformed = a_mWorldViewInv * to_float4 (*ray_dir, 0.0f);
  
  (*ray_pos) = to_float3 (rayPosTransformed);
  (*ray_dir) = to_float3 (normalize (rayDirTransformed));
}

std::array <float, PLENOXEL_SIZE> lerp (const float* a, const float* b, const float t) {
  std::array<float, PLENOXEL_SIZE> ret = {};

  for (size_t i = 0; i < PLENOXEL_SIZE; i++)
    ((float*)ret.data())[i] = LiteMath::lerp(((float*)a)[i], ((float*)b)[i], t);

  return ret;
}

Plenoxel trilinearInterpolation (const float* grid, int gridSize, float3 point) {
  float3 xyz = point * (gridSize - 1);
  int3 xyz0 = int3 (xyz);
  int3 xyz1 = xyz0 + 1;

  xyz0 = clamp (xyz0, 0, gridSize - 1);
  xyz1 = clamp (xyz1, 0, gridSize - 1);

  float3 w_xyz = xyz - float3 (xyz0);

  auto getIndex = [gridSize] (int x, int y, int z) -> size_t {
    return x + gridSize * (y + gridSize * z);
  };

  float* c000 = grid [PLENOXEL_SIZE * getIndex (xyz0.x, xyz0.y, xyz0.z)];
  float* c100 = grid [PLENOXEL_SIZE * getIndex (xyz1.x, xyz0.y, xyz0.z)];
  float* c010 = grid [PLENOXEL_SIZE * getIndex (xyz0.x, xyz1.y, xyz0.z)];
  float* c110 = grid [PLENOXEL_SIZE * getIndex (xyz1.x, xyz1.y, xyz0.z)];
  float* c001 = grid [PLENOXEL_SIZE * getIndex (xyz0.x, xyz0.y, xyz1.z)];
  float* c101 = grid [PLENOXEL_SIZE * getIndex (xyz1.x, xyz0.y, xyz1.z)];
  float* c011 = grid [PLENOXEL_SIZE * getIndex (xyz0.x, xyz1.y, xyz1.z)];
  float* c111 = grid [PLENOXEL_SIZE * getIndex (xyz1.x, xyz1.y, xyz1.z)];
  Plenoxel c00 = lerp (c000, c100, w_xyz.x);
  Plenoxel c10 = lerp (c010, c110, w_xyz.x);
  Plenoxel c01 = lerp (c001, c101, w_xyz.x);
  Plenoxel c11 = lerp (c011, c111, w_xyz.x);
  Plenoxel c0 = lerp (c00.data (), c10.data (), w_xyz.y);
  Plenoxel c1 = lerp (c01.data (), c11.data (), w_xyz.y);
  Plenoxel result = lerp (c0.data (), c1.data (), w_xyz.z);

  return result;
}

float ReLU (float x) {
  return LiteMath::max (0.0f, x);
}

float3 VolumetricRendering (float3 origin, float3 dir, float tmin, float tmax, const std::vector <float>& grid, int gridSize) {
	float3 color = float3 (0.0f);
	float T = 1.0f;
	float t = tmin;

	float delta = 1.0f / gridSize;
	
	while (t < tmax && T > 0.1f) {
	  float3 point = origin + t * dir;
    Plenoxel sample = trilinearInterpolation (grid, gridSize, point);

    float sigma = ReLU (sample [0]);

    float r = clamp (eval_sh (&sample [1], dir), 0.0f, 1.0f);
    float g = clamp (eval_sh (&sample [1 + SH_WIDTH], dir), 0.0f, 1.0f);
    float b = clamp (eval_sh (&sample [1 + 2 * SH_WIDTH], dir), 0.0f, 1.0f);

    color += T * (1 - exp (-sigma * delta)) * float3 (r, g, b);
    T *= exp (-sigma * delta);

	  t += delta;
	}
	
	return clamp (color, 0.0f, 1.0f);
}

static inline uint32_t RealColorToUint32 (float4 real_color) {
  float r = real_color [0] * 255.0f;
  float g = real_color [1] * 255.0f;
  float b = real_color [2] * 255.0f;
  float a = real_color [3] * 255.0f;

  uint32_t red   = (uint32_t) r;
  uint32_t green = (uint32_t) g;
  uint32_t blue  = (uint32_t) b;
  uint32_t alpha = (uint32_t) a;

  return red | (green << 8) | (blue << 16) | (alpha << 24);
}

std::vector<uint32_t> Plenoxels::kernel2D_Forward (uint32_t width, uint32_t height) {
  std::vector<uint32_t> out_color (width * height);
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      float3 rayDir = EyeRayDir ((float (x) + 0.5f) / float (width), (float (y) + 0.5f) / float (height), m_worldViewProjInv); 
      float3 rayPos = float3 (0.0f, 0.0f, 0.0f);

      transform_ray3f (m_worldViewInv, &rayPos, &rayDir);
      
      float2 tNearAndFar = RayBoxIntersection (rayPos, rayDir, float3 (0, 0, 0), float3 (1, 1, 1));
      
      float4 resColor (0.0f);
      if (tNearAndFar.x < tNearAndFar.y && tNearAndFar.x > 0.0f) {
	      resColor = to_float4 (VolumetricRendering (rayPos, rayDir, tNearAndFar.x, tNearAndFar.y, m_grid, m_gridSize), 1.0f);
      }
      
      out_color [y * width + x] = RealColorToUint32 (resColor);
    }
  }
  return out_color;
}

std::vector<uint32_t> Plenoxels::Forward (uint32_t width, uint32_t height) { 
  auto start = std::chrono::high_resolution_clock::now ();
  auto result = kernel2D_Forward (width, height);
  m_forwardTime = float (std::chrono::duration_cast <std::chrono::microseconds> (std::chrono::high_resolution_clock::now () - start).count ()) / 1000.f;
  return result;
}  

void Plenoxels::GetExecutionTime(const char* a_funcName, float a_out[4]) {
  if(std::string(a_funcName) == "Forward")
    a_out[0] =  m_forwardTime;
}

float4 RayMarchConstantFog(float tmin, float tmax, float& alpha)
{
  float dt = 0.01f;
	float t  = tmin;

	alpha = 1.0f;
	float4 color = float4(0.0f);

	while(t < tmax && alpha > 0.01f)
	{
	  float a = 0.01f;
	  color += a*alpha*float4(1.0f,1.0f,0.0f,0.0f);
	  alpha *= (1.0f-a);
	  t += dt;
	}

	return color;
}

std::vector<uint32_t> Plenoxels::kernel2D_RayMarch (uint32_t width, uint32_t height) {
  std::vector<uint32_t> out_color (width * height);
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      float3 rayDir = EyeRayDir((float(x) + 0.5f) / float(width), (float(y) + 0.5f) / float(height), m_worldViewProjInv); 
      float3 rayPos = float3(0.0f, 0.0f, 0.0f);

      transform_ray3f(m_worldViewInv, &rayPos, &rayDir);
      
      float2 tNearAndFar = RayBoxIntersection(rayPos, rayDir, float3(0.2,0.2,0.2), float3(0.8,0.8,0.8));
      
      float4 resColor(0.0f);
      if(tNearAndFar.x < tNearAndFar.y && tNearAndFar.x > 0.0f)
      {
        float alpha = 1.0f;
	      resColor = RayMarchConstantFog(tNearAndFar.x, tNearAndFar.y, alpha);
      }
      
      out_color[y*width+x] = RealColorToUint32(resColor);
    }
  }
  return out_color;
}

std::vector<uint32_t> Plenoxels::RayMarch (uint32_t width, uint32_t height) { 
  auto result = kernel2D_RayMarch (width, height);
  return result;
}  

static inline float4 Uint32ToRealColor (uint color)
{
  float r = (color & 0x000F) / 255.0f;
  float g = ((color >> 8) & 0x000F) / 255.0f;
  float b = ((color >> 16) & 0x000F) / 255.0f;
  float a = ((color >> 24) & 0x000F) / 255.0f;

  return float4(r, g, b, a);
}

