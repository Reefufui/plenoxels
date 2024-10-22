struct Plenoxel {
    float density;
    float sh_r [9];
    float sh_g [9];
    float sh_b [9];
};

struct PushConstant {
    float4 boxMax;
    float4 boxMin;
    column_major float4x4 worldProjInv;
    column_major float4x4 worldViewInv;
    uint gridSize;
    uint height;
    uint width;
};

[[vk::push_constant]] ConstantBuffer<PushConstant> pc;
[[vk::binding(0)]] RWStructuredBuffer<Plenoxel> plenoxels;
[[vk::binding(1)]] RWStructuredBuffer<uint> out_color;

float3 EyeRayDir (float x, float y) {
    float4 pos = float4 (2.0f * x - 1.0f, 2.0f * y - 1.0f, 0.0f, 1.0f);
    pos = mul (pc.worldProjInv, pos);
    pos /= pos.w;
    return normalize (pos.xyz);
}

uint RealColorToUint32 (float4 real_color) {
    float r = real_color [0] * 255.0f;
    float g = real_color [1] * 255.0f;
    float b = real_color [2] * 255.0f;
    float a = real_color [3] * 255.0f;

    uint red   = (uint) r;
    uint green = (uint) g;
    uint blue  = (uint) b;
    uint alpha = (uint) a;

    return red | (green << 8) | (blue << 16) | (alpha << 24);
}

void TransformRay (inout float3 rayPos, inout float3 rayDir) {
    float4 rayPosTransformed = mul (pc.worldViewInv, float4 (rayPos, 1.0f));
    float4 rayDirTransformed = mul (pc.worldViewInv, float4 (rayDir, 0.0f));

    /* rayPos = to_float3 (rayPosTransformed); */
    /* rayDir = to_float3 (normalize (rayDirTransformed)); */
    rayPos = rayPosTransformed.xyz;
    rayDir = normalize (rayDirTransformed).xyz;
}

float2 RayBoxIntersection (float3 ray_pos, float3 ray_dir) {
    ray_dir.x = 1.0f / ray_dir.x; // may precompute if intersect many boxes
    ray_dir.y = 1.0f / ray_dir.y; // may precompute if intersect many boxes
    ray_dir.z = 1.0f / ray_dir.z; // may precompute if intersect many boxes

    float lo = ray_dir.x * (pc.boxMin.x - ray_pos.x);
    float hi = ray_dir.x * (pc.boxMax.x - ray_pos.x);

    float tmin = min (lo, hi);
    float tmax = max (lo, hi);

    float lo1 = ray_dir.y * (pc.boxMin.y - ray_pos.y);
    float hi1 = ray_dir.y * (pc.boxMax.y - ray_pos.y);

    tmin = max (tmin, min (lo1, hi1));
    tmax = min (tmax, max (lo1, hi1));

    float lo2 = ray_dir.z * (pc.boxMin.z - ray_pos.z);
    float hi2 = ray_dir.z * (pc.boxMax.z - ray_pos.z);

    tmin = max (tmin, min (lo2, hi2));
    tmax = min (tmax, max (lo2, hi2));

    return float2 (tmin, tmax);
}

int getIndex (int x, int y, int z) {
    return x + pc.gridSize * (y + pc.gridSize * z);
};

Plenoxel lerp (Plenoxel a, Plenoxel b, float t) {
  Plenoxel result;
  result.density = lerp (a.density, b.density, t);
  for (int i = 0; i < 9; ++i) {
    result.sh_r [i] = lerp (a.sh_r [i], b.sh_r [i], t);
    result.sh_g [i] = lerp (a.sh_g [i], b.sh_g [i], t);
    result.sh_b [i] = lerp (a.sh_b [i], b.sh_b [i], t);
  }
  return result;
}

Plenoxel trilinearInterpolation (float3 point) {
    float3 xyz = point * (pc.gridSize - 1);
    int3 xyz0 = int3 (xyz);
    int3 xyz1 = xyz0 + 1;

    xyz0 = clamp (xyz0, 0, pc.gridSize - 1);
    xyz1 = clamp (xyz1, 0, pc.gridSize - 1);

    float3 w_xyz = xyz - float3 (xyz0);

    Plenoxel c000 = plenoxels [getIndex (xyz0.x, xyz0.y, xyz0.z)];
    Plenoxel c100 = plenoxels [getIndex (xyz1.x, xyz0.y, xyz0.z)];
    Plenoxel c010 = plenoxels [getIndex (xyz0.x, xyz1.y, xyz0.z)];
    Plenoxel c110 = plenoxels [getIndex (xyz1.x, xyz1.y, xyz0.z)];
    Plenoxel c001 = plenoxels [getIndex (xyz0.x, xyz0.y, xyz1.z)];
    Plenoxel c101 = plenoxels [getIndex (xyz1.x, xyz0.y, xyz1.z)];
    Plenoxel c011 = plenoxels [getIndex (xyz0.x, xyz1.y, xyz1.z)];
    Plenoxel c111 = plenoxels [getIndex (xyz1.x, xyz1.y, xyz1.z)];
    Plenoxel c00 = lerp (c000, c100, w_xyz.x);
    Plenoxel c10 = lerp (c010, c110, w_xyz.x);
    Plenoxel c01 = lerp (c001, c101, w_xyz.x);
    Plenoxel c11 = lerp (c011, c111, w_xyz.x);
    Plenoxel c0 = lerp (c00, c10, w_xyz.y);
    Plenoxel c1 = lerp (c01, c11, w_xyz.y);
    Plenoxel result = lerp (c0, c1, w_xyz.z);

    return result;
}

float evaluateSH (float coeffs [9], float3 rayDir) {
    float sh [9];
    float x = rayDir.x, y = rayDir.y, z = rayDir.z, z2 = z * z;
    float c0, c1, s0, s1, tmp_a, tmp_b, tmp_c;

    sh[0] = 0.28209479177387814;
    sh[2] = z * 0.488602511902919923;
    sh[6] = z2 * 0.94617469575756008 + -0.315391565252520045;
    c0 = x;
    s0 = y;

    tmp_a = -0.488602511902919978;
    sh[3] = tmp_a * c0;
    sh[1] = tmp_a * s0;
    tmp_b = z * -1.09254843059207896;
    sh[7] = tmp_b * c0;
    sh[5] = tmp_b * s0;
    c1 = x * c0 - y * s0;
    s1 = x * s0 + y * c0;

    tmp_c = 0.546274215296039478;
    sh [8] = tmp_c * c1;
    sh [4] = tmp_c * s1;

    float sum = 0.0f;
    for (int i = 0; i < 9; i++) {
        sum += coeffs [i] * sh [i];
    }

    return sum;
}

float3 VolumetricRendering (float3 origin, float3 dir, float tmin, float tmax) {
	  float3 color = float3 (0.0f);
	  float T = 1.0f;
	  float t = tmin;

	  float delta = 1.0f / pc.gridSize;

	  while (t < tmax && T > 0.1f) {
	      float3 point = origin + t * dir;
        Plenoxel sample = trilinearInterpolation (point);

        float sigma = sample.density;

        if (sigma < 0.0)
            sigma = 0.0;

        float r = clamp (evaluateSH (sample.sh_r, dir), 0.0f, 1.0f);
        float g = clamp (evaluateSH (sample.sh_g, dir), 0.0f, 1.0f);
        float b = clamp (evaluateSH (sample.sh_b, dir), 0.0f, 1.0f);

        color += T * (1 - exp (-sigma * delta)) * float3 (r, g, b);
        T *= exp (-sigma * delta);

	      t += delta;
	  }

	  return clamp (color, 0.0f, 1.0f);
}

[shader ("compute")]
[numthreads (16, 16, 1)]
void main (uint3 threadIdx : SV_DispatchThreadID) {
    uint x = threadIdx.x;
    uint y = threadIdx.y;
    if (x >= pc.width) return;
    if (y >= pc.height) return;

    float3 rayDir = EyeRayDir ((float (x) + 0.5f) / float (pc.width), (float (y) + 0.5f) / float (pc.height)); 
    float3 rayPos = float3 (0.0f, 0.0f, 0.0f);

    TransformRay (rayPos, rayDir);

    float2 tNearAndFar = RayBoxIntersection (rayPos, rayDir);

    float4 resColor = float4 (0.0f);
    if (tNearAndFar.x < tNearAndFar.y && tNearAndFar.x > 0.0f) {
	    resColor = float4 (VolumetricRendering (rayPos, rayDir, tNearAndFar.x, tNearAndFar.y), 1.0f);
    }

    out_color [y * pc.width + x] = RealColorToUint32 (resColor);
}

