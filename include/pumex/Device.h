//
// Copyright(c) 2017 Pawe� Ksi�opolski ( pumexx )
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once
#include <vector>
#include <memory>
#include <tuple>
#include <mutex>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <pumex/Export.h>

namespace pumex
{

class Viewer;
class PhysicalDevice;
class CommandPool;
class CommandBuffer;
class StagingBuffer;

// struct that represents queues that must be provided by Vulkan implementation during initialization
struct PUMEX_EXPORT QueueTraits
{
  QueueTraits(VkQueueFlags mustHave, VkQueueFlags mustNotHave, const std::vector<float>& priority);

  VkQueueFlags       mustHave    = 0;
  VkQueueFlags       mustNotHave = 0;
  std::vector<float> priority;
};

// class representing Vulkan logical device
class PUMEX_EXPORT Device : public std::enable_shared_from_this<Device>
{
public:
  Device()                         = delete;
  explicit Device(std::shared_ptr<Viewer> viewer, std::shared_ptr<PhysicalDevice>  physical, const std::vector<QueueTraits>& requestedQueues, const std::vector<const char*>& requestedExtensions);
  Device(const Device&)            = delete;
  Device& operator=(const Device&) = delete;
  ~Device();

  inline bool isValid();
  void cleanup();

  std::shared_ptr<CommandBuffer> beginSingleTimeCommands(CommandPool* commandPool);
  void endSingleTimeCommands(std::shared_ptr<CommandBuffer> commandBuffer, VkQueue queue);

  VkQueue                        getQueue(const QueueTraits& queueTraits, bool reserve = false);
  void                           releaseQueue(VkQueue queue);
  bool                           getQueueIndices(VkQueue queue, std::tuple<uint32_t&, uint32_t&>& result);

  std::shared_ptr<StagingBuffer> acquireStagingBuffer( void* data, VkDeviceSize size );
  void                           releaseStagingBuffer(std::shared_ptr<StagingBuffer> buffer);
  
  inline void                    setID(uint32_t newID);
  inline uint32_t                getID() const;

  // debug markers extension stuff - not tested yet
  void                           setObjectName(uint64_t object, VkDebugReportObjectTypeEXT objectType, const std::string& name);
  void                           setObjectTag(uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag);
  void                           beginMarkerRegion(VkCommandBuffer cmdbuffer, const std::string& markerName, glm::vec4 color);
  void                           insertMarker(VkCommandBuffer cmdbuffer, const std::string& markerName, glm::vec4 color);
  void                           endMarkerRegion(VkCommandBuffer cmdBuffer);
  void                           setCommandBufferName(VkCommandBuffer cmdBuffer, const std::string& name);
  void                           setQueueName(VkQueue queue, const std::string& name);
  void                           setImageName(VkImage image, const std::string& name);
  void                           setSamplerName(VkSampler sampler, const std::string& name);
  void                           setBufferName(VkBuffer buffer, const std::string& name);
  void                           setDeviceMemoryName(VkDeviceMemory memory, const std::string& name);
  void                           setShaderModuleName(VkShaderModule shaderModule, const std::string& name);
  void                           setPipelineName(VkPipeline pipeline, const std::string& name);
  void                           setPipelineLayoutName(VkPipelineLayout pipelineLayout, const std::string& name);
  void                           setRenderPassName(VkRenderPass renderPass, const std::string& name);
  void                           setFramebufferName(VkFramebuffer framebuffer, const std::string& name);
  void                           setDescriptorSetLayoutName(VkDescriptorSetLayout descriptorSetLayout, const std::string& name);
  void                           setDescriptorSetName(VkDescriptorSet descriptorSet, const std::string& name);
  void                           setSemaphoreName(VkSemaphore semaphore, const std::string& name);
  void                           setFenceName(VkFence fence, const std::string& name);
  void                           setEventName(VkEvent _event, const std::string& name);

  std::weak_ptr<Viewer>          viewer;
  std::weak_ptr<PhysicalDevice>  physical;
  VkDevice                       device             = VK_NULL_HANDLE;
  bool                           enableDebugMarkers = false;
protected:
  struct Queue
  {
    Queue(const QueueTraits& queueTraits, uint32_t familyIndex, uint32_t index, VkQueue queue);
    bool isEqual(const QueueTraits& queueTraits);

    QueueTraits traits;
    uint32_t    familyIndex = UINT32_MAX;
    uint32_t    index       = UINT32_MAX;
    VkQueue     queue       = VK_NULL_HANDLE;
    bool        available   = true;
  };

  std::vector<Queue>                queues;
  uint32_t                          id                          = 0;

  PFN_vkDebugMarkerSetObjectTagEXT  pfnDebugMarkerSetObjectTag  = VK_NULL_HANDLE;
  PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
  PFN_vkCmdDebugMarkerBeginEXT      pfnCmdDebugMarkerBegin      = VK_NULL_HANDLE;
  PFN_vkCmdDebugMarkerEndEXT        pfnCmdDebugMarkerEnd        = VK_NULL_HANDLE;
  PFN_vkCmdDebugMarkerInsertEXT     pfnCmdDebugMarkerInsert     = VK_NULL_HANDLE;

  std::vector<std::shared_ptr<StagingBuffer>> stagingBuffers;
  std::mutex                                  stagingMutex;
};

bool     Device::isValid()             { return device != VK_NULL_HANDLE; }
void     Device::setID(uint32_t newID) { id = newID; }
uint32_t Device::getID() const         { return id; }

}
