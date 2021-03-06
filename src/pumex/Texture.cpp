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

#include <pumex/Texture.h>
#include <pumex/Device.h>
#include <pumex/Command.h>
#include <pumex/PhysicalDevice.h>
#include <pumex/utils/Buffer.h>
#include <pumex/utils/Log.h>

namespace pumex
{

ImageTraits::ImageTraits(VkImageUsageFlags u, VkFormat f, const VkExtent3D& e, bool lt, uint32_t m, uint32_t l, VkSampleCountFlagBits s, VkImageLayout il, 
  VkImageAspectFlags am, VkMemoryPropertyFlags mp, VkImageCreateFlags ic, VkImageType it, VkSharingMode sm, VkImageViewType vt, const gli::swizzles& sw)
  : usage{ u }, linearTiling{ lt }, format{ f }, extent( e ), mipLevels{ m }, arrayLayers{ l }, samples{ s }, initialLayout{ il }, imageCreate{ ic }, imageType{ it }, sharingMode{ sm }, viewType{ vt }, swizzles{ sw }, aspectMask{ am }, memoryProperty{ mp }
{
}

TextureTraits::TextureTraits(VkImageUsageFlags u, bool lt, VkFilter maf, VkFilter mif, VkSamplerMipmapMode mm,  VkSamplerAddressMode au, VkSamplerAddressMode av, VkSamplerAddressMode aw, float mlb, VkBool32 ae,
  float maa, VkBool32 ce, VkCompareOp co, VkBorderColor bc, VkBool32 uc)
  : usage{ u }, linearTiling{ lt }, magFilter{ maf }, minFilter{ mif }, mipmapMode{ mm }, addressModeU{ au }, addressModeV{ av }, addressModeW{ aw }, mipLodBias{ mlb }, anisotropyEnable{ ae }, maxAnisotropy{ maa }, compareEnable{ ce }, compareOp{ co }, borderColor{ bc }, unnormalizedCoordinates{ uc }
{
}

Image::Image(Device* d, const ImageTraits& it, std::weak_ptr<DeviceMemoryAllocator> a)
  : imageTraits{ it }, device(d->device), allocator{ a }, ownsImage{ true }
{
  VkImageCreateInfo imageCI{};
    imageCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.flags         = imageTraits.imageCreate;
    imageCI.imageType     = imageTraits.imageType;
    imageCI.format        = imageTraits.format;
    imageCI.extent        = imageTraits.extent;
    imageCI.mipLevels     = imageTraits.mipLevels;
    imageCI.arrayLayers   = imageTraits.arrayLayers;
    imageCI.samples       = imageTraits.samples;
    imageCI.tiling        = imageTraits.linearTiling ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage         = imageTraits.usage;
    imageCI.sharingMode   = imageTraits.sharingMode;
//    imageCI.queueFamilyIndexCount;
//    imageCI.pQueueFamilyIndices;
    imageCI.initialLayout = imageTraits.initialLayout;
  VK_CHECK_LOG_THROW(vkCreateImage(device, &imageCI, nullptr, &image), "failed vkCreateImage");

  imageLayout = imageTraits.initialLayout;
  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(device, image, &memReqs);

  std::shared_ptr<DeviceMemoryAllocator> alloc = allocator.lock();
  memoryBlock = alloc->allocate(d, memReqs);
  CHECK_LOG_THROW(memoryBlock.alignedSize == 0, "Cannot allocate memory for Image");
  VK_CHECK_LOG_THROW(vkBindImageMemory(device, image, memoryBlock.memory, memoryBlock.alignedOffset), "failed vkBindImageMemory");
  
  //VkMemoryAllocateInfo mem_alloc{};
  //  mem_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  //  mem_alloc.pNext           = nullptr;
  //  mem_alloc.allocationSize  = memReqs.size;
  //  mem_alloc.memoryTypeIndex = d->physical.lock()->getMemoryType(memReqs.memoryTypeBits, imageTraits.memoryProperty);
  //VK_CHECK_LOG_THROW(vkAllocateMemory(device, &mem_alloc, nullptr, &deviceMemory), "failed vkAllocateMemory " << mem_alloc.allocationSize << " " << mem_alloc.memoryTypeIndex);
  //VK_CHECK_LOG_THROW(vkBindImageMemory(device, image, deviceMemory, 0), "failed vkBindImageMemory");

  VkImageViewCreateInfo imageViewCI{};
    imageViewCI.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.flags      = 0;
    imageViewCI.image      = image;
    imageViewCI.viewType   = imageTraits.viewType;
    imageViewCI.format     = imageTraits.format;
    imageViewCI.components = vulkanComponentMappingFromGliComponentMapping(imageTraits.swizzles);
    imageViewCI.subresourceRange.aspectMask     = imageTraits.aspectMask;
    imageViewCI.subresourceRange.baseMipLevel   = 0;
    imageViewCI.subresourceRange.levelCount     = imageTraits.mipLevels;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount     = imageTraits.arrayLayers;
  VK_CHECK_LOG_THROW(vkCreateImageView(device, &imageViewCI, nullptr, &imageView), "failed vkCreateImageView");
}

Image::Image(Device* d, VkImage i, VkFormat format, uint32_t mipLevels, uint32_t arrayLayers, VkImageAspectFlags aspectMask, VkImageViewType viewType, const gli::swizzles& swizzles)
  : device(d->device), image{ i }, ownsImage {  false }
{
  // gather all what we know about delivered image
  imageTraits.format      = format;
  imageTraits.mipLevels   = mipLevels;
  imageTraits.arrayLayers = arrayLayers;
  imageTraits.aspectMask  = aspectMask;
  imageTraits.viewType    = viewType;
  imageTraits.swizzles    = swizzles;
  imageLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
//  vkGetImageMemoryRequirements(device, image, &memReqs);

  VkImageViewCreateInfo imageViewCI{};
    imageViewCI.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.flags      = 0;
    imageViewCI.image      = image;
    imageViewCI.viewType   = viewType;
    imageViewCI.format     = format;
    imageViewCI.components = vulkanComponentMappingFromGliComponentMapping(swizzles);
    imageViewCI.subresourceRange.aspectMask     = aspectMask;
    imageViewCI.subresourceRange.baseMipLevel   = 0;
    imageViewCI.subresourceRange.levelCount     = mipLevels;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount     = arrayLayers;
  VK_CHECK_LOG_THROW(vkCreateImageView(device, &imageViewCI, nullptr, &imageView), "failed vkCreateImageView");
}


Image::~Image()
{
  if (imageView != VK_NULL_HANDLE)
    vkDestroyImageView(device, imageView, nullptr);
  if (ownsImage)
  {
    if (image != VK_NULL_HANDLE)
      vkDestroyImage(device, image, nullptr);
    if(!allocator.expired())
      allocator.lock()->deallocate(device, memoryBlock);
//    if (deviceMemory != VK_NULL_HANDLE)
//      vkFreeMemory(device, deviceMemory, nullptr);
  }
}

void Image::getImageSubresourceLayout(VkImageSubresource& subRes, VkSubresourceLayout& subResLayout) const
{
  vkGetImageSubresourceLayout(device, image, &subRes, &subResLayout);
}

void* Image::mapMemory(size_t offset, size_t range, VkMemoryMapFlags flags)
{
  void* data;
  VK_CHECK_LOG_THROW(vkMapMemory(device, memoryBlock.memory, memoryBlock.alignedSize + offset, range, flags, &data), "Cannot map memory to image");
  return data;
}

void Image::unmapMemory()
{
  vkUnmapMemory(device, memoryBlock.memory);
}

void Image::setImageLayout(VkImageLayout newLayout)
{
  imageLayout = newLayout;
}

Texture::Texture(const gli::texture& tex, const TextureTraits& tr, std::weak_ptr<DeviceMemoryAllocator> a)
  : traits{ tr }, allocator{ a }
{
  texture = std::make_shared<gli::texture>(tex);
}

Texture::~Texture()
{
  for (auto& pdd : perDeviceData)
  {
    if (pdd.second.sampler != VK_NULL_HANDLE)
      vkDestroySampler(pdd.first, pdd.second.sampler, nullptr);
  }
}

void Texture::setDirty()
{
  std::lock_guard<std::mutex> lock(mutex);
  for (auto& pdd : perDeviceData)
    pdd.second.dirty = true;
}

Image* Texture::getHandleImage(VkDevice device) const
{
  std::lock_guard<std::mutex> lock(mutex);
  auto pddit = perDeviceData.find(device);
  if (pddit == perDeviceData.end())
    return nullptr;
  return pddit->second.image.get();
}

VkSampler Texture::getHandleSampler(VkDevice device) const
{
  std::lock_guard<std::mutex> lock(mutex);
  auto pddit = perDeviceData.find(device);
  if (pddit == perDeviceData.end())
    return VK_NULL_HANDLE;
  return pddit->second.sampler;
}

void Texture::validate(Device* device, CommandPool* commandPool, VkQueue queue)
{
  std::lock_guard<std::mutex> lock(mutex);
  auto pddit = perDeviceData.find(device->device);
  if (pddit == perDeviceData.end())
    pddit = perDeviceData.insert({ device->device, PerDeviceData() }).first;
  if (!pddit->second.dirty)
    return;

  // Create sampler
  if( pddit->second.sampler == VK_NULL_HANDLE )
  {
    VkSamplerCreateInfo sampler{};
      sampler.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler.magFilter               = traits.magFilter;
      sampler.minFilter               = traits.minFilter;
      sampler.mipmapMode              = traits.mipmapMode;
      sampler.addressModeU            = traits.addressModeU;
      sampler.addressModeV            = traits.addressModeV;
      sampler.addressModeW            = traits.addressModeW;
      sampler.mipLodBias              = traits.mipLodBias;
      sampler.anisotropyEnable        = traits.anisotropyEnable;
      sampler.maxAnisotropy           = traits.maxAnisotropy;
      sampler.compareEnable           = traits.compareEnable;
      sampler.compareOp               = traits.compareOp;
      sampler.minLod                  = 0.0f;
      sampler.maxLod                  = (!traits.linearTiling) ? (float)texture->levels() : 0.0f;
      sampler.borderColor             = traits.borderColor;
      sampler.unnormalizedCoordinates = traits.unnormalizedCoordinates;
    VK_CHECK_LOG_THROW( vkCreateSampler(device->device, &sampler, nullptr, &pddit->second.sampler) , "Cannot create sampler");
  }

  VkFormat format = vulkanFormatFromGliFormat(texture->format());

  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(device->physical.lock()->physicalDevice, format, &formatProperties);

  auto cmdBuffer = device->beginSingleTimeCommands(commandPool);
  bool memoryIsLocal = ((allocator.lock()->getMemoryPropertyFlags() & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  CHECK_LOG_THROW(memoryIsLocal && traits.linearTiling, "Cannot have texture with linear tiling in device local memory");
  if (memoryIsLocal)
  {
    if (pddit->second.image.get() == nullptr)
    {
      auto textureExtents = texture->extent(0);
      VkImageUsageFlags usage = traits.usage;
      if (!(usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      ImageTraits imageTraits(usage, format, { uint32_t(textureExtents.x), uint32_t(textureExtents.y), 1 }, false, texture->levels(), texture->layers(),
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_ASPECT_COLOR_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, VK_IMAGE_TYPE_2D, VK_SHARING_MODE_EXCLUSIVE,
        vulkanViewTypeFromGliTarget(texture->target()), texture->swizzles());
      pddit->second.image = std::make_shared<Image>(device, imageTraits, allocator);
    }

    std::vector<VkBufferImageCopy> bufferCopyRegions;
    size_t offset = 0;
    for (uint32_t layer = texture->base_layer(); layer < texture->layers(); ++layer)
    {
      for (uint32_t level = texture->base_level(); level < texture->levels(); ++level)
      {
        auto mipMapExtents = texture->extent(level);
        VkBufferImageCopy bufferCopyRegion{};
          bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
          bufferCopyRegion.imageSubresource.mipLevel       = level;
          bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
          bufferCopyRegion.imageSubresource.layerCount     = 1;
          bufferCopyRegion.imageExtent.width               = static_cast<uint32_t>(mipMapExtents.x);
          bufferCopyRegion.imageExtent.height              = static_cast<uint32_t>(mipMapExtents.y);
          bufferCopyRegion.imageExtent.depth               = 1;
          bufferCopyRegion.bufferOffset                    = offset;
        bufferCopyRegions.push_back(bufferCopyRegion);

        // Increase offset into staging buffer for next level / face
        offset += texture->size(level);
      }
    }

    std::shared_ptr<StagingBuffer> stagingBuffer = device->acquireStagingBuffer(texture->data(), texture->size());
    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    cmdBuffer->setImageLayout( *(pddit->second.image.get()), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy mip levels from staging buffer
    cmdBuffer->cmdCopyBufferToImage(stagingBuffer->buffer, (*pddit->second.image.get()), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, bufferCopyRegions);
    device->releaseStagingBuffer(stagingBuffer);

    // Change texture image layout to shader read after all mip levels have been copied
    cmdBuffer->setImageLayout( *(pddit->second.image.get()), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
  else
  {
//    assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    if (pddit->second.image.get() == nullptr)
    {
      auto textureExtents = texture->extent(0);
      ImageTraits imageTraits(traits.usage, format, { uint32_t(textureExtents.x), uint32_t(textureExtents.y), 1 }, true, 1, texture->layers(),
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_ASPECT_COLOR_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, VK_IMAGE_TYPE_2D, VK_SHARING_MODE_EXCLUSIVE,
        vulkanViewTypeFromGliTarget(texture->target()), texture->swizzles());
      pddit->second.image = std::make_shared<Image>(device, imageTraits, allocator);
    }

    VkImageSubresource subRes{};
      subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      subRes.mipLevel   = 0;

    VkSubresourceLayout subResLayout;
    pddit->second.image->getImageSubresourceLayout(subRes, subResLayout);
    void* data = pddit->second.image->mapMemory(0, pddit->second.image->getMemorySize(), 0);
    memcpy(data, texture->data(0, 0, subRes.mipLevel), texture->size(subRes.mipLevel));
    pddit->second.image->unmapMemory();

    // Setup image memory barrier
    cmdBuffer->setImageLayout(*(pddit->second.image.get()), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
  device->endSingleTimeCommands(cmdBuffer, queue);

  pddit->second.dirty = false;
}

void Texture::getDescriptorSetValues(VkDevice device, uint32_t index, std::vector<DescriptorSetValue>& values) const
{
  std::lock_guard<std::mutex> lock(mutex);
  auto pddit = perDeviceData.find(device);
  CHECK_LOG_THROW(pddit == perDeviceData.end(), "Texture::getDescriptorSetValue : texture was not validated");

  values.push_back( DescriptorSetValue(pddit->second.sampler, pddit->second.image->getImageView(), pddit->second.image->getImageLayout()) );
}

void Texture::setLayer(uint32_t layer, const gli::texture& tex)
{
  CHECK_LOG_THROW((layer < texture->base_layer()) || (layer >= texture->base_layer() + texture->layers()), "Layer out of bounds : " << layer << " should be between " << texture->base_layer() << " and " << texture->base_layer() + texture->layers() -1);
  CHECK_LOG_THROW(texture->format() != tex.format(), "Input texture has wrong format : " << tex.format() << " should be " << texture->format());
  CHECK_LOG_THROW((texture->extent().x != tex.extent().x) || (texture->extent().y != tex.extent().y), "Texture has wrong size : ( " << tex.extent().x << " x " << tex.extent().y << " ) should be ( " << texture->extent().x << " x " << texture->extent().y << " )");
  // FIXME - later we may add size and format conversions if needed

  for (uint32_t level = texture->base_level(); level < texture->levels(); ++level)
    memcpy(texture->data(layer, 0, level), tex.data(0, 0, level), tex.size(level));

}

VkFormat vulkanFormatFromGliFormat(gli::texture::format_type format)
{
  // Formats are almost identical. Looks like someone implemented GLI and Vulkan at the same time
  return (VkFormat)format;
}

VkImageViewType vulkanViewTypeFromGliTarget(gli::texture::target_type target)
{

  switch (target)
  {
  case gli::texture::target_type::TARGET_1D:
    return VK_IMAGE_VIEW_TYPE_1D;
  case gli::texture::target_type::TARGET_1D_ARRAY:
    return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
  case gli::texture::target_type::TARGET_2D:
    return VK_IMAGE_VIEW_TYPE_2D;
  case gli::texture::target_type::TARGET_2D_ARRAY:
    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  case gli::texture::target_type::TARGET_3D:
    return VK_IMAGE_VIEW_TYPE_3D;
  case gli::texture::target_type::TARGET_RECT:
    return VK_IMAGE_VIEW_TYPE_2D;
  case gli::texture::target_type::TARGET_RECT_ARRAY:
    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  case gli::texture::target_type::TARGET_CUBE:
    return VK_IMAGE_VIEW_TYPE_CUBE;
  case gli::texture::target_type::TARGET_CUBE_ARRAY:
    return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
  }
  return VK_IMAGE_VIEW_TYPE_2D;
}

VkComponentSwizzle vulkanSwizzlesFromGliSwizzles(const gli::swizzle& s)
{
  // VK_COMPONENT_SWIZZLE_IDENTITY is not represented in GLI
  switch (s)
  {
  case gli::swizzle::SWIZZLE_RED:
    return VK_COMPONENT_SWIZZLE_R;
  case gli::swizzle::SWIZZLE_GREEN:
    return VK_COMPONENT_SWIZZLE_G;
  case gli::swizzle::SWIZZLE_BLUE:
    return VK_COMPONENT_SWIZZLE_B;
  case gli::swizzle::SWIZZLE_ALPHA:
    return VK_COMPONENT_SWIZZLE_A;
  case gli::swizzle::SWIZZLE_ZERO:
    return VK_COMPONENT_SWIZZLE_ZERO;
  case gli::swizzle::SWIZZLE_ONE:
    return VK_COMPONENT_SWIZZLE_ONE;
  }
  return VK_COMPONENT_SWIZZLE_IDENTITY;
}

VkComponentMapping vulkanComponentMappingFromGliComponentMapping(const gli::swizzles& swz)
{
  VkComponentMapping mapping;
  mapping.r = vulkanSwizzlesFromGliSwizzles(swz.r);
  mapping.g = vulkanSwizzlesFromGliSwizzles(swz.g);
  mapping.b = vulkanSwizzlesFromGliSwizzles(swz.b);
  mapping.a = vulkanSwizzlesFromGliSwizzles(swz.a);
  return mapping;
}


}