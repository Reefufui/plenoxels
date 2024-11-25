#pragma once

#include <cassert>
#include <cstdint>
#include <fstream>
#include <fstream>
#include <iostream>
#include <vector>

#include "LiteMath.h"
using namespace LiteMath;

const size_t SH_WIDTH = 9;
const size_t PLENOXEL_SIZE = 1 + SH_WIDTH * 3;
typedef std::array<float, PLENOXEL_SIZE> Plenoxel;

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
    this->m_grid.resize (m_gridSize * m_gridSize * m_gridSize * PLENOXEL_SIZE);

    for (size_t i = 0; i < m_gridSize * m_gridSize * m_gridSize; i++) {
      this->m_grid [i * PLENOXEL_SIZE] = 0.01;
      for (size_t j = 1; j < PLENOXEL_SIZE; j++) {
        this->m_grid [i * PLENOXEL_SIZE + j] = 0.1;
      }
    }
  }

  void ZeroGrad () {
    assert (this->m_gridSize);
    this->m_grid_d.resize (m_gridSize * m_gridSize * m_gridSize * PLENOXEL_SIZE);

    for (size_t i = 0; i < m_gridSize * m_gridSize * m_gridSize; i++) {
      this->m_grid_d [i * PLENOXEL_SIZE] = 0.0;
      for (size_t j = 1; j < PLENOXEL_SIZE; j++) {
        this->m_grid_d [i * PLENOXEL_SIZE + j] = 0.0;
      }
    }
  }

  virtual void LoadGrid (std::string fileName) {
    std::ifstream fin (fileName, std::ios::in | std::ios::binary);
    fin.read ((char*)&this->m_grid[0], this->m_grid.size () * PLENOXEL_SIZE);
    fin.close ();
  }

  virtual void SetWorldViewProjMatrix (const float4x4& a_mat) {
    m_worldViewProjInv = inverse4x4 (a_mat);
  }

  virtual void SetWorldViewMatrix (const float4x4& a_mat) {
    m_worldViewInv = inverse4x4 (a_mat);
  }

  virtual std::vector<uint32_t> kernel2D_Forward (uint32_t width, uint32_t height);
  std::vector<uint32_t> Forward (uint32_t width, uint32_t height);  
  std::vector<uint32_t> kernel2D_RayMarch (uint32_t width, uint32_t height);
  std::vector<uint32_t> RayMarch (uint32_t width, uint32_t height);

  void GetExecutionTime (const char* a_funcName, float a_out[4]);

protected:
  float4x4 m_worldViewProjInv;
  float4x4 m_worldViewInv;
  float m_forwardTime;
  std::vector<float> m_grid;
  std::vector<float> m_grid_d;
  size_t m_gridSize;
};

