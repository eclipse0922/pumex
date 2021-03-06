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
#include <map>
#include <algorithm>
#include <mutex>
#include <pumex/Export.h>
#include <pumex/Asset.h>
#include <pumex/BoundingBox.h>
#include <pumex/StorageBuffer.h>
#include <pumex/StorageBufferPerSurface.h>
#include <pumex/GenericBuffer.h>
#include <pumex/Pipeline.h>

namespace pumex
{

class Device;
class DeviceMemoryAllocator;
class CommandPool;
class CommandBuffer;

// AssetBuffer is a class that holds all assets in a single place in GPU memory.
// Each asset may have different set of render aspects ( normal rendering with tangents, transluency, lights, etc ) defined by render mask.
// Each render aspect may use different shaders with different vertex semantic in its geometries.
// For each render mask used by AssetBuffer you must register its render semantic using registerVertexSemantic() method.
// 
// Asset's render masks are defined per single geometry. It's in user's responsibility to mark each geometry
// by its specific render mask ( using geometry name, associated materials, textures and whatever the user finds appropriate ).
// 
// To register single object you must define an object type by calling registerType() method
// Then for that type you register Assets as different LODs. Each asset has skeletons, animations, geometries, materials, textures etc.
// Materials and textures are treated in different class called MaterialSet.
// Animations are stored and used by CPU
//
// To bind AssetBuffer resources to vulkan you may use cmdBindVertexIndexBuffer().
// Each render aspect ( identified by render mask ) has its own vertex and index buffers, so the user is able to use different shaders to 
// draw to different subpasses.
//
// After vertex/index binding the user is ready to draw objects.
// To draw a single object it is enough to use cmdDrawObject() method, but AssetBuffer was created with
// MASSIVE INSTANCED RENDERING in mind - check crowd and pumexgpucull examples on how to achieve this.
//
// Every object type in AssetBuffer : 
//  - is recognized by its ID number.
//  - has predefined bounding box ( user is responsible for handing this information over to AssetBuffer )
//  - may have one or more levels of detail ( LODs )
//
// Every LOD in AssetBuffer :
//  - has minimum visible distance and maximum visible distance defined
//  - has a list of geometries used by that LOD
//
// Every geometry in AssetBuffer :
//  - has render mask
//  - has pointers to vertex and index buffers ( in form of offset/size numbers )

struct PUMEX_EXPORT AssetBufferVertexSemantics
{
  AssetBufferVertexSemantics(uint32_t rm, const std::vector<VertexSemantic>& vs)
    : renderMask{ rm }, vertexSemantic( vs )
  {
  }
  uint32_t                    renderMask;
  std::vector<VertexSemantic> vertexSemantic;
};

struct PUMEX_EXPORT AssetTypeDefinition
{
  AssetTypeDefinition() = default;
  AssetTypeDefinition(const BoundingBox& bb)
    : bbMin{ bb.bbMin.x, bb.bbMin.y, bb.bbMin.z, 1.0f }, bbMax{ bb.bbMax.x, bb.bbMax.y, bb.bbMax.z, 1.0f }
  {
  }
  glm::vec4   bbMin;           // we use vec4 for bounding box storing because of std430
  glm::vec4   bbMax;
  uint32_t    lodFirst   = 0;  // used internally
  uint32_t    lodSize    = 0;  // used internally
  uint32_t    std430pad0;
  uint32_t    std430pad1;
};

struct PUMEX_EXPORT AssetLodDefinition
{
  AssetLodDefinition() = default;
  AssetLodDefinition(float minval, float maxval)
    : minDistance{ glm::min(minval, maxval) }, maxDistance{ glm::max(minval, maxval) }
  {
  }
  inline bool active(float distance) const
  {
    return distance >= minDistance && distance < maxDistance;
  }
  uint32_t geomFirst   = 0; // used internally
  uint32_t geomSize    = 0; // used internally
  float    minDistance = 0.0f;
  float    maxDistance = 0.0f;
};

struct PUMEX_EXPORT AssetGeometryDefinition
{
  AssetGeometryDefinition() = default;
  AssetGeometryDefinition(uint32_t ic, uint32_t fi, uint32_t vo)
    : indexCount{ ic }, firstIndex{ fi }, vertexOffset{vo}
  {
  }
  uint32_t indexCount   = 0;
  uint32_t firstIndex   = 0;
  uint32_t vertexOffset = 0;
};

struct PUMEX_EXPORT DrawIndexedIndirectCommand
{
  DrawIndexedIndirectCommand() = default;
  DrawIndexedIndirectCommand(uint32_t ic, uint32_t inc, uint32_t fi, uint32_t vo, uint32_t fin)
    : indexCount{ic}, instanceCount{inc}, firstIndex{fi}, vertexOffset{vo}, firstInstance{fin}
  {
  }

  uint32_t indexCount    = 0;
  uint32_t instanceCount = 0;
  uint32_t firstIndex    = 0;
  uint32_t vertexOffset  = 0;
  uint32_t firstInstance = 0;
};

class PUMEX_EXPORT AssetBuffer
{
public:
  AssetBuffer()                              = delete;
  explicit AssetBuffer(const std::vector<AssetBufferVertexSemantics>& vertexSemantics, std::weak_ptr<DeviceMemoryAllocator> bufferAllocator, std::weak_ptr<DeviceMemoryAllocator> vertexIndexAllocator);
  AssetBuffer(const AssetBuffer&)            = delete;
  AssetBuffer& operator=(const AssetBuffer&) = delete;
  virtual ~AssetBuffer();

  uint32_t               registerType(const std::string& typeName, const AssetTypeDefinition& tdef);
  uint32_t               registerObjectLOD( uint32_t typeID, std::shared_ptr<Asset> asset, const AssetLodDefinition& ldef );
  uint32_t               getTypeID(const std::string& typeName) const;
  std::string            getTypeName( uint32_t typeID ) const;
  uint32_t               getLodID(uint32_t typeID, float distance) const;
  std::shared_ptr<Asset> getAsset(uint32_t typeID, uint32_t lodID);
  inline uint32_t        getNumTypesID() const;
  
  inline void            setDirty();
  void                   validate(Device* device, CommandPool* commandPool, VkQueue queue = VK_NULL_HANDLE);

  void                   cmdBindVertexIndexBuffer(Device* device, CommandBuffer* commandBuffer, uint32_t renderMask, uint32_t vertexBinding = 0) const;
  void                   cmdDrawObject(Device* device, CommandBuffer* commandBuffer, uint32_t renderMask, uint32_t typeID, uint32_t firstInstance, float distanceToViewer) const;

  inline uint32_t        getNumRenderMasks() const;

  void                   prepareDrawIndexedIndirectCommandBuffer(uint32_t renderMask, std::vector<DrawIndexedIndirectCommand>& resultBuffer, std::vector<uint32_t>& resultGeomToType) const;

  std::shared_ptr<StorageBuffer<AssetTypeDefinition>>     getTypeBuffer(uint32_t renderMask);
  std::shared_ptr<StorageBuffer<AssetLodDefinition>>      getLodBuffer(uint32_t renderMask);
  std::shared_ptr<StorageBuffer<AssetGeometryDefinition>> getGeomBuffer(uint32_t renderMask);

protected:
  struct PerRenderMaskData
  {
    PerRenderMaskData() = default;
    PerRenderMaskData(std::weak_ptr<DeviceMemoryAllocator> bufferAllocator, std::weak_ptr<DeviceMemoryAllocator> vertexIndexAllocator)
    {
      vertices     = std::make_shared<std::vector<float>>();
      indices      = std::make_shared<std::vector<uint32_t>>();
      vertexBuffer = std::make_shared<GenericBuffer<std::vector<float>>>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices, vertexIndexAllocator);
      indexBuffer  = std::make_shared<GenericBuffer<std::vector<uint32_t>>>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices, vertexIndexAllocator);

      typeBuffer   = std::make_shared<StorageBuffer<AssetTypeDefinition>>(bufferAllocator);
      lodBuffer    = std::make_shared<StorageBuffer<AssetLodDefinition>>(bufferAllocator);
      geomBuffer   = std::make_shared<StorageBuffer<AssetGeometryDefinition>>(bufferAllocator);

    }
    std::shared_ptr<std::vector<float>>                   vertices;
    std::shared_ptr<std::vector<uint32_t>>                indices;
    std::shared_ptr<GenericBuffer<std::vector<float>>>    vertexBuffer;
    std::shared_ptr<GenericBuffer<std::vector<uint32_t>>> indexBuffer;

    std::shared_ptr<StorageBuffer<AssetTypeDefinition>>     typeBuffer;
    std::shared_ptr<StorageBuffer<AssetLodDefinition>>      lodBuffer;
    std::shared_ptr<StorageBuffer<AssetGeometryDefinition>> geomBuffer;
  };


  struct InternalGeometryDefinition
  {
    InternalGeometryDefinition(uint32_t tid, uint32_t lid, uint32_t rm, uint32_t ai, uint32_t gi)
      : typeID{tid}, lodID{lid}, renderMask{rm}, assetIndex{ai}, geometryIndex{gi}
    {
    }

    uint32_t typeID;
    uint32_t lodID;
    uint32_t renderMask;
    uint32_t assetIndex;
    uint32_t geometryIndex;
  };

  struct AssetKey
  {
    AssetKey(uint32_t t, uint32_t l)
      : typeID{ t }, lodID{ l }
    {
    }
    uint32_t typeID;
    uint32_t lodID;
  };
  struct AssetKeyCompare
  {
    bool operator()(const AssetKey& lhs, const AssetKey& rhs) const
    {
      if (lhs.typeID != rhs.typeID)
        return lhs.typeID < rhs.typeID;
      return lhs.lodID < rhs.lodID;
    }
  };

  bool                                            dirty = true;
  mutable std::mutex                              mutex;
  std::map<uint32_t, std::vector<VertexSemantic>> semantics;
  std::unordered_map<uint32_t, PerRenderMaskData> perRenderMaskData;

  std::vector<std::string>                        typeNames;
  std::map<std::string, uint32_t>                 invTypeNames;
  std::vector<AssetTypeDefinition>                typeDefinitions;
  std::vector<std::vector<AssetLodDefinition>>    lodDefinitions;
  std::vector<InternalGeometryDefinition>         geometryDefinitions;

  std::vector<std::shared_ptr<Asset>>             assets; // asset buffer owns assets
  std::map<AssetKey, std::shared_ptr<Asset>, AssetKeyCompare> assetMapping;
};

uint32_t AssetBuffer::getNumTypesID() const     { return typeNames.size(); }
void     AssetBuffer::setDirty()                { dirty = true; }
uint32_t AssetBuffer::getNumRenderMasks() const { return perRenderMaskData.size(); }

// helper class with buffers storing results of compute shader computations
class PUMEX_EXPORT AssetBufferInstancedResults
{
public:
  AssetBufferInstancedResults()                                              = delete;
  explicit AssetBufferInstancedResults(const std::vector<AssetBufferVertexSemantics>& vertexSemantics, std::weak_ptr<AssetBuffer> assetBuffer, std::weak_ptr<DeviceMemoryAllocator> buffersAllocator);
  AssetBufferInstancedResults(const AssetBufferInstancedResults&)            = delete;
  AssetBufferInstancedResults& operator=(const AssetBufferInstancedResults&) = delete;
  virtual ~AssetBufferInstancedResults();

  void                                                                 setup();
  void                                                                 prepareBuffers(const std::vector<uint32_t>& typeCount);

  std::shared_ptr<StorageBufferPerSurface<DrawIndexedIndirectCommand>> getResults(uint32_t renderMask);
  std::shared_ptr<StorageBufferPerSurface<uint32_t>>                   getOffsetValues(uint32_t renderMask);
  uint32_t                                                             getDrawCount(uint32_t renderMask);


  void                                                                 setActiveIndex(uint32_t index);
  void                                                                 validate(Surface* surface);

protected:
  struct PerRenderMaskData
  {
    PerRenderMaskData() = default;
    PerRenderMaskData(std::weak_ptr<DeviceMemoryAllocator> allocator)
    {
      resultsSbo = std::make_shared<StorageBufferPerSurface<DrawIndexedIndirectCommand>>(allocator, 3, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
      offValuesSbo = std::make_shared<StorageBufferPerSurface<uint32_t>>(allocator, 3);
    }

    std::vector<DrawIndexedIndirectCommand>                              initialResultValues;
    std::vector<uint32_t>                                                resultsGeomToType;
    std::shared_ptr<StorageBufferPerSurface<DrawIndexedIndirectCommand>> resultsSbo;
    std::shared_ptr<StorageBufferPerSurface<uint32_t>>                   offValuesSbo;
  };

  mutable std::mutex                              mutex;
  std::map<uint32_t, std::vector<VertexSemantic>> semantics;
  std::unordered_map<uint32_t, PerRenderMaskData> perRenderMaskData;
  std::weak_ptr<AssetBuffer>                      assetBuffer;
};

}
