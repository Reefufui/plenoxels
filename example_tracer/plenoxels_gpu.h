#pragma once

#include "vk_context.h"
#include "vk_descriptor_sets.h"

#include "plenoxels.h"

class PlenoxelsGPU : public Plenoxels {
  public:
    PlenoxelsGPU ();
    ~PlenoxelsGPU ();

    void InitGrid (const float a_gridSize) override;
    void LoadGrid (std::string fileName) override;
    void SetBoundingBox (const float3 boxMin, const float3 boxMax) override;
    void SetWorldViewProjMatrix (const float4x4& a_mat) override;
    void SetWorldViewMatrix (const float4x4& a_mat) override;

    std::vector<uint32_t> kernel2D_Forward (uint32_t width, uint32_t height) override;

  private:
    void CreatePipeline ();

    vk_utils::VulkanContext m_context;

    VkBuffer m_gridBuffer;
    VkDeviceMemory m_gridBufferMemory;

    VkBuffer m_colorBuffer;
    VkDeviceMemory m_colorBufferMemory;

    struct PushConstants {
      float4 boxMax;
      float4 boxMin;
      float4x4 worldProjInv;
      float4x4 worldViewInv;
      uint32_t gridSize;
      uint32_t height;
      uint32_t width;
    } m_pc;

    std::shared_ptr<vk_utils::DescriptorMaker> m_dsMaker;

    struct Pipeline
    {
      VkPipelineLayout      layout;
      VkPipeline            handle;
      VkDescriptorSet       descriptors;
      VkDescriptorSetLayout descriptorsLayout;
    };

    Pipeline forward;
};

