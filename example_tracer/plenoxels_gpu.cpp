#include <cassert>
#include <iostream>

#include "vk_buffers.h"
#include "vk_pipeline.h"
#include "vk_utils.h"

#include "plenoxels_gpu.h"

PlenoxelsGPU::PlenoxelsGPU () {
#ifndef NDEBUG
  const bool enableValidationLayers = true;
#else // NDEBUG
  const bool enableValidationLayers = false;
#endif // NDEBUG
  this->m_context = vk_utils::globalContextGet (enableValidationLayers, 0);

  assert (vk_utils::globalContextIsInitialized ());

  std::vector <std::pair <VkDescriptorType, uint32_t>> dtypes = {
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}
  };

  this->m_dsMaker = std::make_shared <vk_utils::DescriptorMaker> (this->m_context.device, dtypes, 2);
}

PlenoxelsGPU::~PlenoxelsGPU () {
  vkDeviceWaitIdle (m_context.device);

  vk_utils::destroyPipelineIfExists(this->m_context.device, forward.handle, forward.layout);

  vkDestroyBuffer (m_context.device, this->m_gridBuffer, nullptr);
  vkFreeMemory (m_context.device, this->m_gridBufferMemory, nullptr);

  this->m_dsMaker = nullptr;
  this->m_context.pCopyHelper = nullptr;

  vk_utils::globalContextDestroy ();
}

void PlenoxelsGPU::InitGrid (const float a_gridSize) {
  Plenoxels::InitGrid (a_gridSize);
  this->m_pc.gridSize = a_gridSize;

  std::cout << "[InitGrid]: creating plenoxels buffer..." << std::endl;
  auto size = this->m_grid.size () * sizeof (Plenoxel);
  auto usg = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  this->m_gridBuffer = vk_utils::createBuffer (m_context.device, size, usg);
  this->m_gridBufferMemory = vk_utils::allocateAndBindWithPadding(m_context.device, m_context.physicalDevice, {this->m_gridBuffer});
  std::cout << "[InitGrid]: creating plenoxels buffer...done" << std::endl;
}

void PlenoxelsGPU::LoadGrid (std::string fileName) {
  Plenoxels::LoadGrid (fileName);

  std::cout << "[LoadGrid]: copying data to plenoxels buffer..." << std::endl;
  auto size = this->m_grid.size () * sizeof (Plenoxel);
  m_context.pCopyHelper->UpdateBuffer(this->m_gridBuffer, 0, this->m_grid.data (), size);
  std::cout << "[LoadGrid]: copying data to plenoxels buffer...done" << std::endl;
}

void PlenoxelsGPU::SetBoundingBox (const float3 boxMin, const float3 boxMax) {
  Plenoxels::SetBoundingBox (boxMin, boxMax);
  this->m_pc.boxMin = to_float4 (boxMin, 0.0f); // align
  this->m_pc.boxMax = to_float4 (boxMax, 0.0f); // align
}

void PlenoxelsGPU::SetWorldViewProjMatrix (const float4x4& a_mat) {
  Plenoxels::SetWorldViewProjMatrix (a_mat);
  this->m_pc.worldProjInv = this->m_worldViewProjInv;
}

void PlenoxelsGPU::SetWorldViewMatrix (const float4x4& a_mat) {
  Plenoxels::SetWorldViewMatrix (a_mat);
  this->m_pc.worldViewInv = this->m_worldViewInv;
}

std::vector<uint32_t> PlenoxelsGPU::kernel2D_Forward (uint32_t width, uint32_t height) {
  std::vector<uint32_t> out_color (width * height);

  this->m_pc.width = width;
  this->m_pc.height = height;

  auto size = width * height * sizeof (uint32_t);
  auto usg = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  this->m_colorBuffer = vk_utils::createBuffer (m_context.device, size, usg);
  this->m_colorBufferMemory = vk_utils::allocateAndBindWithPadding(m_context.device, m_context.physicalDevice, {this->m_colorBuffer});

  CreatePipeline ();
  vkDeviceWaitIdle (m_context.device);

  auto cmdBuff = vk_utils::createCommandBuffer (this->m_context.device, this->m_context.commandPool);
  VkCommandBufferBeginInfo beginInfo {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  if (vkBeginCommandBuffer (cmdBuff, &beginInfo) == VK_SUCCESS) {
    vkCmdBindPipeline (cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, forward.handle);
    vkCmdBindDescriptorSets (cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, forward.layout, 0, 1, &forward.descriptors, 0, VK_NULL_HANDLE);
    vkCmdPushConstants (cmdBuff, forward.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (PushConstants), &this->m_pc);
    vkCmdDispatch (cmdBuff, (width + 15) / 16, (height + 15) / 16, 1);
  }

  if (vkEndCommandBuffer (cmdBuff) == VK_SUCCESS) {
    vk_utils::executeCommandBufferNow (cmdBuff, this->m_context.computeQueue, this->m_context.device);
  } else {
    throw std::runtime_error ("failed to record command buffer!");
  }

  vkDeviceWaitIdle (m_context.device);
  m_context.pCopyHelper->ReadBuffer (this->m_colorBuffer, 0, out_color.data (), size);
  vkDeviceWaitIdle (m_context.device);
  vkFreeMemory (m_context.device, this->m_colorBufferMemory, nullptr);
  vkDestroyBuffer (m_context.device, this->m_colorBuffer, nullptr);

  return out_color;
}

void PlenoxelsGPU::CreatePipeline () {
  vk_utils::ComputePipelineMaker maker;
  maker.LoadShader (this->m_context.device, "shaders/plenoxel.spv");

  this->m_dsMaker->BindBegin (VK_SHADER_STAGE_COMPUTE_BIT);
  {
    this->m_dsMaker->BindBuffer (0, this->m_gridBuffer);
    this->m_dsMaker->BindBuffer (1, this->m_colorBuffer);
  }
  this->m_dsMaker->BindEnd (&forward.descriptors, &forward.descriptorsLayout);

  forward.layout = maker.MakeLayout (this->m_context.device, {forward.descriptorsLayout}, sizeof (PushConstants));
  forward.handle = maker.MakePipeline (this->m_context.device);
}

