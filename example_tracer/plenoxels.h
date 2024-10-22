#pragma once

#include <cstdint>
#include <fstream>
#include <fstream>
#include <iostream>
#include <vector>

#include "LiteMath.h"
using namespace LiteMath;

const size_t SH_WIDTH = 9;

struct Plenoxel {
  float density;
  float sh_r[SH_WIDTH];
  float sh_g[SH_WIDTH];
  float sh_b[SH_WIDTH];
};

struct BoundingBox {
  float3 min;
  float3 max;
};

class Plenoxels {
public:

  Plenoxels () {
    const float4x4 view = lookAt (float3 (0, 1.5, -3), float3 (0, 0, 0), float3 (0, 1, 0));
    const float4x4 proj = perspectiveMatrix (90.0f, 1.0f, 0.1f, 100.0f);
    m_worldViewInv      = inverse4x4 (view); 
    m_worldViewProjInv  = inverse4x4 (proj); 
  }

  virtual void InitGrid (const float a_gridSize) {
    this->m_gridSize = a_gridSize;
    this->m_grid.resize (m_gridSize * m_gridSize * m_gridSize);

    for (size_t i = 0; i < m_gridSize * m_gridSize * m_gridSize; i++) {
      m_grid[i].density = 0.01;
      for (size_t j = 0; j < SH_WIDTH; j++) {
        m_grid[i].sh_r[j] = 0.1;
        m_grid[i].sh_g[j] = 0.1;
        m_grid[i].sh_b[j] = 0.1;
      }
    }
  }

  virtual void LoadGrid (std::string fileName) {
    std::ifstream fin (fileName, std::ios::in | std::ios::binary);
    fin.read ((char*)&this->m_grid[0], this->m_grid.size () * sizeof (Plenoxel));
    fin.close ();
  }

  virtual void SetBoundingBox (const float3 boxMin, const float3 boxMax) {
    m_sceneBoundingBox.min = boxMin;
    m_sceneBoundingBox.max = boxMax;
  }

  virtual void SetWorldViewProjMatrix (const float4x4& a_mat) {
    m_worldViewProjInv = inverse4x4 (a_mat);
  }

  virtual void SetWorldViewMatrix (const float4x4& a_mat) {
    m_worldViewInv = inverse4x4 (a_mat);
  }

  virtual std::vector<uint32_t> kernel2D_Forward (uint32_t width, uint32_t height);
  std::vector<uint32_t> Forward (uint32_t width, uint32_t height);  

  void GetExecutionTime (const char* a_funcName, float a_out[4]);

protected:
  float4x4 m_worldViewProjInv;
  float4x4 m_worldViewInv;
  float    forwardTime;
  std::vector<Plenoxel> m_grid;
  size_t m_gridSize;
  BoundingBox m_sceneBoundingBox;
};

