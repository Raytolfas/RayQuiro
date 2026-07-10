#pragma once

#ifdef _WIN32

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define DrawText Win32DrawText
#define DrawTextEx Win32DrawTextEx
#define LoadImage Win32LoadImage
#define PlaySound Win32PlaySound
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound

namespace vulkan_backend {
using VkFlags = std::uint32_t;
using VkBool32 = std::uint32_t;
using VkResult = std::int32_t;
using VkStructureType = std::uint32_t;
using VkInstance = struct VkInstance_T*;
using VkPhysicalDevice = struct VkPhysicalDevice_T*;
using VkDevice = struct VkDevice_T*;
using VkSurfaceKHR = struct VkSurfaceKHR_T*;
using VkQueue = struct VkQueue_T*;
using VkSwapchainKHR = struct VkSwapchainKHR_T*;
using VkImage = struct VkImage_T*;
using VkImageView = struct VkImageView_T*;
using VkBuffer = struct VkBuffer_T*;
using VkDeviceMemory = struct VkDeviceMemory_T*;
using VkShaderModule = struct VkShaderModule_T*;
using VkPipelineLayout = struct VkPipelineLayout_T*;
using VkPipeline = struct VkPipeline_T*;
using VkDescriptorSetLayout = struct VkDescriptorSetLayout_T*;
using VkDescriptorPool = struct VkDescriptorPool_T*;
using VkDescriptorSet = struct VkDescriptorSet_T*;
using VkSampler = struct VkSampler_T*;
using VkRenderPass = struct VkRenderPass_T*;
using VkFramebuffer = struct VkFramebuffer_T*;
using VkCommandPool = struct VkCommandPool_T*;
using VkCommandBuffer = struct VkCommandBuffer_T*;
using VkSemaphore = struct VkSemaphore_T*;
using VkFence = struct VkFence_T*;
using VkFormat = std::uint32_t;
using VkColorSpaceKHR = std::uint32_t;
using VkPresentModeKHR = std::uint32_t;
using VkImageViewType = std::uint32_t;
using VkComponentSwizzle = std::uint32_t;
using VkSharingMode = std::uint32_t;
using VkCommandBufferLevel = std::uint32_t;
using VkImageType = std::uint32_t;
using VkImageTiling = std::uint32_t;
using VkBufferUsageFlags = VkFlags;
using VkMemoryPropertyFlags = VkFlags;
using VkShaderStageFlags = VkFlags;
using VkPipelineStageFlags = VkFlags;

constexpr VkResult VK_SUCCESS = 0;
constexpr VkResult VK_SUBOPTIMAL_KHR = 1000001003;
constexpr VkResult VK_ERROR_OUT_OF_DATE_KHR = -1000001004;
constexpr VkStructureType VK_STRUCTURE_TYPE_APPLICATION_INFO = 0;
constexpr VkStructureType VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1;
constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2;
constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3;
constexpr VkStructureType VK_STRUCTURE_TYPE_SUBMIT_INFO = 4;
constexpr VkStructureType VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5;
constexpr VkStructureType VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8;
constexpr VkStructureType VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO = 9;
constexpr VkStructureType VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12;
constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14;
constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15;
constexpr VkStructureType VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO = 16;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO = 18;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO = 19;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO = 20;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO = 22;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO = 23;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO = 24;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO = 25;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO = 26;
constexpr VkStructureType VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO = 37;
constexpr VkStructureType VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO = 38;
constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39;
constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;
constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42;
constexpr VkStructureType VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO = 43;
constexpr VkStructureType VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 45;
constexpr VkStructureType VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO = 30;
constexpr VkStructureType VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO = 31;
constexpr VkStructureType VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32;
constexpr VkStructureType VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 33;
constexpr VkStructureType VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 34;
constexpr VkStructureType VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35;
constexpr VkStructureType VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO = 28;
constexpr VkStructureType VK_STRUCTURE_TYPE_PRESENT_INFO_KHR = 1000001001;
constexpr VkStructureType VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR = 1000009000;
constexpr VkStructureType VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR = 1000001000;
constexpr std::uint32_t VK_API_VERSION_1_0 = (1u << 22);
constexpr VkFlags VK_QUEUE_GRAPHICS_BIT = 0x00000001u;
constexpr VkFlags VK_CULL_MODE_NONE = 0x00000000u;
constexpr VkFlags VK_CULL_MODE_FRONT_BIT = 0x00000001u;
constexpr VkFlags VK_CULL_MODE_BACK_BIT = 0x00000002u;
constexpr VkFlags VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002u;
constexpr VkFlags VK_IMAGE_USAGE_SAMPLED_BIT = 0x00000004u;
constexpr VkFlags VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010u;
constexpr VkFlags VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000020u;
constexpr VkFlags VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001u;
constexpr VkFlags VK_IMAGE_ASPECT_DEPTH_BIT = 0x00000002u;
constexpr VkFlags VK_ACCESS_SHADER_READ_BIT = 0x00000020u;
constexpr VkFlags VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = 0x00000100u;
constexpr VkFlags VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000u;
constexpr VkFlags VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x00000001u;
constexpr VkFlags VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000040u;
constexpr VkFlags VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000080u;
constexpr VkFlags VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001u;
constexpr VkFlags VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002u;
constexpr VkFlags VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004u;
constexpr VkFlags VK_SHADER_STAGE_VERTEX_BIT = 0x00000001u;
constexpr VkFlags VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010u;
constexpr VkFlags VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001u;
constexpr VkFlags VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080u;
constexpr VkFlags VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400u;
constexpr VkFlags VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000u;
constexpr VkFlags VK_COLOR_COMPONENT_R_BIT = 0x00000001u;
constexpr VkFlags VK_COLOR_COMPONENT_G_BIT = 0x00000002u;
constexpr VkFlags VK_COLOR_COMPONENT_B_BIT = 0x00000004u;
constexpr VkFlags VK_COLOR_COMPONENT_A_BIT = 0x00000008u;
constexpr VkFlags VK_FENCE_CREATE_SIGNALED_BIT = 0x00000001u;
constexpr VkFlags VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001u;
constexpr VkFlags VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002u;
constexpr VkFlags VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR = 0x00000001u;
constexpr VkFlags VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR = 0x00000001u;
constexpr VkPresentModeKHR VK_PRESENT_MODE_FIFO_KHR = 2;
constexpr VkPresentModeKHR VK_PRESENT_MODE_MAILBOX_KHR = 1;
constexpr VkSharingMode VK_SHARING_MODE_EXCLUSIVE = 0;
constexpr VkImageViewType VK_IMAGE_VIEW_TYPE_2D = 1;
constexpr VkImageType VK_IMAGE_TYPE_2D = 1;
constexpr VkImageTiling VK_IMAGE_TILING_OPTIMAL = 0;
constexpr VkComponentSwizzle VK_COMPONENT_SWIZZLE_IDENTITY = 0;
constexpr VkCommandBufferLevel VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0;
constexpr std::uint32_t VK_VERTEX_INPUT_RATE_VERTEX = 0;
constexpr std::uint32_t VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3;
constexpr std::uint32_t VK_POLYGON_MODE_FILL = 0;
constexpr std::uint32_t VK_FRONT_FACE_CLOCKWISE = 0;
constexpr std::uint32_t VK_FRONT_FACE_COUNTER_CLOCKWISE = 1;
constexpr std::uint32_t VK_COMPARE_OP_LESS_OR_EQUAL = 3;
constexpr std::uint32_t VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1;
constexpr std::uint32_t VK_FILTER_NEAREST = 0;
constexpr std::uint32_t VK_FILTER_LINEAR = 1;
constexpr std::uint32_t VK_SAMPLER_ADDRESS_MODE_REPEAT = 0;
constexpr std::uint32_t VK_INDEX_TYPE_UINT16 = 0;
constexpr std::uint32_t VK_ATTACHMENT_LOAD_OP_CLEAR = 1;
constexpr std::uint32_t VK_ATTACHMENT_LOAD_OP_DONT_CARE = 2;
constexpr std::uint32_t VK_ATTACHMENT_STORE_OP_STORE = 0;
constexpr std::uint32_t VK_ATTACHMENT_STORE_OP_DONT_CARE = 1;
constexpr std::uint32_t VK_IMAGE_LAYOUT_UNDEFINED = 0;
constexpr std::uint32_t VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5;
constexpr std::uint32_t VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7;
constexpr std::uint32_t VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2;
constexpr std::uint32_t VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3;
constexpr std::uint32_t VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002;
constexpr std::uint32_t VK_PIPELINE_BIND_POINT_GRAPHICS = 0;
constexpr std::uint32_t VK_SAMPLE_COUNT_1_BIT = 1;
constexpr std::uint32_t VK_SUBPASS_CONTENTS_INLINE = 0;
constexpr std::uint32_t VK_SUBPASS_EXTERNAL = 0xffffffffu;
constexpr std::uint32_t VK_INVALID_QUEUE_FAMILY = 0xffffffffu;
constexpr VkBool32 VK_FALSE = 0;
constexpr VkBool32 VK_TRUE = 1;
constexpr VkFormat VK_FORMAT_D32_SFLOAT = 126;
constexpr VkFormat VK_FORMAT_R8G8B8A8_UNORM = 37;
constexpr VkFormat VK_FORMAT_R32G32_SFLOAT = 103;
constexpr VkFormat VK_FORMAT_R32G32B32_SFLOAT = 106;

struct VkExtent2D {
    std::uint32_t width;
    std::uint32_t height;
};

struct VkRect2D;

struct VkExtent3D {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
};

struct VkApplicationInfo {
    VkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    std::uint32_t applicationVersion;
    const char* pEngineName;
    std::uint32_t engineVersion;
    std::uint32_t apiVersion;
};

struct VkInstanceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

struct VkDeviceQueueCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t queueFamilyIndex;
    std::uint32_t queueCount;
    const float* pQueuePriorities;
};

struct VkDeviceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t queueCreateInfoCount;
    const VkDeviceQueueCreateInfo* pQueueCreateInfos;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const void* pEnabledFeatures;
};

struct VkQueueFamilyProperties {
    VkFlags queueFlags;
    std::uint32_t queueCount;
    std::uint32_t timestampValidBits;
    VkExtent3D minImageTransferGranularity;
};

struct VkSurfaceCapabilitiesKHR {
    std::uint32_t minImageCount;
    std::uint32_t maxImageCount;
    VkExtent2D currentExtent;
    VkExtent2D minImageExtent;
    VkExtent2D maxImageExtent;
    std::uint32_t maxImageArrayLayers;
    VkFlags supportedTransforms;
    VkFlags currentTransform;
    VkFlags supportedCompositeAlpha;
    VkFlags supportedUsageFlags;
};

struct VkSurfaceFormatKHR {
    VkFormat format;
    VkColorSpaceKHR colorSpace;
};

struct VkSwapchainCreateInfoKHR {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkSurfaceKHR surface;
    std::uint32_t minImageCount;
    VkFormat imageFormat;
    VkColorSpaceKHR imageColorSpace;
    VkExtent2D imageExtent;
    std::uint32_t imageArrayLayers;
    VkFlags imageUsage;
    VkSharingMode imageSharingMode;
    std::uint32_t queueFamilyIndexCount;
    const std::uint32_t* pQueueFamilyIndices;
    VkFlags preTransform;
    VkFlags compositeAlpha;
    VkPresentModeKHR presentMode;
    VkBool32 clipped;
    VkSwapchainKHR oldSwapchain;
};

struct VkComponentMapping {
    VkComponentSwizzle r;
    VkComponentSwizzle g;
    VkComponentSwizzle b;
    VkComponentSwizzle a;
};

struct VkImageSubresourceRange {
    VkFlags aspectMask;
    std::uint32_t baseMipLevel;
    std::uint32_t levelCount;
    std::uint32_t baseArrayLayer;
    std::uint32_t layerCount;
};

struct VkImageSubresourceLayers {
    VkFlags aspectMask;
    std::uint32_t mipLevel;
    std::uint32_t baseArrayLayer;
    std::uint32_t layerCount;
};

struct VkBufferImageCopy {
    std::uint64_t bufferOffset;
    std::uint32_t bufferRowLength;
    std::uint32_t bufferImageHeight;
    VkImageSubresourceLayers imageSubresource;
    struct {
        std::int32_t x;
        std::int32_t y;
        std::int32_t z;
    } imageOffset;
    VkExtent3D imageExtent;
};

struct VkImageMemoryBarrier {
    VkStructureType sType;
    const void* pNext;
    VkFlags srcAccessMask;
    VkFlags dstAccessMask;
    std::uint32_t oldLayout;
    std::uint32_t newLayout;
    std::uint32_t srcQueueFamilyIndex;
    std::uint32_t dstQueueFamilyIndex;
    VkImage image;
    VkImageSubresourceRange subresourceRange;
};

struct VkShaderModuleCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::size_t codeSize;
    const std::uint32_t* pCode;
};

struct VkPipelineLayoutCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t setLayoutCount;
    const VkDescriptorSetLayout* pSetLayouts;
    std::uint32_t pushConstantRangeCount;
    const struct VkPushConstantRange* pPushConstantRanges;
};

struct VkPushConstantRange {
    VkShaderStageFlags stageFlags;
    std::uint32_t offset;
    std::uint32_t size;
};

struct VkDescriptorSetLayoutBinding {
    std::uint32_t binding;
    std::uint32_t descriptorType;
    std::uint32_t descriptorCount;
    VkShaderStageFlags stageFlags;
    const VkSampler* pImmutableSamplers;
};

struct VkDescriptorSetLayoutCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t bindingCount;
    const VkDescriptorSetLayoutBinding* pBindings;
};

struct VkDescriptorPoolSize {
    std::uint32_t type;
    std::uint32_t descriptorCount;
};

struct VkDescriptorPoolCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t maxSets;
    std::uint32_t poolSizeCount;
    const VkDescriptorPoolSize* pPoolSizes;
};

struct VkDescriptorSetAllocateInfo {
    VkStructureType sType;
    const void* pNext;
    VkDescriptorPool descriptorPool;
    std::uint32_t descriptorSetCount;
    const VkDescriptorSetLayout* pSetLayouts;
};

struct VkDescriptorImageInfo {
    VkSampler sampler;
    VkImageView imageView;
    std::uint32_t imageLayout;
};

struct VkWriteDescriptorSet {
    VkStructureType sType;
    const void* pNext;
    VkDescriptorSet dstSet;
    std::uint32_t dstBinding;
    std::uint32_t dstArrayElement;
    std::uint32_t descriptorCount;
    std::uint32_t descriptorType;
    const VkDescriptorImageInfo* pImageInfo;
    const void* pBufferInfo;
    const void* pTexelBufferView;
};

struct VkSamplerCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t magFilter;
    std::uint32_t minFilter;
    std::uint32_t mipmapMode;
    std::uint32_t addressModeU;
    std::uint32_t addressModeV;
    std::uint32_t addressModeW;
    float mipLodBias;
    VkBool32 anisotropyEnable;
    float maxAnisotropy;
    VkBool32 compareEnable;
    std::uint32_t compareOp;
    float minLod;
    float maxLod;
    std::uint32_t borderColor;
    VkBool32 unnormalizedCoordinates;
};

struct VkPipelineShaderStageCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkShaderStageFlags stage;
    VkShaderModule module;
    const char* pName;
    const void* pSpecializationInfo;
};

struct VkVertexInputBindingDescription {
    std::uint32_t binding;
    std::uint32_t stride;
    std::uint32_t inputRate;
};

struct VkVertexInputAttributeDescription {
    std::uint32_t location;
    std::uint32_t binding;
    VkFormat format;
    std::uint32_t offset;
};

struct VkPipelineVertexInputStateCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t vertexBindingDescriptionCount;
    const VkVertexInputBindingDescription* pVertexBindingDescriptions;
    std::uint32_t vertexAttributeDescriptionCount;
    const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
};

struct VkPipelineInputAssemblyStateCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t topology;
    VkBool32 primitiveRestartEnable;
};

struct VkViewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
};

struct VkPipelineViewportStateCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t viewportCount;
    const VkViewport* pViewports;
    std::uint32_t scissorCount;
    const VkRect2D* pScissors;
};

struct VkPipelineRasterizationStateCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkBool32 depthClampEnable;
    VkBool32 rasterizerDiscardEnable;
    std::uint32_t polygonMode;
    VkFlags cullMode;
    std::uint32_t frontFace;
    VkBool32 depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
    float lineWidth;
};

struct VkPipelineMultisampleStateCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t rasterizationSamples;
    VkBool32 sampleShadingEnable;
    float minSampleShading;
    const std::uint32_t* pSampleMask;
    VkBool32 alphaToCoverageEnable;
    VkBool32 alphaToOneEnable;
};

struct VkStencilOpState {
    std::uint32_t failOp;
    std::uint32_t passOp;
    std::uint32_t depthFailOp;
    std::uint32_t compareOp;
    std::uint32_t compareMask;
    std::uint32_t writeMask;
    std::uint32_t reference;
};

struct VkPipelineDepthStencilStateCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkBool32 depthTestEnable;
    VkBool32 depthWriteEnable;
    std::uint32_t depthCompareOp;
    VkBool32 depthBoundsTestEnable;
    VkBool32 stencilTestEnable;
    VkStencilOpState front;
    VkStencilOpState back;
    float minDepthBounds;
    float maxDepthBounds;
};

struct VkPipelineColorBlendAttachmentState {
    VkBool32 blendEnable;
    std::uint32_t srcColorBlendFactor;
    std::uint32_t dstColorBlendFactor;
    std::uint32_t colorBlendOp;
    std::uint32_t srcAlphaBlendFactor;
    std::uint32_t dstAlphaBlendFactor;
    std::uint32_t alphaBlendOp;
    VkFlags colorWriteMask;
};

struct VkPipelineColorBlendStateCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkBool32 logicOpEnable;
    std::uint32_t logicOp;
    std::uint32_t attachmentCount;
    const VkPipelineColorBlendAttachmentState* pAttachments;
    float blendConstants[4];
};

struct VkGraphicsPipelineCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t stageCount;
    const VkPipelineShaderStageCreateInfo* pStages;
    const void* pVertexInputState;
    const void* pInputAssemblyState;
    const void* pTessellationState;
    const void* pViewportState;
    const void* pRasterizationState;
    const void* pMultisampleState;
    const void* pDepthStencilState;
    const void* pColorBlendState;
    const void* pDynamicState;
    VkPipelineLayout layout;
    VkRenderPass renderPass;
    std::uint32_t subpass;
    VkPipeline basePipelineHandle;
    std::int32_t basePipelineIndex;
};

struct VkImageCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkImageType imageType;
    VkFormat format;
    VkExtent3D extent;
    std::uint32_t mipLevels;
    std::uint32_t arrayLayers;
    std::uint32_t samples;
    VkImageTiling tiling;
    VkFlags usage;
    VkSharingMode sharingMode;
    std::uint32_t queueFamilyIndexCount;
    const std::uint32_t* pQueueFamilyIndices;
    std::uint32_t initialLayout;
};

struct VkBufferCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint64_t size;
    VkBufferUsageFlags usage;
    VkSharingMode sharingMode;
    std::uint32_t queueFamilyIndexCount;
    const std::uint32_t* pQueueFamilyIndices;
};

struct VkMemoryType {
    VkMemoryPropertyFlags propertyFlags;
    std::uint32_t heapIndex;
};

struct VkMemoryHeap {
    std::uint64_t size;
    VkFlags flags;
};

struct VkPhysicalDeviceMemoryProperties {
    std::uint32_t memoryTypeCount;
    VkMemoryType memoryTypes[32];
    std::uint32_t memoryHeapCount;
    VkMemoryHeap memoryHeaps[16];
};

struct VkMemoryRequirements {
    std::uint64_t size;
    std::uint64_t alignment;
    std::uint32_t memoryTypeBits;
};

struct VkMemoryAllocateInfo {
    VkStructureType sType;
    const void* pNext;
    std::uint64_t allocationSize;
    std::uint32_t memoryTypeIndex;
};

struct VkImageViewCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkImage image;
    VkImageViewType viewType;
    VkFormat format;
    VkComponentMapping components;
    VkImageSubresourceRange subresourceRange;
};

struct VkAttachmentDescription {
    VkFlags flags;
    VkFormat format;
    std::uint32_t samples;
    std::uint32_t loadOp;
    std::uint32_t storeOp;
    std::uint32_t stencilLoadOp;
    std::uint32_t stencilStoreOp;
    std::uint32_t initialLayout;
    std::uint32_t finalLayout;
};

struct VkAttachmentReference {
    std::uint32_t attachment;
    std::uint32_t layout;
};

struct VkSubpassDescription {
    VkFlags flags;
    std::uint32_t pipelineBindPoint;
    std::uint32_t inputAttachmentCount;
    const void* pInputAttachments;
    std::uint32_t colorAttachmentCount;
    const VkAttachmentReference* pColorAttachments;
    const void* pResolveAttachments;
    const void* pDepthStencilAttachment;
    std::uint32_t preserveAttachmentCount;
    const std::uint32_t* pPreserveAttachments;
};

struct VkSubpassDependency {
    std::uint32_t srcSubpass;
    std::uint32_t dstSubpass;
    VkFlags srcStageMask;
    VkFlags dstStageMask;
    VkFlags srcAccessMask;
    VkFlags dstAccessMask;
    VkFlags dependencyFlags;
};

struct VkRenderPassCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t attachmentCount;
    const VkAttachmentDescription* pAttachments;
    std::uint32_t subpassCount;
    const VkSubpassDescription* pSubpasses;
    std::uint32_t dependencyCount;
    const VkSubpassDependency* pDependencies;
};

struct VkFramebufferCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkRenderPass renderPass;
    std::uint32_t attachmentCount;
    const VkImageView* pAttachments;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t layers;
};

struct VkCommandPoolCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t queueFamilyIndex;
};

struct VkCommandBufferAllocateInfo {
    VkStructureType sType;
    const void* pNext;
    VkCommandPool commandPool;
    VkCommandBufferLevel level;
    std::uint32_t commandBufferCount;
};

struct VkCommandBufferBeginInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    const void* pInheritanceInfo;
};

struct VkSemaphoreCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
};

struct VkFenceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
};

struct VkOffset2D {
    std::int32_t x;
    std::int32_t y;
};

struct VkRect2D {
    VkOffset2D offset;
    VkExtent2D extent;
};

struct VkClearColorValue {
    float float32[4];
};

struct VkClearDepthStencilValue {
    float depth;
    std::uint32_t stencil;
};

struct VkClearValue {
    union {
        VkClearColorValue color;
        VkClearDepthStencilValue depthStencil;
    };
};

struct VkRenderPassBeginInfo {
    VkStructureType sType;
    const void* pNext;
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
    VkRect2D renderArea;
    std::uint32_t clearValueCount;
    const VkClearValue* pClearValues;
};

struct VkSubmitInfo {
    VkStructureType sType;
    const void* pNext;
    std::uint32_t waitSemaphoreCount;
    const VkSemaphore* pWaitSemaphores;
    const VkPipelineStageFlags* pWaitDstStageMask;
    std::uint32_t commandBufferCount;
    const VkCommandBuffer* pCommandBuffers;
    std::uint32_t signalSemaphoreCount;
    const VkSemaphore* pSignalSemaphores;
};

struct VkPresentInfoKHR {
    VkStructureType sType;
    const void* pNext;
    std::uint32_t waitSemaphoreCount;
    const VkSemaphore* pWaitSemaphores;
    std::uint32_t swapchainCount;
    const VkSwapchainKHR* pSwapchains;
    const std::uint32_t* pImageIndices;
    VkResult* pResults;
};

struct VkWin32SurfaceCreateInfoKHR {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    HINSTANCE hinstance;
    HWND hwnd;
};

using PFN_vkVoidFunction = void (*)();
using PFN_vkGetInstanceProcAddr = PFN_vkVoidFunction (*)(VkInstance, const char*);
using PFN_vkGetDeviceProcAddr = PFN_vkVoidFunction (*)(VkDevice, const char*);
using PFN_vkCreateInstance = VkResult (*)(const VkInstanceCreateInfo*, const void*, VkInstance*);
using PFN_vkDestroyInstance = void (*)(VkInstance, const void*);
using PFN_vkEnumeratePhysicalDevices = VkResult (*)(VkInstance, std::uint32_t*, VkPhysicalDevice*);
using PFN_vkCreateWin32SurfaceKHR = VkResult (*)(VkInstance, const VkWin32SurfaceCreateInfoKHR*, const void*, VkSurfaceKHR*);
using PFN_vkDestroySurfaceKHR = void (*)(VkInstance, VkSurfaceKHR, const void*);
using PFN_vkGetPhysicalDeviceQueueFamilyProperties = void (*)(VkPhysicalDevice, std::uint32_t*, VkQueueFamilyProperties*);
using PFN_vkGetPhysicalDeviceMemoryProperties = void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*);
using PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR = VkBool32 (*)(VkPhysicalDevice, std::uint32_t);
using PFN_vkCreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice*);
using PFN_vkDestroyDevice = void (*)(VkDevice, const void*);
using PFN_vkGetDeviceQueue = void (*)(VkDevice, std::uint32_t, std::uint32_t, VkQueue*);
using PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR*);
using PFN_vkGetPhysicalDeviceSurfaceFormatsKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, std::uint32_t*, VkSurfaceFormatKHR*);
using PFN_vkGetPhysicalDeviceSurfacePresentModesKHR = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR, std::uint32_t*, VkPresentModeKHR*);
using PFN_vkCreateSwapchainKHR = VkResult (*)(VkDevice, const VkSwapchainCreateInfoKHR*, const void*, VkSwapchainKHR*);
using PFN_vkDestroySwapchainKHR = void (*)(VkDevice, VkSwapchainKHR, const void*);
using PFN_vkGetSwapchainImagesKHR = VkResult (*)(VkDevice, VkSwapchainKHR, std::uint32_t*, VkImage*);
using PFN_vkCreateImage = VkResult (*)(VkDevice, const VkImageCreateInfo*, const void*, VkImage*);
using PFN_vkDestroyImage = void (*)(VkDevice, VkImage, const void*);
using PFN_vkCreateImageView = VkResult (*)(VkDevice, const VkImageViewCreateInfo*, const void*, VkImageView*);
using PFN_vkDestroyImageView = void (*)(VkDevice, VkImageView, const void*);
using PFN_vkCreateShaderModule = VkResult (*)(VkDevice, const VkShaderModuleCreateInfo*, const void*, VkShaderModule*);
using PFN_vkDestroyShaderModule = void (*)(VkDevice, VkShaderModule, const void*);
using PFN_vkCreateBuffer = VkResult (*)(VkDevice, const VkBufferCreateInfo*, const void*, VkBuffer*);
using PFN_vkDestroyBuffer = void (*)(VkDevice, VkBuffer, const void*);
using PFN_vkGetBufferMemoryRequirements = void (*)(VkDevice, VkBuffer, VkMemoryRequirements*);
using PFN_vkGetImageMemoryRequirements = void (*)(VkDevice, VkImage, VkMemoryRequirements*);
using PFN_vkAllocateMemory = VkResult (*)(VkDevice, const VkMemoryAllocateInfo*, const void*, VkDeviceMemory*);
using PFN_vkFreeMemory = void (*)(VkDevice, VkDeviceMemory, const void*);
using PFN_vkBindBufferMemory = VkResult (*)(VkDevice, VkBuffer, VkDeviceMemory, std::uint64_t);
using PFN_vkBindImageMemory = VkResult (*)(VkDevice, VkImage, VkDeviceMemory, std::uint64_t);
using PFN_vkMapMemory = VkResult (*)(VkDevice, VkDeviceMemory, std::uint64_t, std::uint64_t, VkFlags, void**);
using PFN_vkUnmapMemory = void (*)(VkDevice, VkDeviceMemory);
using PFN_vkCreatePipelineLayout = VkResult (*)(VkDevice, const VkPipelineLayoutCreateInfo*, const void*, VkPipelineLayout*);
using PFN_vkDestroyPipelineLayout = void (*)(VkDevice, VkPipelineLayout, const void*);
using PFN_vkCreateDescriptorSetLayout = VkResult (*)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const void*, VkDescriptorSetLayout*);
using PFN_vkDestroyDescriptorSetLayout = void (*)(VkDevice, VkDescriptorSetLayout, const void*);
using PFN_vkCreateDescriptorPool = VkResult (*)(VkDevice, const VkDescriptorPoolCreateInfo*, const void*, VkDescriptorPool*);
using PFN_vkDestroyDescriptorPool = void (*)(VkDevice, VkDescriptorPool, const void*);
using PFN_vkAllocateDescriptorSets = VkResult (*)(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
using PFN_vkUpdateDescriptorSets = void (*)(VkDevice, std::uint32_t, const VkWriteDescriptorSet*, std::uint32_t, const void*);
using PFN_vkCreateSampler = VkResult (*)(VkDevice, const VkSamplerCreateInfo*, const void*, VkSampler*);
using PFN_vkDestroySampler = void (*)(VkDevice, VkSampler, const void*);
using PFN_vkCreateGraphicsPipelines = VkResult (*)(VkDevice, void*, std::uint32_t, const VkGraphicsPipelineCreateInfo*, const void*, VkPipeline*);
using PFN_vkDestroyPipeline = void (*)(VkDevice, VkPipeline, const void*);
using PFN_vkCreateRenderPass = VkResult (*)(VkDevice, const VkRenderPassCreateInfo*, const void*, VkRenderPass*);
using PFN_vkDestroyRenderPass = void (*)(VkDevice, VkRenderPass, const void*);
using PFN_vkCreateFramebuffer = VkResult (*)(VkDevice, const VkFramebufferCreateInfo*, const void*, VkFramebuffer*);
using PFN_vkDestroyFramebuffer = void (*)(VkDevice, VkFramebuffer, const void*);
using PFN_vkCreateCommandPool = VkResult (*)(VkDevice, const VkCommandPoolCreateInfo*, const void*, VkCommandPool*);
using PFN_vkDestroyCommandPool = void (*)(VkDevice, VkCommandPool, const void*);
using PFN_vkAllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
using PFN_vkFreeCommandBuffers = void (*)(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*);
using PFN_vkResetCommandBuffer = VkResult (*)(VkCommandBuffer, VkFlags);
using PFN_vkBeginCommandBuffer = VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
using PFN_vkEndCommandBuffer = VkResult (*)(VkCommandBuffer);
using PFN_vkCmdBeginRenderPass = void (*)(VkCommandBuffer, const VkRenderPassBeginInfo*, std::uint32_t);
using PFN_vkCmdEndRenderPass = void (*)(VkCommandBuffer);
using PFN_vkCmdBindPipeline = void (*)(VkCommandBuffer, std::uint32_t, VkPipeline);
using PFN_vkCmdPushConstants = void (*)(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*);
using PFN_vkCmdBindDescriptorSets = void (*)(VkCommandBuffer, std::uint32_t, VkPipelineLayout, std::uint32_t, std::uint32_t, const VkDescriptorSet*, std::uint32_t, const std::uint32_t*);
using PFN_vkCmdBindVertexBuffers = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t, const VkBuffer*, const std::uint64_t*);
using PFN_vkCmdBindIndexBuffer = void (*)(VkCommandBuffer, VkBuffer, std::uint64_t, std::uint32_t);
using PFN_vkCmdDrawIndexed = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t);
using PFN_vkCmdPipelineBarrier = void (*)(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkFlags, std::uint32_t, const void*, std::uint32_t, const void*, std::uint32_t, const VkImageMemoryBarrier*);
using PFN_vkCmdCopyBufferToImage = void (*)(VkCommandBuffer, VkBuffer, VkImage, std::uint32_t, std::uint32_t, const VkBufferImageCopy*);
using PFN_vkCreateSemaphore = VkResult (*)(VkDevice, const VkSemaphoreCreateInfo*, const void*, VkSemaphore*);
using PFN_vkDestroySemaphore = void (*)(VkDevice, VkSemaphore, const void*);
using PFN_vkCreateFence = VkResult (*)(VkDevice, const VkFenceCreateInfo*, const void*, VkFence*);
using PFN_vkDestroyFence = void (*)(VkDevice, VkFence, const void*);
using PFN_vkWaitForFences = VkResult (*)(VkDevice, std::uint32_t, const VkFence*, VkBool32, std::uint64_t);
using PFN_vkResetFences = VkResult (*)(VkDevice, std::uint32_t, const VkFence*);
using PFN_vkAcquireNextImageKHR = VkResult (*)(VkDevice, VkSwapchainKHR, std::uint64_t, VkSemaphore, VkFence, std::uint32_t*);
using PFN_vkQueueSubmit = VkResult (*)(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence);
using PFN_vkQueuePresentKHR = VkResult (*)(VkQueue, const VkPresentInfoKHR*);
using PFN_vkQueueWaitIdle = VkResult (*)(VkQueue);

struct VulkanState {
    HMODULE loader = nullptr;
    HWND window = nullptr;
    HWND hostParentWindow = nullptr;
    HINSTANCE windowInstance = nullptr;
    ATOM windowClass = 0;
    bool closeRequested = false;
    bool initialized = false;
    bool embeddedMode = false;
    std::wstring className;
    std::wstring title;
    RTColor clearColor{11, 18, 28, 255};
    LARGE_INTEGER timerFrequency{};
    LARGE_INTEGER frameStart{};
    float lastFrameTime = 0.0f;
    HDC backbufferDC = nullptr;
    HBITMAP backbufferBitmap = nullptr;
    HGDIOBJ backbufferOldObject = nullptr;
    int backbufferWidth = 0;
    int backbufferHeight = 0;
    int requestedX = CW_USEDEFAULT;
    int requestedY = CW_USEDEFAULT;
    int requestedWidth = 1280;
    int requestedHeight = 720;
    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;
    VkPhysicalDevice physicalDevice = nullptr;
    VkDevice device = nullptr;
    VkQueue graphicsQueue = nullptr;
    VkSwapchainKHR swapchain = nullptr;
    VkImage depthImage = nullptr;
    VkDeviceMemory depthImageMemory = nullptr;
    VkImageView depthImageView = nullptr;
    VkShaderModule demoVertexShader = nullptr;
    VkShaderModule demoFragmentShader = nullptr;
    VkDescriptorSetLayout sceneDescriptorSetLayout = nullptr;
    VkDescriptorPool sceneDescriptorPool = nullptr;
    VkDescriptorSet sceneDescriptorSet = nullptr;
    VkSampler sceneTextureSampler = nullptr;
    VkImage sceneTextureImage = nullptr;
    VkDeviceMemory sceneTextureMemory = nullptr;
    VkImageView sceneTextureView = nullptr;
    int sceneTextureWidth = 0;
    int sceneTextureHeight = 0;
    VkPipelineLayout demoPipelineLayout = nullptr;
    VkPipeline demoGraphicsPipeline = nullptr;
    VkBuffer demoVertexBuffer = nullptr;
    VkDeviceMemory demoVertexMemory = nullptr;
    VkBuffer demoIndexBuffer = nullptr;
    VkDeviceMemory demoIndexMemory = nullptr;
    VkRenderPass renderPass = nullptr;
    VkCommandPool commandPool = nullptr;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore = nullptr;
    VkSemaphore renderFinishedSemaphore = nullptr;
    VkFence inFlightFence = nullptr;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkFormat swapchainFormat = 0;
    VkExtent2D swapchainExtent{0, 0};
    std::uint32_t graphicsQueueFamilyIndex = VK_INVALID_QUEUE_FAMILY;
    std::uint32_t currentImageIndex = 0;
    std::uint32_t gpuCount = 0;
    RTVec3 cameraPosition{6.0f, 6.0f, 6.0f};
    RTVec3 cameraTarget{0.0f, 0.0f, 0.0f};
    RTVec3 cameraUp{0.0f, 1.0f, 0.0f};
    float cameraFov = 45.0f;
    bool stablePreviewMode = true;
    bool surfaceReady = false;
    bool deviceReady = false;
    bool presentationReady = false;
    bool swapchainReady = false;
    bool renderPassReady = false;
    bool framebuffersReady = false;
    bool depthReady = false;
    bool geometryBuffersReady = false;
    bool shaderAssetsReady = false;
    bool shaderModulesReady = false;
    bool pipelineLayoutReady = false;
    bool descriptorSetReady = false;
    bool textureSamplerReady = false;
    bool textureImageReady = false;
    bool graphicsPipelineReady = false;
    bool commandPoolReady = false;
    bool commandBuffersReady = false;
    bool syncReady = false;
    bool framePathReady = false;
    bool frameAcquired = false;
    bool resizePending = false;
    std::uint64_t presentedFrames = 0;
    std::uint64_t demoVertexBufferBytes = 0;
    std::uint64_t demoIndexBufferBytes = 0;
    std::uint64_t demoVertexBufferCapacity = 0;
    std::uint64_t demoIndexBufferCapacity = 0;
    std::uint32_t sceneIndexCount = 0;
    bool useSceneRendererThisFrame = false;
    std::wstring shaderAssetRoot;
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
    PFN_vkCreateInstance createInstance = nullptr;
    PFN_vkDestroyInstance destroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices = nullptr;
    PFN_vkCreateWin32SurfaceKHR createWin32Surface = nullptr;
    PFN_vkDestroySurfaceKHR destroySurface = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR getWin32PresentationSupport = nullptr;
    PFN_vkCreateDevice createDevice = nullptr;
    PFN_vkDestroyDevice destroyDevice = nullptr;
    PFN_vkGetDeviceQueue getDeviceQueue = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getSurfaceCapabilities = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getSurfaceFormats = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR getPresentModes = nullptr;
    PFN_vkCreateSwapchainKHR createSwapchain = nullptr;
    PFN_vkDestroySwapchainKHR destroySwapchain = nullptr;
    PFN_vkGetSwapchainImagesKHR getSwapchainImages = nullptr;
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkDestroyImage destroyImage = nullptr;
    PFN_vkCreateImageView createImageView = nullptr;
    PFN_vkDestroyImageView destroyImageView = nullptr;
    PFN_vkCreateShaderModule createShaderModule = nullptr;
    PFN_vkDestroyShaderModule destroyShaderModule = nullptr;
    PFN_vkCreateBuffer createBuffer = nullptr;
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements getBufferMemoryRequirements = nullptr;
    PFN_vkGetImageMemoryRequirements getImageMemoryRequirements = nullptr;
    PFN_vkAllocateMemory allocateMemory = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    PFN_vkBindBufferMemory bindBufferMemory = nullptr;
    PFN_vkBindImageMemory bindImageMemory = nullptr;
    PFN_vkMapMemory mapMemory = nullptr;
    PFN_vkUnmapMemory unmapMemory = nullptr;
    PFN_vkCreatePipelineLayout createPipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout destroyPipelineLayout = nullptr;
    PFN_vkCreateDescriptorSetLayout createDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout destroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool createDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool destroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets allocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets updateDescriptorSets = nullptr;
    PFN_vkCreateSampler createSampler = nullptr;
    PFN_vkDestroySampler destroySampler = nullptr;
    PFN_vkCreateGraphicsPipelines createGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline destroyPipeline = nullptr;
    PFN_vkCreateRenderPass createRenderPass = nullptr;
    PFN_vkDestroyRenderPass destroyRenderPass = nullptr;
    PFN_vkCreateFramebuffer createFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer destroyFramebuffer = nullptr;
    PFN_vkCreateCommandPool createCommandPool = nullptr;
    PFN_vkDestroyCommandPool destroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers allocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers freeCommandBuffers = nullptr;
    PFN_vkResetCommandBuffer resetCommandBuffer = nullptr;
    PFN_vkBeginCommandBuffer beginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer endCommandBuffer = nullptr;
    PFN_vkCmdBeginRenderPass cmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass cmdEndRenderPass = nullptr;
    PFN_vkCmdBindPipeline cmdBindPipeline = nullptr;
    PFN_vkCmdPushConstants cmdPushConstants = nullptr;
    PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets = nullptr;
    PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer = nullptr;
    PFN_vkCmdDrawIndexed cmdDrawIndexed = nullptr;
    PFN_vkCmdPipelineBarrier cmdPipelineBarrier = nullptr;
    PFN_vkCmdCopyBufferToImage cmdCopyBufferToImage = nullptr;
    PFN_vkCreateSemaphore createSemaphore = nullptr;
    PFN_vkDestroySemaphore destroySemaphore = nullptr;
    PFN_vkCreateFence createFence = nullptr;
    PFN_vkDestroyFence destroyFence = nullptr;
    PFN_vkWaitForFences waitForFences = nullptr;
    PFN_vkResetFences resetFences = nullptr;
    PFN_vkAcquireNextImageKHR acquireNextImage = nullptr;
    PFN_vkQueueSubmit queueSubmit = nullptr;
    PFN_vkQueuePresentKHR queuePresent = nullptr;
    PFN_vkQueueWaitIdle queueWaitIdle = nullptr;
};

struct SceneVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float er = 0.0f;
    float eg = 0.0f;
    float eb = 0.0f;
    float roughness = 0.5f;
    float metallic = 0.0f;
    float textured = 0.0f;
};

struct ScenePushConstants {
    float mvp[16]{};
    float viewportPostfx[4]{};
    float fogParams[4]{};
    float fogColor[4]{};
    float grading[4]{};
};

struct Mat4 {
    float value[16]{};
};

inline VulkanState& state() {
    static VulkanState value;
    return value;
}

inline std::vector<SceneVertex>& frame_vertices() {
    static std::vector<SceneVertex> value;
    return value;
}

inline std::vector<std::uint16_t>& frame_indices() {
    static std::vector<std::uint16_t> value;
    return value;
}

inline ScenePushConstants make_scene_push_constants();
inline void create_swapchain();
inline void create_depth_resources();
inline void create_render_targets();
inline void create_command_infra();
inline void create_pipeline_scaffold();
inline void recreate_swapchain_dependent_resources();

inline bool trace_enabled() {
    static bool enabled = []() {
        const char* value = std::getenv("RAYQUIRO_VULKAN_TRACE");
        return value != nullptr && value[0] != '\0' && std::string(value) != "0";
    }();
    return enabled;
}

inline std::filesystem::path trace_file_path() {
    if (const char* custom = std::getenv("RAYQUIRO_VULKAN_TRACE_FILE")) {
        if (custom[0] != '\0') {
            return std::filesystem::path(custom);
        }
    }
    return std::filesystem::temp_directory_path() / "rayquiro-vulkan-trace.log";
}

inline void trace_step(const std::string& message) {
    if (!trace_enabled()) {
        return;
    }

    try {
        std::ofstream output(trace_file_path(), std::ios::app);
        output << message << "\n";
    } catch (...) {
    }
}

inline void ensure_backbuffer();
inline void blit_backbuffer_to_window();
inline void upload_scene_texture_rgba(int width, int height, const unsigned char* pixels);

inline std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

inline std::string format_error(const std::string& prefix, DWORD code) {
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );

    std::string message = prefix;
    if (length != 0 && buffer != nullptr) {
        const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<size_t>(utf8Size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), utf8.data(), utf8Size, nullptr, nullptr);
        message += ": " + utf8;
    } else {
        message += ": Win32 error " + std::to_string(code);
    }

    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return message;
}

inline LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    (void)lParam;
    auto& vk = state();

    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        if (dc != nullptr && vk.backbufferDC != nullptr && vk.backbufferWidth > 0 && vk.backbufferHeight > 0) {
            BitBlt(dc, 0, 0, vk.backbufferWidth, vk.backbufferHeight, vk.backbufferDC, 0, 0, SRCCOPY);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_SIZE:
        if (vk.window == hwnd) {
            ensure_backbuffer();
            if (vk.initialized) {
                const int width = LOWORD(lParam);
                const int height = HIWORD(lParam);
                if (width > 0 && height > 0) {
                    vk.resizePending = true;
                }
            }
        }
        return 0;
    case WM_CLOSE:
        vk.closeRequested = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        vk.closeRequested = true;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

inline void pump_messages() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

inline bool runtime_available() {
    HMODULE handle = LoadLibraryW(L"vulkan-1.dll");
    if (handle == nullptr) {
        return false;
    }
    FreeLibrary(handle);
    return true;
}

inline void ensure_loader() {
    auto& vk = state();
    if (vk.loader != nullptr) {
        return;
    }

    vk.loader = LoadLibraryW(L"vulkan-1.dll");
    if (vk.loader == nullptr) {
        throw std::runtime_error("Vulkan loader was not found. Install a Vulkan runtime so engine.backend(\"vulkan\") can start.");
    }

    vk.getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vk.loader, "vkGetInstanceProcAddr"));
    vk.getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(vk.loader, "vkGetDeviceProcAddr"));
    vk.createInstance = reinterpret_cast<PFN_vkCreateInstance>(GetProcAddress(vk.loader, "vkCreateInstance"));
    if (vk.getInstanceProcAddr == nullptr || vk.getDeviceProcAddr == nullptr || vk.createInstance == nullptr) {
        throw std::runtime_error("Failed to resolve Vulkan entry points from vulkan-1.dll.");
    }
}

inline void create_instance() {
    auto& vk = state();
    if (vk.instance != nullptr) {
        return;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RayQuiro";
    appInfo.applicationVersion = 2;
    appInfo.pEngineName = "Raytolfas Engine";
    appInfo.engineVersion = 2;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const char* instanceExtensions[] = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = instanceExtensions;

    const VkResult result = vk.createInstance(&createInfo, nullptr, &vk.instance);
    if (result != VK_SUCCESS || vk.instance == nullptr) {
        throw std::runtime_error("vkCreateInstance failed for the RayQuiro Vulkan backend skeleton.");
    }

    vk.destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(vk.getInstanceProcAddr(vk.instance, "vkDestroyInstance"));
    vk.enumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(vk.getInstanceProcAddr(vk.instance, "vkEnumeratePhysicalDevices"));
    vk.createWin32Surface = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vk.getInstanceProcAddr(vk.instance, "vkCreateWin32SurfaceKHR"));
    vk.destroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(vk.getInstanceProcAddr(vk.instance, "vkDestroySurfaceKHR"));
    vk.getQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(vk.getInstanceProcAddr(vk.instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    vk.getPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(vk.getInstanceProcAddr(vk.instance, "vkGetPhysicalDeviceMemoryProperties"));
    vk.getWin32PresentationSupport = reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(vk.getInstanceProcAddr(vk.instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
    vk.createDevice = reinterpret_cast<PFN_vkCreateDevice>(vk.getInstanceProcAddr(vk.instance, "vkCreateDevice"));
    vk.getSurfaceCapabilities = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vk.getInstanceProcAddr(vk.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    vk.getSurfaceFormats = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vk.getInstanceProcAddr(vk.instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    vk.getPresentModes = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(vk.getInstanceProcAddr(vk.instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));

    if (vk.createWin32Surface == nullptr || vk.destroySurface == nullptr || vk.getQueueFamilyProperties == nullptr || vk.getPhysicalDeviceMemoryProperties == nullptr || vk.createDevice == nullptr ||
        vk.getSurfaceCapabilities == nullptr || vk.getSurfaceFormats == nullptr || vk.getPresentModes == nullptr) {
        throw std::runtime_error("Failed to resolve required Vulkan instance functions for surface/device creation.");
    }

    if (vk.enumeratePhysicalDevices != nullptr) {
        std::uint32_t deviceCount = 0;
        if (vk.enumeratePhysicalDevices(vk.instance, &deviceCount, nullptr) == VK_SUCCESS) {
            vk.gpuCount = deviceCount;
        }
    }
}

inline void create_surface() {
    auto& vk = state();
    if (vk.surface != nullptr) {
        return;
    }
    if (vk.instance == nullptr || vk.window == nullptr) {
        throw std::runtime_error("Vulkan surface creation needs both a VkInstance and a Win32 window.");
    }

    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = vk.windowInstance;
    createInfo.hwnd = vk.window;

    const VkResult result = vk.createWin32Surface(vk.instance, &createInfo, nullptr, &vk.surface);
    if (result != VK_SUCCESS || vk.surface == nullptr) {
        throw std::runtime_error("vkCreateWin32SurfaceKHR failed for the RayQuiro Vulkan backend.");
    }

    vk.surfaceReady = true;
}

inline void pick_physical_device_and_queue_family() {
    auto& vk = state();
    if (vk.physicalDevice != nullptr && vk.graphicsQueueFamilyIndex != VK_INVALID_QUEUE_FAMILY) {
        return;
    }
    if (vk.enumeratePhysicalDevices == nullptr || vk.getQueueFamilyProperties == nullptr) {
        throw std::runtime_error("Vulkan physical-device selection is unavailable because required procedures were not resolved.");
    }

    std::uint32_t deviceCount = 0;
    if (vk.enumeratePhysicalDevices(vk.instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
        throw std::runtime_error("No Vulkan physical devices were found for the RayQuiro Vulkan backend.");
    }

    std::vector<VkPhysicalDevice> devices(static_cast<size_t>(deviceCount), nullptr);
    if (vk.enumeratePhysicalDevices(vk.instance, &deviceCount, devices.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to enumerate Vulkan physical devices.");
    }

    vk.gpuCount = deviceCount;

    for (VkPhysicalDevice device : devices) {
        std::uint32_t familyCount = 0;
        vk.getQueueFamilyProperties(device, &familyCount, nullptr);
        if (familyCount == 0) {
            continue;
        }

        std::vector<VkQueueFamilyProperties> families(static_cast<size_t>(familyCount));
        vk.getQueueFamilyProperties(device, &familyCount, families.data());

        for (std::uint32_t index = 0; index < familyCount; ++index) {
            const bool graphics = (families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            const bool present = vk.getWin32PresentationSupport == nullptr
                ? graphics
                : (vk.getWin32PresentationSupport(device, index) != 0);

            if (graphics && present) {
                vk.physicalDevice = device;
                vk.graphicsQueueFamilyIndex = index;
                vk.presentationReady = present;
                return;
            }
        }
    }

    throw std::runtime_error("No Vulkan queue family with graphics + Win32 presentation support was found.");
}

inline void create_logical_device() {
    auto& vk = state();
    if (vk.device != nullptr) {
        return;
    }
    if (vk.physicalDevice == nullptr || vk.graphicsQueueFamilyIndex == VK_INVALID_QUEUE_FAMILY) {
        throw std::runtime_error("Vulkan logical device creation needs a selected physical device and queue family.");
    }

    const float queuePriority = 1.0f;
    const char* deviceExtensions[] = {
        "VK_KHR_swapchain"
    };
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = vk.graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    const VkResult result = vk.createDevice(vk.physicalDevice, &deviceCreateInfo, nullptr, &vk.device);
    if (result != VK_SUCCESS || vk.device == nullptr) {
        throw std::runtime_error("vkCreateDevice failed for the RayQuiro Vulkan backend.");
    }

    const auto resolveDeviceProc = [&vk](const char* name) -> PFN_vkVoidFunction {
        PFN_vkVoidFunction proc = vk.getDeviceProcAddr(vk.device, name);
        if (proc == nullptr) {
            proc = vk.getInstanceProcAddr(vk.instance, name);
        }
        return proc;
    };

    vk.destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(resolveDeviceProc("vkDestroyDevice"));
    vk.getDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(resolveDeviceProc("vkGetDeviceQueue"));
    vk.createSwapchain = reinterpret_cast<PFN_vkCreateSwapchainKHR>(resolveDeviceProc("vkCreateSwapchainKHR"));
    vk.destroySwapchain = reinterpret_cast<PFN_vkDestroySwapchainKHR>(resolveDeviceProc("vkDestroySwapchainKHR"));
    vk.getSwapchainImages = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(resolveDeviceProc("vkGetSwapchainImagesKHR"));
    vk.createImage = reinterpret_cast<PFN_vkCreateImage>(resolveDeviceProc("vkCreateImage"));
    vk.destroyImage = reinterpret_cast<PFN_vkDestroyImage>(resolveDeviceProc("vkDestroyImage"));
    vk.createImageView = reinterpret_cast<PFN_vkCreateImageView>(resolveDeviceProc("vkCreateImageView"));
    vk.destroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(resolveDeviceProc("vkDestroyImageView"));
    vk.createShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(resolveDeviceProc("vkCreateShaderModule"));
    vk.destroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(resolveDeviceProc("vkDestroyShaderModule"));
    vk.createBuffer = reinterpret_cast<PFN_vkCreateBuffer>(resolveDeviceProc("vkCreateBuffer"));
    vk.destroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(resolveDeviceProc("vkDestroyBuffer"));
    vk.getBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(resolveDeviceProc("vkGetBufferMemoryRequirements"));
    vk.getImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(resolveDeviceProc("vkGetImageMemoryRequirements"));
    vk.allocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(resolveDeviceProc("vkAllocateMemory"));
    vk.freeMemory = reinterpret_cast<PFN_vkFreeMemory>(resolveDeviceProc("vkFreeMemory"));
    vk.bindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(resolveDeviceProc("vkBindBufferMemory"));
    vk.bindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(resolveDeviceProc("vkBindImageMemory"));
    vk.mapMemory = reinterpret_cast<PFN_vkMapMemory>(resolveDeviceProc("vkMapMemory"));
    vk.unmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(resolveDeviceProc("vkUnmapMemory"));
    vk.createPipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(resolveDeviceProc("vkCreatePipelineLayout"));
    vk.destroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(resolveDeviceProc("vkDestroyPipelineLayout"));
    vk.createDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(resolveDeviceProc("vkCreateDescriptorSetLayout"));
    vk.destroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(resolveDeviceProc("vkDestroyDescriptorSetLayout"));
    vk.createDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(resolveDeviceProc("vkCreateDescriptorPool"));
    vk.destroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(resolveDeviceProc("vkDestroyDescriptorPool"));
    vk.allocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(resolveDeviceProc("vkAllocateDescriptorSets"));
    vk.updateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(resolveDeviceProc("vkUpdateDescriptorSets"));
    vk.createSampler = reinterpret_cast<PFN_vkCreateSampler>(resolveDeviceProc("vkCreateSampler"));
    vk.destroySampler = reinterpret_cast<PFN_vkDestroySampler>(resolveDeviceProc("vkDestroySampler"));
    vk.createGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(resolveDeviceProc("vkCreateGraphicsPipelines"));
    vk.destroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(resolveDeviceProc("vkDestroyPipeline"));
    vk.createRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(resolveDeviceProc("vkCreateRenderPass"));
    vk.destroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(resolveDeviceProc("vkDestroyRenderPass"));
    vk.createFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(resolveDeviceProc("vkCreateFramebuffer"));
    vk.destroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(resolveDeviceProc("vkDestroyFramebuffer"));
    vk.createCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(resolveDeviceProc("vkCreateCommandPool"));
    vk.destroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(resolveDeviceProc("vkDestroyCommandPool"));
    vk.allocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(resolveDeviceProc("vkAllocateCommandBuffers"));
    vk.freeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(resolveDeviceProc("vkFreeCommandBuffers"));
    vk.resetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(resolveDeviceProc("vkResetCommandBuffer"));
    vk.beginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(resolveDeviceProc("vkBeginCommandBuffer"));
    vk.endCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(resolveDeviceProc("vkEndCommandBuffer"));
    vk.cmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(resolveDeviceProc("vkCmdBeginRenderPass"));
    vk.cmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(resolveDeviceProc("vkCmdEndRenderPass"));
    vk.cmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(resolveDeviceProc("vkCmdBindPipeline"));
    vk.cmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(resolveDeviceProc("vkCmdPushConstants"));
    vk.cmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(resolveDeviceProc("vkCmdBindDescriptorSets"));
    vk.cmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(resolveDeviceProc("vkCmdBindVertexBuffers"));
    vk.cmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(resolveDeviceProc("vkCmdBindIndexBuffer"));
    vk.cmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(resolveDeviceProc("vkCmdDrawIndexed"));
    vk.cmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(resolveDeviceProc("vkCmdPipelineBarrier"));
    vk.cmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(resolveDeviceProc("vkCmdCopyBufferToImage"));
    vk.createSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(resolveDeviceProc("vkCreateSemaphore"));
    vk.destroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(resolveDeviceProc("vkDestroySemaphore"));
    vk.createFence = reinterpret_cast<PFN_vkCreateFence>(resolveDeviceProc("vkCreateFence"));
    vk.destroyFence = reinterpret_cast<PFN_vkDestroyFence>(resolveDeviceProc("vkDestroyFence"));
    vk.waitForFences = reinterpret_cast<PFN_vkWaitForFences>(resolveDeviceProc("vkWaitForFences"));
    vk.resetFences = reinterpret_cast<PFN_vkResetFences>(resolveDeviceProc("vkResetFences"));
    vk.acquireNextImage = reinterpret_cast<PFN_vkAcquireNextImageKHR>(resolveDeviceProc("vkAcquireNextImageKHR"));
    vk.queueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(resolveDeviceProc("vkQueueSubmit"));
    vk.queuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(resolveDeviceProc("vkQueuePresentKHR"));
    vk.queueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(resolveDeviceProc("vkQueueWaitIdle"));
    if (vk.destroyDevice == nullptr || vk.getDeviceQueue == nullptr || vk.createSwapchain == nullptr ||
        vk.destroySwapchain == nullptr || vk.getSwapchainImages == nullptr || vk.createImage == nullptr || vk.destroyImage == nullptr ||
        vk.createImageView == nullptr || vk.destroyImageView == nullptr || vk.createShaderModule == nullptr || vk.destroyShaderModule == nullptr ||
        vk.createBuffer == nullptr || vk.destroyBuffer == nullptr ||
        vk.getBufferMemoryRequirements == nullptr || vk.getImageMemoryRequirements == nullptr || vk.allocateMemory == nullptr ||
        vk.freeMemory == nullptr || vk.bindBufferMemory == nullptr || vk.bindImageMemory == nullptr || vk.mapMemory == nullptr ||
        vk.unmapMemory == nullptr || vk.createPipelineLayout == nullptr || vk.destroyPipelineLayout == nullptr ||
        vk.createDescriptorSetLayout == nullptr || vk.destroyDescriptorSetLayout == nullptr || vk.createDescriptorPool == nullptr ||
        vk.destroyDescriptorPool == nullptr || vk.allocateDescriptorSets == nullptr || vk.updateDescriptorSets == nullptr ||
        vk.createSampler == nullptr || vk.destroySampler == nullptr ||
        vk.createGraphicsPipelines == nullptr || vk.destroyPipeline == nullptr || vk.createRenderPass == nullptr || vk.destroyRenderPass == nullptr ||
        vk.createFramebuffer == nullptr || vk.destroyFramebuffer == nullptr || vk.createCommandPool == nullptr ||
        vk.destroyCommandPool == nullptr || vk.allocateCommandBuffers == nullptr || vk.freeCommandBuffers == nullptr ||
        vk.resetCommandBuffer == nullptr || vk.beginCommandBuffer == nullptr || vk.endCommandBuffer == nullptr ||
        vk.cmdBeginRenderPass == nullptr || vk.cmdEndRenderPass == nullptr || vk.cmdBindPipeline == nullptr ||
        vk.cmdPushConstants == nullptr || vk.cmdBindDescriptorSets == nullptr || vk.cmdBindVertexBuffers == nullptr || vk.cmdBindIndexBuffer == nullptr || vk.cmdDrawIndexed == nullptr ||
        vk.cmdPipelineBarrier == nullptr || vk.cmdCopyBufferToImage == nullptr ||
        vk.createSemaphore == nullptr || vk.destroySemaphore == nullptr ||
        vk.createFence == nullptr || vk.destroyFence == nullptr || vk.waitForFences == nullptr || vk.resetFences == nullptr ||
        vk.acquireNextImage == nullptr || vk.queueSubmit == nullptr || vk.queuePresent == nullptr || vk.queueWaitIdle == nullptr) {
        throw std::runtime_error("Failed to resolve Vulkan device procedures after vkCreateDevice.");
    }

    vk.getDeviceQueue(vk.device, vk.graphicsQueueFamilyIndex, 0, &vk.graphicsQueue);
    if (vk.graphicsQueue == nullptr) {
        throw std::runtime_error("Failed to retrieve a Vulkan graphics queue for the selected device.");
    }

    vk.deviceReady = true;
}

inline VkExtent2D client_extent() {
    auto& vk = state();
    RECT rect{};
    GetClientRect(vk.window, &rect);
    return VkExtent2D{
        static_cast<std::uint32_t>(std::max<LONG>(1, rect.right - rect.left)),
        static_cast<std::uint32_t>(std::max<LONG>(1, rect.bottom - rect.top))
    };
}

inline std::uint32_t find_memory_type(std::uint32_t typeBits, VkMemoryPropertyFlags requiredProperties) {
    auto& vk = state();
    if (vk.getPhysicalDeviceMemoryProperties == nullptr || vk.physicalDevice == nullptr) {
        throw std::runtime_error("Vulkan memory properties are unavailable.");
    }

    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vk.getPhysicalDeviceMemoryProperties(vk.physicalDevice, &memoryProperties);
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        const bool supported = (typeBits & (1u << index)) != 0;
        const bool matches = (memoryProperties.memoryTypes[index].propertyFlags & requiredProperties) == requiredProperties;
        if (supported && matches) {
            return index;
        }
    }

    throw std::runtime_error("No compatible Vulkan memory type was found.");
}

inline std::wstring engine_root_directory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L".";
    }
    std::filesystem::path path(buffer);
    return path.parent_path().wstring();
}

inline std::vector<std::uint32_t> load_spirv_words(const std::filesystem::path& filePath) {
    std::ifstream stream(filePath, std::ios::binary);
    if (!stream.good()) {
        return {};
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (bytes.empty() || (bytes.size() % 4) != 0) {
        return {};
    }

    std::vector<std::uint32_t> words(bytes.size() / 4, 0);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    if (words.empty() || words[0] != 0x07230203u) {
        return {};
    }
    return words;
}

inline void append_spirv_instruction(std::vector<std::uint32_t>& words, std::uint16_t opcode, std::initializer_list<std::uint32_t> operands) {
    const std::uint32_t wordCount = static_cast<std::uint32_t>(operands.size()) + 1u;
    words.push_back((wordCount << 16) | opcode);
    words.insert(words.end(), operands.begin(), operands.end());
}

inline void append_spirv_entry_point(
    std::vector<std::uint32_t>& words,
    std::uint32_t executionModel,
    std::uint32_t entryPointId,
    const char* name,
    std::initializer_list<std::uint32_t> interfaceIds) {
    std::vector<std::uint32_t> encodedName;
    std::uint32_t currentWord = 0;
    std::uint32_t shift = 0;
    for (const unsigned char ch : std::string(name)) {
        currentWord |= static_cast<std::uint32_t>(ch) << shift;
        shift += 8;
        if (shift == 32) {
            encodedName.push_back(currentWord);
            currentWord = 0;
            shift = 0;
        }
    }
    encodedName.push_back(currentWord);

    const std::uint32_t wordCount = 1u + 2u + static_cast<std::uint32_t>(encodedName.size()) + static_cast<std::uint32_t>(interfaceIds.size());
    words.push_back((wordCount << 16) | 15u);
    words.push_back(executionModel);
    words.push_back(entryPointId);
    words.insert(words.end(), encodedName.begin(), encodedName.end());
    words.insert(words.end(), interfaceIds.begin(), interfaceIds.end());
}

inline std::vector<std::uint32_t> built_in_preview_vertex_spirv() {
    std::vector<std::uint32_t> words = {
        0x07230203u,
        0x00010000u,
        0u,
        19u,
        0u
    };

    append_spirv_instruction(words, 17u, {1u});
    append_spirv_instruction(words, 14u, {0u, 1u});
    append_spirv_entry_point(words, 0u, 11u, "main", {8u, 10u});
    append_spirv_instruction(words, 71u, {8u, 30u, 0u});
    append_spirv_instruction(words, 71u, {10u, 11u, 0u});
    append_spirv_instruction(words, 19u, {1u});
    append_spirv_instruction(words, 33u, {2u, 1u});
    append_spirv_instruction(words, 22u, {3u, 32u});
    append_spirv_instruction(words, 23u, {4u, 3u, 3u});
    append_spirv_instruction(words, 23u, {5u, 3u, 4u});
    append_spirv_instruction(words, 43u, {3u, 6u, 0x00000000u});
    append_spirv_instruction(words, 43u, {3u, 18u, 0x3f800000u});
    append_spirv_instruction(words, 32u, {7u, 1u, 4u});
    append_spirv_instruction(words, 59u, {7u, 8u, 1u});
    append_spirv_instruction(words, 32u, {9u, 3u, 5u});
    append_spirv_instruction(words, 59u, {9u, 10u, 3u});
    append_spirv_instruction(words, 54u, {1u, 11u, 0u, 2u});
    append_spirv_instruction(words, 248u, {12u});
    append_spirv_instruction(words, 61u, {4u, 13u, 8u});
    append_spirv_instruction(words, 81u, {3u, 14u, 13u, 0u});
    append_spirv_instruction(words, 81u, {3u, 15u, 13u, 1u});
    append_spirv_instruction(words, 81u, {3u, 16u, 13u, 2u});
    append_spirv_instruction(words, 80u, {5u, 17u, 14u, 15u, 16u, 18u});
    append_spirv_instruction(words, 62u, {10u, 17u});
    append_spirv_instruction(words, 253u, {});
    append_spirv_instruction(words, 56u, {});
    return words;
}

inline std::vector<std::uint32_t> built_in_preview_fragment_spirv() {
    std::vector<std::uint32_t> words = {
        0x07230203u,
        0x00010000u,
        0u,
        12u,
        0u
    };

    append_spirv_instruction(words, 17u, {1u});
    append_spirv_instruction(words, 14u, {0u, 1u});
    append_spirv_entry_point(words, 4u, 10u, "main", {8u});
    append_spirv_instruction(words, 16u, {10u, 7u});
    append_spirv_instruction(words, 71u, {8u, 30u, 0u});
    append_spirv_instruction(words, 19u, {1u});
    append_spirv_instruction(words, 33u, {2u, 1u});
    append_spirv_instruction(words, 22u, {3u, 32u});
    append_spirv_instruction(words, 23u, {4u, 3u, 4u});
    append_spirv_instruction(words, 43u, {3u, 5u, 0x3f800000u});
    append_spirv_instruction(words, 43u, {3u, 6u, 0x00000000u});
    append_spirv_instruction(words, 32u, {7u, 3u, 4u});
    append_spirv_instruction(words, 59u, {7u, 8u, 3u});
    append_spirv_instruction(words, 80u, {4u, 9u, 5u, 6u, 6u, 5u});
    append_spirv_instruction(words, 54u, {1u, 10u, 0u, 2u});
    append_spirv_instruction(words, 248u, {11u});
    append_spirv_instruction(words, 62u, {8u, 9u});
    append_spirv_instruction(words, 253u, {});
    append_spirv_instruction(words, 56u, {});
    return words;
}

inline std::filesystem::path resolve_shader_asset_root() {
    const std::array<std::filesystem::path, 5> candidates = {{
        std::filesystem::path("assets") / "vulkan",
        std::filesystem::path("modules") / "vulkan",
        std::filesystem::path(engine_root_directory()) / L"assets" / L"vulkan",
        std::filesystem::path(engine_root_directory()) / L"modules" / L"vulkan",
        std::filesystem::path("build") / "vulkan"
    }};

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate / "preview.vert.spv") && std::filesystem::exists(candidate / "preview.frag.spv")) {
            return candidate;
        }
    }

    return {};
}

inline void create_pipeline_scaffold() {
    auto& vk = state();
    if (vk.graphicsPipelineReady) {
        trace_step("pipeline: already ready");
        return;
    }
    if (vk.device == nullptr || vk.createShaderModule == nullptr || vk.createPipelineLayout == nullptr ||
        vk.createGraphicsPipelines == nullptr || vk.renderPass == nullptr) {
        return;
    }

    std::vector<std::uint32_t> vertWords;
    std::vector<std::uint32_t> fragWords;
    const std::filesystem::path assetRoot = resolve_shader_asset_root();
    if (!assetRoot.empty()) {
        vertWords = load_spirv_words(assetRoot / "preview.vert.spv");
        fragWords = load_spirv_words(assetRoot / "preview.frag.spv");
    }
    if (vertWords.empty() || fragWords.empty()) {
        trace_step("pipeline: external shader assets are unavailable, skipping graphics pipeline");
        vk.shaderAssetsReady = false;
        vk.shaderAssetRoot.clear();
        return;
    }
    trace_step("pipeline: using external shader assets");
    vk.shaderAssetsReady = true;
    vk.shaderAssetRoot = assetRoot.wstring();

    auto create_shader_module = [&vk](const std::vector<std::uint32_t>& words, VkShaderModule& outModule) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = words.size() * sizeof(std::uint32_t);
        createInfo.pCode = words.data();
        trace_step(std::string("pipeline: create shader module words=") + std::to_string(words.size()));
        if (vk.createShaderModule(vk.device, &createInfo, nullptr, &outModule) != VK_SUCCESS || outModule == nullptr) {
            throw std::runtime_error("vkCreateShaderModule failed for Vulkan preview shader.");
        }
    };

    create_shader_module(vertWords, vk.demoVertexShader);
    trace_step("pipeline: vertex shader module ready");
    create_shader_module(fragWords, vk.demoFragmentShader);
    trace_step("pipeline: fragment shader module ready");
    vk.shaderModulesReady = vk.demoVertexShader != nullptr && vk.demoFragmentShader != nullptr;
    trace_step(std::string("pipeline: shader modules ready=") + (vk.shaderModulesReady ? "true" : "false"));

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vk.sceneDescriptorSetLayout != nullptr) {
        layoutInfo.setLayoutCount = 1u;
        layoutInfo.pSetLayouts = &vk.sceneDescriptorSetLayout;
    }
    const VkPushConstantRange pushConstantRange{
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0u,
        static_cast<std::uint32_t>(sizeof(ScenePushConstants))
    };
    layoutInfo.pushConstantRangeCount = 1u;
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    trace_step("pipeline: create pipeline layout");
    if (vk.createPipelineLayout(vk.device, &layoutInfo, nullptr, &vk.demoPipelineLayout) == VK_SUCCESS && vk.demoPipelineLayout != nullptr) {
        vk.pipelineLayoutReady = true;
    }
    trace_step(std::string("pipeline: layout ready=") + (vk.pipelineLayoutReady ? "true" : "false"));

    if (!vk.pipelineLayoutReady) {
        return;
    }

    const VkPipelineShaderStageCreateInfo stages[] = {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vk.demoVertexShader,
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            vk.demoFragmentShader,
            "main",
            nullptr
        }
    };

    const VkVertexInputBindingDescription bindingDescription{
        0u,
        17u * sizeof(float),
        VK_VERTEX_INPUT_RATE_VERTEX
    };
    const std::array<VkVertexInputAttributeDescription, 6> attributeDescriptions = {{
        {0u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 0u},
        {1u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 3u * sizeof(float)},
        {2u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 6u * sizeof(float)},
        {3u, 0u, VK_FORMAT_R32G32_SFLOAT, 9u * sizeof(float)},
        {4u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 11u * sizeof(float)},
        {5u, 0u, VK_FORMAT_R32G32B32_SFLOAT, 14u * sizeof(float)}
    }};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    const VkViewport viewport{
        0.0f,
        0.0f,
        static_cast<float>(vk.swapchainExtent.width),
        static_cast<float>(vk.swapchainExtent.height),
        0.0f,
        1.0f
    };
    const VkRect2D scissor{{0, 0}, vk.swapchainExtent};

    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.pViewports = &viewport;
    viewportInfo.scissorCount = 1;
    viewportInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterInfo{};
    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.depthClampEnable = VK_FALSE;
    rasterInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterInfo.cullMode = VK_CULL_MODE_NONE;
    rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterInfo.depthBiasEnable = VK_FALSE;
    rasterInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleInfo{};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleInfo.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.stencilTestEnable = VK_FALSE;
    depthStencilInfo.minDepthBounds = 0.0f;
    depthStencilInfo.maxDepthBounds = 1.0f;

    const VkPipelineColorBlendAttachmentState colorBlendAttachment{
        VK_FALSE,
        0u, 0u, 0u,
        0u, 0u, 0u,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pDepthStencilState = &depthStencilInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.layout = vk.demoPipelineLayout;
    pipelineInfo.renderPass = vk.renderPass;
    pipelineInfo.subpass = 0;

    trace_step("pipeline: create graphics pipeline");
    if (vk.createGraphicsPipelines(vk.device, nullptr, 1, &pipelineInfo, nullptr, &vk.demoGraphicsPipeline) == VK_SUCCESS &&
        vk.demoGraphicsPipeline != nullptr) {
        vk.graphicsPipelineReady = true;
    }
    trace_step(std::string("pipeline: graphics ready=") + (vk.graphicsPipelineReady ? "true" : "false"));
}

inline void create_depth_resources() {
    auto& vk = state();
    if (vk.depthReady) {
        trace_step("depth: already ready");
        return;
    }
    if (vk.device == nullptr || !vk.swapchainReady || vk.createImage == nullptr || vk.allocateMemory == nullptr ||
        vk.bindImageMemory == nullptr || vk.createImageView == nullptr || vk.getImageMemoryRequirements == nullptr) {
        throw std::runtime_error("Vulkan depth resource setup requires image/memory procedures and a ready swapchain.");
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.extent = VkExtent3D{vk.swapchainExtent.width, vk.swapchainExtent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    trace_step("depth: create image");
    if (vk.createImage(vk.device, &imageInfo, nullptr, &vk.depthImage) != VK_SUCCESS || vk.depthImage == nullptr) {
        throw std::runtime_error("vkCreateImage failed for Vulkan depth attachment.");
    }

    VkMemoryRequirements memoryRequirements{};
    trace_step("depth: query memory requirements");
    vk.getImageMemoryRequirements(vk.device, vk.depthImage, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    trace_step("depth: allocate memory");
    if (vk.allocateMemory(vk.device, &allocateInfo, nullptr, &vk.depthImageMemory) != VK_SUCCESS || vk.depthImageMemory == nullptr) {
        throw std::runtime_error("vkAllocateMemory failed for Vulkan depth attachment.");
    }

    trace_step("depth: bind image memory");
    if (vk.bindImageMemory(vk.device, vk.depthImage, vk.depthImageMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("vkBindImageMemory failed for Vulkan depth attachment.");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vk.depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    viewInfo.subresourceRange = {
        VK_IMAGE_ASPECT_DEPTH_BIT,
        0,
        1,
        0,
        1
    };

    trace_step("depth: create image view");
    if (vk.createImageView(vk.device, &viewInfo, nullptr, &vk.depthImageView) != VK_SUCCESS || vk.depthImageView == nullptr) {
        throw std::runtime_error("vkCreateImageView failed for Vulkan depth attachment.");
    }

    vk.depthReady = true;
    trace_step("depth: ready");
}

inline void release_geometry_buffers() {
    auto& vk = state();
    if (vk.device == nullptr) {
        return;
    }
    if (vk.demoVertexBuffer != nullptr) {
        vk.destroyBuffer(vk.device, vk.demoVertexBuffer, nullptr);
        vk.demoVertexBuffer = nullptr;
    }
    if (vk.demoVertexMemory != nullptr) {
        vk.freeMemory(vk.device, vk.demoVertexMemory, nullptr);
        vk.demoVertexMemory = nullptr;
    }
    if (vk.demoIndexBuffer != nullptr) {
        vk.destroyBuffer(vk.device, vk.demoIndexBuffer, nullptr);
        vk.demoIndexBuffer = nullptr;
    }
    if (vk.demoIndexMemory != nullptr) {
        vk.freeMemory(vk.device, vk.demoIndexMemory, nullptr);
        vk.demoIndexMemory = nullptr;
    }
    vk.demoVertexBufferBytes = 0;
    vk.demoIndexBufferBytes = 0;
    vk.demoVertexBufferCapacity = 0;
    vk.demoIndexBufferCapacity = 0;
    vk.sceneIndexCount = 0;
    vk.geometryBuffersReady = false;
}

inline void create_host_visible_geometry_buffer(std::uint64_t size, VkBufferUsageFlags usage, VkBuffer& outBuffer, VkDeviceMemory& outMemory) {
    auto& vk = state();
    trace_step(std::string("geometry: create buffer size=") + std::to_string(size));
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vk.createBuffer(vk.device, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS || outBuffer == nullptr) {
        throw std::runtime_error("vkCreateBuffer failed for Vulkan scene geometry buffer.");
    }

    VkMemoryRequirements requirements{};
    vk.getBufferMemoryRequirements(vk.device, outBuffer, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_type(
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (vk.allocateMemory(vk.device, &allocateInfo, nullptr, &outMemory) != VK_SUCCESS || outMemory == nullptr) {
        throw std::runtime_error("vkAllocateMemory failed for Vulkan scene geometry buffer.");
    }

    if (vk.bindBufferMemory(vk.device, outBuffer, outMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("vkBindBufferMemory failed for Vulkan scene geometry buffer.");
    }
}

inline void ensure_geometry_capacity(std::uint64_t vertexBytes, std::uint64_t indexBytes) {
    auto& vk = state();
    if (vk.device == nullptr || vk.createBuffer == nullptr || vk.allocateMemory == nullptr ||
        vk.bindBufferMemory == nullptr || vk.getBufferMemoryRequirements == nullptr) {
        throw std::runtime_error("Vulkan geometry buffer setup requires buffer/memory procedures.");
    }

    const std::uint64_t safeVertexBytes = std::max<std::uint64_t>(vertexBytes, sizeof(SceneVertex) * 3u);
    const std::uint64_t safeIndexBytes = std::max<std::uint64_t>(indexBytes, sizeof(std::uint16_t) * 3u);
    if (vk.demoVertexBuffer != nullptr && vk.demoIndexBuffer != nullptr &&
        vk.demoVertexBufferCapacity >= safeVertexBytes &&
        vk.demoIndexBufferCapacity >= safeIndexBytes) {
        vk.geometryBuffersReady = true;
        return;
    }

    release_geometry_buffers();
    create_host_visible_geometry_buffer(safeVertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vk.demoVertexBuffer, vk.demoVertexMemory);
    create_host_visible_geometry_buffer(safeIndexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, vk.demoIndexBuffer, vk.demoIndexMemory);
    vk.demoVertexBufferCapacity = safeVertexBytes;
    vk.demoIndexBufferCapacity = safeIndexBytes;
    vk.geometryBuffersReady = true;
}

inline void create_demo_geometry_buffers() {
    ensure_geometry_capacity(sizeof(SceneVertex) * 3u, sizeof(std::uint16_t) * 3u);
    trace_step("geometry: ready");
}

inline void upload_scene_geometry() {
    auto& vk = state();
    auto& vertices = frame_vertices();
    auto& indices = frame_indices();
    if (vertices.empty() || indices.empty()) {
        vk.sceneIndexCount = 0;
        vk.demoVertexBufferBytes = 0;
        vk.demoIndexBufferBytes = 0;
        return;
    }

    const std::uint64_t vertexBytes = static_cast<std::uint64_t>(vertices.size() * sizeof(SceneVertex));
    const std::uint64_t indexBytes = static_cast<std::uint64_t>(indices.size() * sizeof(std::uint16_t));
    ensure_geometry_capacity(vertexBytes, indexBytes);

    void* mapped = nullptr;
    if (vk.mapMemory(vk.device, vk.demoVertexMemory, 0, vertexBytes, 0, &mapped) != VK_SUCCESS || mapped == nullptr) {
        throw std::runtime_error("vkMapMemory failed for Vulkan scene vertex buffer.");
    }
    std::memcpy(mapped, vertices.data(), static_cast<size_t>(vertexBytes));
    vk.unmapMemory(vk.device, vk.demoVertexMemory);

    mapped = nullptr;
    if (vk.mapMemory(vk.device, vk.demoIndexMemory, 0, indexBytes, 0, &mapped) != VK_SUCCESS || mapped == nullptr) {
        throw std::runtime_error("vkMapMemory failed for Vulkan scene index buffer.");
    }
    std::memcpy(mapped, indices.data(), static_cast<size_t>(indexBytes));
    vk.unmapMemory(vk.device, vk.demoIndexMemory);

    vk.demoVertexBufferBytes = vertexBytes;
    vk.demoIndexBufferBytes = indexBytes;
    vk.sceneIndexCount = static_cast<std::uint32_t>(indices.size());
}

inline VkCommandBuffer begin_single_use_command_buffer() {
    auto& vk = state();
    VkCommandBuffer commandBuffer = nullptr;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vk.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vk.allocateCommandBuffers(vk.device, &allocInfo, &commandBuffer) != VK_SUCCESS || commandBuffer == nullptr) {
        throw std::runtime_error("vkAllocateCommandBuffers failed for Vulkan upload command buffer.");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vk.beginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vk.freeCommandBuffers(vk.device, vk.commandPool, 1, &commandBuffer);
        throw std::runtime_error("vkBeginCommandBuffer failed for Vulkan upload command buffer.");
    }

    return commandBuffer;
}

inline void end_single_use_command_buffer(VkCommandBuffer commandBuffer) {
    auto& vk = state();
    if (commandBuffer == nullptr) {
        return;
    }
    if (vk.endCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vk.freeCommandBuffers(vk.device, vk.commandPool, 1, &commandBuffer);
        throw std::runtime_error("vkEndCommandBuffer failed for Vulkan upload command buffer.");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    if (vk.queueSubmit(vk.graphicsQueue, 1, &submitInfo, nullptr) != VK_SUCCESS) {
        vk.freeCommandBuffers(vk.device, vk.commandPool, 1, &commandBuffer);
        throw std::runtime_error("vkQueueSubmit failed for Vulkan upload command buffer.");
    }
    if (vk.queueWaitIdle(vk.graphicsQueue) != VK_SUCCESS) {
        vk.freeCommandBuffers(vk.device, vk.commandPool, 1, &commandBuffer);
        throw std::runtime_error("vkQueueWaitIdle failed for Vulkan upload command buffer.");
    }
    vk.freeCommandBuffers(vk.device, vk.commandPool, 1, &commandBuffer);
}

inline void transition_image_layout(
    VkCommandBuffer commandBuffer,
    VkImage image,
    std::uint32_t oldLayout,
    std::uint32_t newLayout,
    VkFlags srcAccessMask,
    VkFlags dstAccessMask,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage) {
    auto& vk = state();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_INVALID_QUEUE_FAMILY;
    barrier.dstQueueFamilyIndex = VK_INVALID_QUEUE_FAMILY;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vk.cmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

inline void create_texture_resources() {
    auto& vk = state();
    if (vk.textureImageReady && vk.descriptorSetReady && vk.textureSamplerReady) {
        return;
    }
    if (vk.device == nullptr || vk.commandPool == nullptr || vk.graphicsQueue == nullptr) {
        throw std::runtime_error("Vulkan texture resources need a device, command pool and queue.");
    }

    if (vk.sceneDescriptorSetLayout == nullptr) {
        const VkDescriptorSetLayoutBinding binding{
            0u,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1u,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
        };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1u;
        layoutInfo.pBindings = &binding;
        if (vk.createDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &vk.sceneDescriptorSetLayout) != VK_SUCCESS ||
            vk.sceneDescriptorSetLayout == nullptr) {
            throw std::runtime_error("vkCreateDescriptorSetLayout failed for Vulkan texture resources.");
        }
    }

    if (vk.sceneTextureSampler == nullptr) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = 0u;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        if (vk.createSampler(vk.device, &samplerInfo, nullptr, &vk.sceneTextureSampler) != VK_SUCCESS ||
            vk.sceneTextureSampler == nullptr) {
            throw std::runtime_error("vkCreateSampler failed for Vulkan texture resources.");
        }
        vk.textureSamplerReady = true;
    }

    constexpr std::uint32_t textureWidth = 64u;
    constexpr std::uint32_t textureHeight = 64u;
    constexpr std::uint64_t textureBytes = textureWidth * textureHeight * 4u;

    if (vk.sceneTextureImage == nullptr) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {textureWidth, textureHeight, 1u};
        imageInfo.mipLevels = 1u;
        imageInfo.arrayLayers = 1u;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vk.createImage(vk.device, &imageInfo, nullptr, &vk.sceneTextureImage) != VK_SUCCESS || vk.sceneTextureImage == nullptr) {
            throw std::runtime_error("vkCreateImage failed for Vulkan texture resources.");
        }
        vk.sceneTextureWidth = static_cast<int>(textureWidth);
        vk.sceneTextureHeight = static_cast<int>(textureHeight);

        VkMemoryRequirements memoryRequirements{};
        vk.getImageMemoryRequirements(vk.device, vk.sceneTextureImage, &memoryRequirements);
        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = find_memory_type(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vk.allocateMemory(vk.device, &allocateInfo, nullptr, &vk.sceneTextureMemory) != VK_SUCCESS || vk.sceneTextureMemory == nullptr) {
            throw std::runtime_error("vkAllocateMemory failed for Vulkan texture resources.");
        }
        if (vk.bindImageMemory(vk.device, vk.sceneTextureImage, vk.sceneTextureMemory, 0u) != VK_SUCCESS) {
            throw std::runtime_error("vkBindImageMemory failed for Vulkan texture resources.");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vk.sceneTextureImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vk.createImageView(vk.device, &viewInfo, nullptr, &vk.sceneTextureView) != VK_SUCCESS || vk.sceneTextureView == nullptr) {
            throw std::runtime_error("vkCreateImageView failed for Vulkan texture resources.");
        }

        VkBuffer stagingBuffer = nullptr;
        VkDeviceMemory stagingMemory = nullptr;
        create_host_visible_geometry_buffer(textureBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory);

        std::vector<std::uint8_t> pixels(static_cast<size_t>(textureBytes), 255u);
        for (std::uint32_t y = 0; y < textureHeight; ++y) {
            for (std::uint32_t x = 0; x < textureWidth; ++x) {
                const bool checker = ((x / 8u) + (y / 8u)) % 2u == 0u;
                const bool stripe = (x / 4u) % 2u == 0u;
                const std::uint8_t warm = checker ? 228u : 92u;
                const std::uint8_t cool = stripe ? 238u : 134u;
                const size_t pixelIndex = static_cast<size_t>((y * textureWidth + x) * 4u);
                pixels[pixelIndex + 0] = static_cast<std::uint8_t>((warm + cool) / 2u);
                pixels[pixelIndex + 1] = static_cast<std::uint8_t>(checker ? 186u : 118u);
                pixels[pixelIndex + 2] = static_cast<std::uint8_t>(stripe ? 250u : 168u);
                pixels[pixelIndex + 3] = 255u;
            }
        }

        void* mapped = nullptr;
        if (vk.mapMemory(vk.device, stagingMemory, 0u, textureBytes, 0u, &mapped) != VK_SUCCESS || mapped == nullptr) {
            throw std::runtime_error("vkMapMemory failed for Vulkan staging texture.");
        }
        std::memcpy(mapped, pixels.data(), pixels.size());
        vk.unmapMemory(vk.device, stagingMemory);

        VkCommandBuffer commandBuffer = begin_single_use_command_buffer();
        transition_image_layout(
            commandBuffer,
            vk.sceneTextureImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0u,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {textureWidth, textureHeight, 1u};
        vk.cmdCopyBufferToImage(commandBuffer, stagingBuffer, vk.sceneTextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);

        transition_image_layout(
            commandBuffer,
            vk.sceneTextureImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        end_single_use_command_buffer(commandBuffer);

        if (vk.destroyBuffer != nullptr && stagingBuffer != nullptr) {
            vk.destroyBuffer(vk.device, stagingBuffer, nullptr);
        }
        if (vk.freeMemory != nullptr && stagingMemory != nullptr) {
            vk.freeMemory(vk.device, stagingMemory, nullptr);
        }
        vk.textureImageReady = true;
    }

    if (vk.sceneDescriptorPool == nullptr) {
        const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1u;
        poolInfo.poolSizeCount = 1u;
        poolInfo.pPoolSizes = &poolSize;
        if (vk.createDescriptorPool(vk.device, &poolInfo, nullptr, &vk.sceneDescriptorPool) != VK_SUCCESS || vk.sceneDescriptorPool == nullptr) {
            throw std::runtime_error("vkCreateDescriptorPool failed for Vulkan texture resources.");
        }
    }

    if (vk.sceneDescriptorSet == nullptr) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = vk.sceneDescriptorPool;
        allocInfo.descriptorSetCount = 1u;
        allocInfo.pSetLayouts = &vk.sceneDescriptorSetLayout;
        if (vk.allocateDescriptorSets(vk.device, &allocInfo, &vk.sceneDescriptorSet) != VK_SUCCESS || vk.sceneDescriptorSet == nullptr) {
            throw std::runtime_error("vkAllocateDescriptorSets failed for Vulkan texture resources.");
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = vk.sceneTextureSampler;
        imageInfo.imageView = vk.sceneTextureView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = vk.sceneDescriptorSet;
        write.dstBinding = 0u;
        write.descriptorCount = 1u;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vk.updateDescriptorSets(vk.device, 1u, &write, 0u, nullptr);
        vk.descriptorSetReady = true;
    }
}

inline void destroy_scene_texture_image() {
    auto& vk = state();
    if (vk.destroyImageView != nullptr && vk.device != nullptr && vk.sceneTextureView != nullptr) {
        vk.destroyImageView(vk.device, vk.sceneTextureView, nullptr);
    }
    if (vk.destroyImage != nullptr && vk.device != nullptr && vk.sceneTextureImage != nullptr) {
        vk.destroyImage(vk.device, vk.sceneTextureImage, nullptr);
    }
    if (vk.freeMemory != nullptr && vk.device != nullptr && vk.sceneTextureMemory != nullptr) {
        vk.freeMemory(vk.device, vk.sceneTextureMemory, nullptr);
    }
    vk.sceneTextureView = nullptr;
    vk.sceneTextureImage = nullptr;
    vk.sceneTextureMemory = nullptr;
    vk.sceneTextureWidth = 0;
    vk.sceneTextureHeight = 0;
    vk.textureImageReady = false;
}

inline void write_scene_texture_descriptor() {
    auto& vk = state();
    if (vk.sceneDescriptorSet == nullptr || vk.sceneTextureSampler == nullptr || vk.sceneTextureView == nullptr) {
        return;
    }
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = vk.sceneTextureSampler;
    imageInfo.imageView = vk.sceneTextureView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = vk.sceneDescriptorSet;
    write.dstBinding = 0u;
    write.descriptorCount = 1u;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vk.updateDescriptorSets(vk.device, 1u, &write, 0u, nullptr);
    vk.descriptorSetReady = true;
}

inline void upload_scene_texture_rgba(int width, int height, const unsigned char* pixels) {
    auto& vk = state();
    if (width <= 0 || height <= 0 || pixels == nullptr) {
        return;
    }
    create_texture_resources();

    const std::uint64_t textureBytes = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4u;
    if (vk.sceneTextureImage != nullptr && (vk.sceneTextureWidth != width || vk.sceneTextureHeight != height)) {
        destroy_scene_texture_image();
    }

    if (vk.sceneTextureImage == nullptr) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1u};
        imageInfo.mipLevels = 1u;
        imageInfo.arrayLayers = 1u;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vk.createImage(vk.device, &imageInfo, nullptr, &vk.sceneTextureImage) != VK_SUCCESS || vk.sceneTextureImage == nullptr) {
            throw std::runtime_error("vkCreateImage failed for uploaded Vulkan texture.");
        }
        vk.sceneTextureWidth = width;
        vk.sceneTextureHeight = height;

        VkMemoryRequirements memoryRequirements{};
        vk.getImageMemoryRequirements(vk.device, vk.sceneTextureImage, &memoryRequirements);
        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = find_memory_type(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vk.allocateMemory(vk.device, &allocateInfo, nullptr, &vk.sceneTextureMemory) != VK_SUCCESS || vk.sceneTextureMemory == nullptr) {
            throw std::runtime_error("vkAllocateMemory failed for uploaded Vulkan texture.");
        }
        if (vk.bindImageMemory(vk.device, vk.sceneTextureImage, vk.sceneTextureMemory, 0u) != VK_SUCCESS) {
            throw std::runtime_error("vkBindImageMemory failed for uploaded Vulkan texture.");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vk.sceneTextureImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vk.createImageView(vk.device, &viewInfo, nullptr, &vk.sceneTextureView) != VK_SUCCESS || vk.sceneTextureView == nullptr) {
            throw std::runtime_error("vkCreateImageView failed for uploaded Vulkan texture.");
        }
    }

    VkBuffer stagingBuffer = nullptr;
    VkDeviceMemory stagingMemory = nullptr;
    create_host_visible_geometry_buffer(textureBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory);
    void* mapped = nullptr;
    if (vk.mapMemory(vk.device, stagingMemory, 0u, textureBytes, 0u, &mapped) != VK_SUCCESS || mapped == nullptr) {
        throw std::runtime_error("vkMapMemory failed for uploaded Vulkan texture.");
    }
    std::memcpy(mapped, pixels, static_cast<size_t>(textureBytes));
    vk.unmapMemory(vk.device, stagingMemory);

    const std::uint32_t oldLayout = (vk.sceneTextureWidth == width && vk.sceneTextureHeight == height && vk.textureImageReady)
        ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        : VK_IMAGE_LAYOUT_UNDEFINED;
    VkCommandBuffer commandBuffer = begin_single_use_command_buffer();
    transition_image_layout(
        commandBuffer,
        vk.sceneTextureImage,
        oldLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0u : VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1u};
    vk.cmdCopyBufferToImage(commandBuffer, stagingBuffer, vk.sceneTextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);

    transition_image_layout(
        commandBuffer,
        vk.sceneTextureImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    end_single_use_command_buffer(commandBuffer);

    if (vk.destroyBuffer != nullptr && stagingBuffer != nullptr) {
        vk.destroyBuffer(vk.device, stagingBuffer, nullptr);
    }
    if (vk.freeMemory != nullptr && stagingMemory != nullptr) {
        vk.freeMemory(vk.device, stagingMemory, nullptr);
    }

    vk.textureImageReady = true;
    write_scene_texture_descriptor();
}

inline void create_swapchain() {
    auto& vk = state();
    if (vk.swapchain != nullptr) {
        return;
    }
    if (vk.device == nullptr || vk.surface == nullptr || vk.physicalDevice == nullptr) {
        throw std::runtime_error("Vulkan swapchain creation needs device, surface and physical device.");
    }

    VkSurfaceCapabilitiesKHR capabilities{};
    if (vk.getSurfaceCapabilities(vk.physicalDevice, vk.surface, &capabilities) != VK_SUCCESS) {
        throw std::runtime_error("Failed to query Vulkan surface capabilities.");
    }

    std::uint32_t formatCount = 0;
    if (vk.getSurfaceFormats(vk.physicalDevice, vk.surface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) {
        throw std::runtime_error("Failed to query Vulkan surface formats.");
    }
    std::vector<VkSurfaceFormatKHR> formats(static_cast<size_t>(formatCount));
    if (vk.getSurfaceFormats(vk.physicalDevice, vk.surface, &formatCount, formats.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to enumerate Vulkan surface formats.");
    }

    std::uint32_t presentModeCount = 0;
    if (vk.getPresentModes(vk.physicalDevice, vk.surface, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0) {
        throw std::runtime_error("Failed to query Vulkan present modes.");
    }
    std::vector<VkPresentModeKHR> presentModes(static_cast<size_t>(presentModeCount));
    if (vk.getPresentModes(vk.physicalDevice, vk.surface, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to enumerate Vulkan present modes.");
    }

    VkSurfaceFormatKHR chosenFormat = formats.front();
    for (const auto& format : formats) {
        chosenFormat = format;
        if (format.format != 0) {
            break;
        }
    }

    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!rte::backendState.vsyncEnabled) {
        for (const auto& mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                chosenPresentMode = mode;
                break;
            }
        }
    }

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == 0xffffffffu || extent.height == 0xffffffffu) {
        extent = client_extent();
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vk.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = chosenFormat.format;
    createInfo.imageColorSpace = chosenFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
        : capabilities.currentTransform;
    createInfo.compositeAlpha = (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0
        ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        : capabilities.supportedCompositeAlpha;
    createInfo.presentMode = chosenPresentMode;
    createInfo.clipped = 1;

    if (vk.createSwapchain(vk.device, &createInfo, nullptr, &vk.swapchain) != VK_SUCCESS || vk.swapchain == nullptr) {
        throw std::runtime_error("vkCreateSwapchainKHR failed for the RayQuiro Vulkan backend.");
    }

    vk.swapchainFormat = chosenFormat.format;
    vk.swapchainExtent = extent;

    std::uint32_t actualImageCount = 0;
    if (vk.getSwapchainImages(vk.device, vk.swapchain, &actualImageCount, nullptr) != VK_SUCCESS || actualImageCount == 0) {
        throw std::runtime_error("Failed to query Vulkan swapchain images.");
    }
    vk.swapchainImages.resize(actualImageCount);
    if (vk.getSwapchainImages(vk.device, vk.swapchain, &actualImageCount, vk.swapchainImages.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to enumerate Vulkan swapchain images.");
    }

    vk.swapchainImageViews.clear();
    vk.swapchainImageViews.reserve(actualImageCount);
    for (VkImage image : vk.swapchainImages) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = vk.swapchainFormat;
        viewInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        viewInfo.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            1,
            0,
            1
        };

        VkImageView imageView = nullptr;
        if (vk.createImageView(vk.device, &viewInfo, nullptr, &imageView) != VK_SUCCESS || imageView == nullptr) {
            throw std::runtime_error("vkCreateImageView failed for the RayQuiro Vulkan swapchain.");
        }
        vk.swapchainImageViews.push_back(imageView);
    }

    vk.swapchainReady = true;
}

inline void create_command_infra() {
    auto& vk = state();
    if (vk.commandPool != nullptr) {
        return;
    }
    if (vk.device == nullptr || vk.graphicsQueueFamilyIndex == VK_INVALID_QUEUE_FAMILY || !vk.swapchainReady) {
        throw std::runtime_error("Vulkan command setup needs device, queue family and swapchain.");
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = vk.graphicsQueueFamilyIndex;

    if (vk.createCommandPool(vk.device, &poolInfo, nullptr, &vk.commandPool) != VK_SUCCESS || vk.commandPool == nullptr) {
        throw std::runtime_error("vkCreateCommandPool failed for the RayQuiro Vulkan backend.");
    }
    vk.commandPoolReady = true;

    if (!vk.swapchainImages.empty()) {
        vk.commandBuffers.resize(vk.swapchainImages.size(), nullptr);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vk.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<std::uint32_t>(vk.commandBuffers.size());

        if (vk.allocateCommandBuffers(vk.device, &allocInfo, vk.commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("vkAllocateCommandBuffers failed for the RayQuiro Vulkan backend.");
        }

        vk.commandBuffersReady = true;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vk.createSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.imageAvailableSemaphore) != VK_SUCCESS || vk.imageAvailableSemaphore == nullptr) {
        throw std::runtime_error("vkCreateSemaphore failed for imageAvailableSemaphore.");
    }
    if (vk.createSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.renderFinishedSemaphore) != VK_SUCCESS || vk.renderFinishedSemaphore == nullptr) {
        throw std::runtime_error("vkCreateSemaphore failed for renderFinishedSemaphore.");
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vk.createFence(vk.device, &fenceInfo, nullptr, &vk.inFlightFence) != VK_SUCCESS || vk.inFlightFence == nullptr) {
        throw std::runtime_error("vkCreateFence failed for the RayQuiro Vulkan backend.");
    }

    vk.syncReady = true;
    vk.framePathReady = vk.commandPoolReady && vk.commandBuffersReady && vk.syncReady &&
        vk.acquireNextImage != nullptr && vk.queueSubmit != nullptr && vk.queuePresent != nullptr;
}

inline bool has_valid_client_extent() {
    const VkExtent2D extent = client_extent();
    return extent.width > 0 && extent.height > 0;
}

inline void destroy_swapchain_dependent_resources() {
    auto& vk = state();
    if (vk.device == nullptr) {
        return;
    }

    if (vk.syncReady && vk.inFlightFence != nullptr && vk.waitForFences != nullptr) {
        vk.waitForFences(vk.device, 1, &vk.inFlightFence, 1, ~static_cast<std::uint64_t>(0));
    }
    if (vk.queueWaitIdle != nullptr && vk.graphicsQueue != nullptr) {
        vk.queueWaitIdle(vk.graphicsQueue);
    }

    if (vk.destroyFramebuffer != nullptr) {
        for (VkFramebuffer framebuffer : vk.swapchainFramebuffers) {
            if (framebuffer != nullptr) {
                vk.destroyFramebuffer(vk.device, framebuffer, nullptr);
            }
        }
    }
    vk.swapchainFramebuffers.clear();
    vk.framebuffersReady = false;

    if (vk.destroyPipeline != nullptr && vk.demoGraphicsPipeline != nullptr) {
        vk.destroyPipeline(vk.device, vk.demoGraphicsPipeline, nullptr);
    }
    if (vk.destroyPipelineLayout != nullptr && vk.demoPipelineLayout != nullptr) {
        vk.destroyPipelineLayout(vk.device, vk.demoPipelineLayout, nullptr);
    }
    if (vk.destroyShaderModule != nullptr) {
        if (vk.demoVertexShader != nullptr) {
            vk.destroyShaderModule(vk.device, vk.demoVertexShader, nullptr);
        }
        if (vk.demoFragmentShader != nullptr) {
            vk.destroyShaderModule(vk.device, vk.demoFragmentShader, nullptr);
        }
    }
    vk.demoGraphicsPipeline = nullptr;
    vk.demoPipelineLayout = nullptr;
    vk.demoVertexShader = nullptr;
    vk.demoFragmentShader = nullptr;
    vk.shaderModulesReady = false;
    vk.pipelineLayoutReady = false;
    vk.graphicsPipelineReady = false;

    if (vk.destroyRenderPass != nullptr && vk.renderPass != nullptr) {
        vk.destroyRenderPass(vk.device, vk.renderPass, nullptr);
    }
    vk.renderPass = nullptr;
    vk.renderPassReady = false;

    if (vk.destroyImageView != nullptr && vk.depthImageView != nullptr) {
        vk.destroyImageView(vk.device, vk.depthImageView, nullptr);
    }
    if (vk.destroyImage != nullptr && vk.depthImage != nullptr) {
        vk.destroyImage(vk.device, vk.depthImage, nullptr);
    }
    if (vk.freeMemory != nullptr && vk.depthImageMemory != nullptr) {
        vk.freeMemory(vk.device, vk.depthImageMemory, nullptr);
    }
    vk.depthImageView = nullptr;
    vk.depthImage = nullptr;
    vk.depthImageMemory = nullptr;
    vk.depthReady = false;

    if (vk.freeCommandBuffers != nullptr && vk.commandPool != nullptr && !vk.commandBuffers.empty()) {
        vk.freeCommandBuffers(vk.device, vk.commandPool, static_cast<std::uint32_t>(vk.commandBuffers.size()), vk.commandBuffers.data());
    }
    vk.commandBuffers.clear();
    vk.commandBuffersReady = false;

    if (vk.destroyFence != nullptr && vk.inFlightFence != nullptr) {
        vk.destroyFence(vk.device, vk.inFlightFence, nullptr);
    }
    if (vk.destroySemaphore != nullptr) {
        if (vk.imageAvailableSemaphore != nullptr) {
            vk.destroySemaphore(vk.device, vk.imageAvailableSemaphore, nullptr);
        }
        if (vk.renderFinishedSemaphore != nullptr) {
            vk.destroySemaphore(vk.device, vk.renderFinishedSemaphore, nullptr);
        }
    }
    vk.inFlightFence = nullptr;
    vk.imageAvailableSemaphore = nullptr;
    vk.renderFinishedSemaphore = nullptr;
    vk.syncReady = false;
    vk.framePathReady = false;
    vk.frameAcquired = false;

    if (vk.destroyCommandPool != nullptr && vk.commandPool != nullptr) {
        vk.destroyCommandPool(vk.device, vk.commandPool, nullptr);
    }
    vk.commandPool = nullptr;
    vk.commandPoolReady = false;

    if (vk.destroyImageView != nullptr) {
        for (VkImageView imageView : vk.swapchainImageViews) {
            if (imageView != nullptr) {
                vk.destroyImageView(vk.device, imageView, nullptr);
            }
        }
    }
    vk.swapchainImageViews.clear();
    vk.swapchainImages.clear();

    if (vk.destroySwapchain != nullptr && vk.swapchain != nullptr) {
        vk.destroySwapchain(vk.device, vk.swapchain, nullptr);
    }
    vk.swapchain = nullptr;
    vk.swapchainFormat = 0;
    vk.swapchainExtent = {0, 0};
    vk.swapchainReady = false;
    vk.currentImageIndex = 0;
}

inline void recreate_swapchain_dependent_resources() {
    auto& vk = state();
    if (vk.device == nullptr || vk.window == nullptr || !has_valid_client_extent()) {
        return;
    }
    destroy_swapchain_dependent_resources();
    create_swapchain();
    create_depth_resources();
    create_render_targets();
    create_command_infra();
    create_pipeline_scaffold();
    ensure_backbuffer();
    vk.resizePending = false;
}

inline void create_render_targets() {
    auto& vk = state();
    if (vk.renderPass != nullptr) {
        return;
    }
    if (vk.device == nullptr || !vk.swapchainReady || vk.swapchainImageViews.empty()) {
        throw std::runtime_error("Vulkan render target setup needs device, swapchain and image views.");
    }

    if (!vk.depthReady || vk.depthImageView == nullptr) {
        throw std::runtime_error("Vulkan depth resources were not prepared before render target creation.");
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = vk.swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    const std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vk.createRenderPass(vk.device, &renderPassInfo, nullptr, &vk.renderPass) != VK_SUCCESS || vk.renderPass == nullptr) {
        throw std::runtime_error("vkCreateRenderPass failed for the RayQuiro Vulkan backend.");
    }
    vk.renderPassReady = true;

    vk.swapchainFramebuffers.clear();
    vk.swapchainFramebuffers.reserve(vk.swapchainImageViews.size());
    for (VkImageView imageView : vk.swapchainImageViews) {
        const std::array<VkImageView, 2> framebufferAttachments = {imageView, vk.depthImageView};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = vk.renderPass;
        framebufferInfo.attachmentCount = static_cast<std::uint32_t>(framebufferAttachments.size());
        framebufferInfo.pAttachments = framebufferAttachments.data();
        framebufferInfo.width = vk.swapchainExtent.width;
        framebufferInfo.height = vk.swapchainExtent.height;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer = nullptr;
        if (vk.createFramebuffer(vk.device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS || framebuffer == nullptr) {
            throw std::runtime_error("vkCreateFramebuffer failed for the RayQuiro Vulkan backend.");
        }
        vk.swapchainFramebuffers.push_back(framebuffer);
    }

    vk.framebuffersReady = !vk.swapchainFramebuffers.empty();
}

inline bool record_current_frame_commands() {
    auto& vk = state();
    if (!vk.frameAcquired || !vk.renderPassReady || !vk.framebuffersReady || vk.currentImageIndex >= vk.commandBuffers.size() ||
        vk.currentImageIndex >= vk.swapchainFramebuffers.size()) {
        return false;
    }

    VkCommandBuffer commandBuffer = vk.commandBuffers[vk.currentImageIndex];
    if (vk.resetCommandBuffer(commandBuffer, 0) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vk.beginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        return false;
    }

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color.float32[0] = static_cast<float>(vk.clearColor.r) / 255.0f;
    clearValues[0].color.float32[1] = static_cast<float>(vk.clearColor.g) / 255.0f;
    clearValues[0].color.float32[2] = static_cast<float>(vk.clearColor.b) / 255.0f;
    clearValues[0].color.float32[3] = static_cast<float>(vk.clearColor.a) / 255.0f;
    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0u;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = vk.renderPass;
    renderPassInfo.framebuffer = vk.swapchainFramebuffers[vk.currentImageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = vk.swapchainExtent;
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vk.cmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (vk.graphicsPipelineReady && vk.demoGraphicsPipeline != nullptr &&
        vk.demoVertexBuffer != nullptr && vk.demoIndexBuffer != nullptr &&
        vk.sceneIndexCount > 0) {
        const VkBuffer vertexBuffers[] = {vk.demoVertexBuffer};
        const std::uint64_t offsets[] = {0u};
        const ScenePushConstants pushConstants = make_scene_push_constants();
        vk.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.demoGraphicsPipeline);
        if (vk.descriptorSetReady && vk.sceneDescriptorSet != nullptr && vk.demoPipelineLayout != nullptr) {
            vk.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.demoPipelineLayout, 0u, 1u, &vk.sceneDescriptorSet, 0u, nullptr);
        }
        if (vk.demoPipelineLayout != nullptr) {
            vk.cmdPushConstants(
                commandBuffer,
                vk.demoPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0u,
                static_cast<std::uint32_t>(sizeof(ScenePushConstants)),
                &pushConstants);
        }
        vk.cmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vk.cmdBindIndexBuffer(commandBuffer, vk.demoIndexBuffer, 0u, VK_INDEX_TYPE_UINT16);
        vk.cmdDrawIndexed(commandBuffer, vk.sceneIndexCount, 1u, 0u, 0, 0u);
    }
    vk.cmdEndRenderPass(commandBuffer);
    if (vk.endCommandBuffer(commandBuffer) != VK_SUCCESS) {
        return false;
    }

    return true;
}

inline bool acquire_frame_image() {
    auto& vk = state();
    if (!vk.framePathReady || vk.swapchain == nullptr || vk.commandBuffers.empty()) {
        return false;
    }

    const VkResult waitResult = vk.waitForFences(vk.device, 1, &vk.inFlightFence, 1, ~static_cast<std::uint64_t>(0));
    if (waitResult != VK_SUCCESS) {
        return false;
    }

    if (vk.resetFences(vk.device, 1, &vk.inFlightFence) != VK_SUCCESS) {
        return false;
    }

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vk.acquireNextImage(
        vk.device,
        vk.swapchain,
        ~static_cast<std::uint64_t>(0),
        vk.imageAvailableSemaphore,
        nullptr,
        &imageIndex
    );
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        vk.resizePending = true;
        return false;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR) {
        vk.resizePending = true;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    if (imageIndex >= vk.commandBuffers.size()) {
        return false;
    }

    vk.currentImageIndex = imageIndex;
    vk.frameAcquired = true;
    return true;
}

inline bool submit_and_present_frame() {
    auto& vk = state();
    if (!vk.framePathReady || !vk.frameAcquired || vk.currentImageIndex >= vk.commandBuffers.size()) {
        return false;
    }

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkCommandBuffer commandBuffer = vk.commandBuffers[vk.currentImageIndex];
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vk.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &vk.renderFinishedSemaphore;

    const VkResult submitResult = vk.queueSubmit(vk.graphicsQueue, 1, &submitInfo, vk.inFlightFence);
    if (submitResult != VK_SUCCESS) {
        vk.frameAcquired = false;
        return false;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vk.renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vk.swapchain;
    presentInfo.pImageIndices = &vk.currentImageIndex;

    const VkResult presentResult = vk.queuePresent(vk.graphicsQueue, &presentInfo);
    vk.frameAcquired = false;
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        vk.resizePending = true;
        return false;
    }
    if (presentResult == VK_SUBOPTIMAL_KHR) {
        vk.resizePending = true;
    }
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    vk.presentedFrames += 1;
    return true;
}

inline void register_window_class() {
    auto& vk = state();
    if (vk.windowClass != 0) {
        return;
    }

    vk.windowInstance = GetModuleHandleW(nullptr);
    vk.className = L"RayQuiroVulkanBackendWindow";

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = window_proc;
    windowClass.hInstance = vk.windowInstance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = vk.className.c_str();

    vk.windowClass = RegisterClassExW(&windowClass);
    if (vk.windowClass == 0) {
        throw std::runtime_error(format_error("Failed to register Vulkan backend window class", GetLastError()));
    }
}

inline void create_window(int width, int height, const char* title) {
    auto& vk = state();
    if (vk.window != nullptr) {
        return;
    }

    register_window_class();
    vk.title = utf8_to_wide(title == nullptr ? "RayQuiro Vulkan" : std::string(title));
    vk.requestedWidth = std::max(1, width);
    vk.requestedHeight = std::max(1, height);

    if (vk.embeddedMode && vk.hostParentWindow != nullptr) {
        vk.window = CreateWindowExW(
            0,
            vk.className.c_str(),
            vk.title.c_str(),
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            vk.requestedX,
            vk.requestedY,
            vk.requestedWidth,
            vk.requestedHeight,
            vk.hostParentWindow,
            nullptr,
            vk.windowInstance,
            nullptr
        );
    } else {
        RECT rect{0, 0, width, height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        vk.window = CreateWindowExW(
            0,
            vk.className.c_str(),
            vk.title.c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            vk.windowInstance,
            nullptr
        );
    }

    if (vk.window == nullptr) {
        throw std::runtime_error(format_error("Failed to create the Vulkan backend window", GetLastError()));
    }

    ShowWindow(vk.window, SW_SHOW);
    UpdateWindow(vk.window);
}

inline void attach_host_window(void* parentWindow, int x, int y, int width, int height, const char* title) {
    auto& vk = state();
    vk.hostParentWindow = reinterpret_cast<HWND>(parentWindow);
    vk.embeddedMode = vk.hostParentWindow != nullptr;
    vk.requestedX = x;
    vk.requestedY = y;
    vk.requestedWidth = std::max(1, width);
    vk.requestedHeight = std::max(1, height);
    if (title != nullptr && title[0] != '\0') {
        vk.title = utf8_to_wide(std::string(title));
    }
    if (vk.window != nullptr && vk.embeddedMode) {
        MoveWindow(vk.window, vk.requestedX, vk.requestedY, vk.requestedWidth, vk.requestedHeight, TRUE);
        vk.resizePending = true;
    }
}

inline void resize_host_window(int x, int y, int width, int height) {
    auto& vk = state();
    vk.requestedX = x;
    vk.requestedY = y;
    vk.requestedWidth = std::max(1, width);
    vk.requestedHeight = std::max(1, height);
    if (vk.window != nullptr && vk.embeddedMode) {
        MoveWindow(vk.window, vk.requestedX, vk.requestedY, vk.requestedWidth, vk.requestedHeight, TRUE);
        vk.resizePending = true;
    }
}

inline void draw_clear_color() {
    auto& vk = state();
    if (vk.window == nullptr || vk.backbufferDC == nullptr) {
        return;
    }

    RECT rect{};
    GetClientRect(vk.window, &rect);
    HBRUSH brush = CreateSolidBrush(RGB(vk.clearColor.r, vk.clearColor.g, vk.clearColor.b));
    FillRect(vk.backbufferDC, &rect, brush);
    DeleteObject(brush);
}

inline void release_backbuffer() {
    auto& vk = state();
    if (vk.backbufferDC != nullptr) {
        if (vk.backbufferOldObject != nullptr) {
            SelectObject(vk.backbufferDC, vk.backbufferOldObject);
            vk.backbufferOldObject = nullptr;
        }
        if (vk.backbufferBitmap != nullptr) {
            DeleteObject(vk.backbufferBitmap);
            vk.backbufferBitmap = nullptr;
        }
        DeleteDC(vk.backbufferDC);
        vk.backbufferDC = nullptr;
    }
    vk.backbufferWidth = 0;
    vk.backbufferHeight = 0;
}

inline void ensure_backbuffer() {
    auto& vk = state();
    if (vk.window == nullptr) {
        return;
    }

    RECT rect{};
    GetClientRect(vk.window, &rect);
    const int width = std::max(1L, rect.right - rect.left);
    const int height = std::max(1L, rect.bottom - rect.top);

    if (vk.backbufferDC != nullptr && vk.backbufferWidth == width && vk.backbufferHeight == height) {
        return;
    }

    release_backbuffer();

    HDC windowDC = GetDC(vk.window);
    vk.backbufferDC = CreateCompatibleDC(windowDC);
    vk.backbufferBitmap = CreateCompatibleBitmap(windowDC, width, height);
    vk.backbufferOldObject = SelectObject(vk.backbufferDC, vk.backbufferBitmap);
    ReleaseDC(vk.window, windowDC);

    vk.backbufferWidth = width;
    vk.backbufferHeight = height;
}

inline void blit_backbuffer_to_window() {
    auto& vk = state();
    if (vk.window == nullptr || vk.backbufferDC == nullptr || vk.backbufferWidth <= 0 || vk.backbufferHeight <= 0) {
        return;
    }
    HDC windowDC = GetDC(vk.window);
    BitBlt(windowDC, 0, 0, vk.backbufferWidth, vk.backbufferHeight, vk.backbufferDC, 0, 0, SRCCOPY);
    ReleaseDC(vk.window, windowDC);
    ValidateRect(vk.window, nullptr);
}

inline void init(int width, int height, const char* title) {
    auto& vk = state();
    if (vk.initialized) {
        trace_step("init: already initialized");
        return;
    }

    trace_step("init: begin");
    ensure_loader();
    trace_step("init: loader ready");
    create_instance();
    trace_step("init: instance ready");
    create_window(width, height, title);
    trace_step("init: window ready");
    create_surface();
    trace_step("init: surface ready");
    pick_physical_device_and_queue_family();
    trace_step("init: physical device ready");
    create_logical_device();
    trace_step("init: logical device ready");
    create_swapchain();
    trace_step("init: swapchain ready");
    create_depth_resources();
    trace_step("init: depth ready");
    create_demo_geometry_buffers();
    trace_step("init: geometry buffers ready");
    create_render_targets();
    trace_step("init: render targets ready");
    create_command_infra();
    trace_step("init: command infra ready");
    create_texture_resources();
    trace_step("init: texture resources ready");
    create_pipeline_scaffold();
    trace_step("init: pipeline scaffold ready");
    ensure_backbuffer();
    trace_step("init: backbuffer ready");

    QueryPerformanceFrequency(&vk.timerFrequency);
    QueryPerformanceCounter(&vk.frameStart);
    vk.closeRequested = false;
    vk.resizePending = false;
    vk.initialized = true;
    rte::backendState.activeKind = RTBackendKind::Vulkan;
    rte::backendState.windowReady = true;
    rte::backendState.drawingReady = false;
    rte::backendState.mode3dReady = false;
    rte::backendState.frameDrawCalls = 0;
    rte::backendState.frameRenderItems = 0;
    trace_step("init: completed");
}

inline void shutdown() {
    auto& vk = state();
    if (vk.window != nullptr) {
        release_backbuffer();
    }

    if (vk.syncReady && vk.device != nullptr && vk.inFlightFence != nullptr && vk.waitForFences != nullptr) {
        vk.waitForFences(vk.device, 1, &vk.inFlightFence, 1, ~static_cast<std::uint64_t>(0));
    }

    if (vk.destroyFramebuffer != nullptr && vk.device != nullptr) {
        for (VkFramebuffer framebuffer : vk.swapchainFramebuffers) {
            if (framebuffer != nullptr) {
                vk.destroyFramebuffer(vk.device, framebuffer, nullptr);
            }
        }
    }
    vk.swapchainFramebuffers.clear();
    vk.framebuffersReady = false;

    release_geometry_buffers();
    frame_vertices().clear();
    frame_indices().clear();

    if (vk.destroyDescriptorPool != nullptr && vk.device != nullptr && vk.sceneDescriptorPool != nullptr) {
        vk.destroyDescriptorPool(vk.device, vk.sceneDescriptorPool, nullptr);
    }
    if (vk.destroySampler != nullptr && vk.device != nullptr && vk.sceneTextureSampler != nullptr) {
        vk.destroySampler(vk.device, vk.sceneTextureSampler, nullptr);
    }
    if (vk.destroyImageView != nullptr && vk.device != nullptr && vk.sceneTextureView != nullptr) {
        vk.destroyImageView(vk.device, vk.sceneTextureView, nullptr);
    }
    if (vk.destroyImage != nullptr && vk.device != nullptr && vk.sceneTextureImage != nullptr) {
        vk.destroyImage(vk.device, vk.sceneTextureImage, nullptr);
    }
    if (vk.freeMemory != nullptr && vk.device != nullptr && vk.sceneTextureMemory != nullptr) {
        vk.freeMemory(vk.device, vk.sceneTextureMemory, nullptr);
    }
    if (vk.destroyDescriptorSetLayout != nullptr && vk.device != nullptr && vk.sceneDescriptorSetLayout != nullptr) {
        vk.destroyDescriptorSetLayout(vk.device, vk.sceneDescriptorSetLayout, nullptr);
    }
    vk.sceneDescriptorSet = nullptr;
    vk.sceneDescriptorPool = nullptr;
    vk.sceneTextureSampler = nullptr;
    vk.sceneTextureView = nullptr;
    vk.sceneTextureImage = nullptr;
    vk.sceneTextureMemory = nullptr;
    vk.sceneDescriptorSetLayout = nullptr;
    vk.descriptorSetReady = false;
    vk.textureSamplerReady = false;
    vk.textureImageReady = false;

    if (vk.destroyPipeline != nullptr && vk.device != nullptr && vk.demoGraphicsPipeline != nullptr) {
        vk.destroyPipeline(vk.device, vk.demoGraphicsPipeline, nullptr);
    }
    if (vk.destroyPipelineLayout != nullptr && vk.device != nullptr && vk.demoPipelineLayout != nullptr) {
        vk.destroyPipelineLayout(vk.device, vk.demoPipelineLayout, nullptr);
    }
    if (vk.destroyShaderModule != nullptr && vk.device != nullptr) {
        if (vk.demoVertexShader != nullptr) {
            vk.destroyShaderModule(vk.device, vk.demoVertexShader, nullptr);
        }
        if (vk.demoFragmentShader != nullptr) {
            vk.destroyShaderModule(vk.device, vk.demoFragmentShader, nullptr);
        }
    }
    vk.demoVertexShader = nullptr;
    vk.demoFragmentShader = nullptr;
    vk.demoPipelineLayout = nullptr;
    vk.demoGraphicsPipeline = nullptr;
    vk.shaderAssetsReady = false;
    vk.shaderModulesReady = false;
    vk.pipelineLayoutReady = false;
    vk.graphicsPipelineReady = false;
    vk.shaderAssetRoot.clear();

    if (vk.destroyImageView != nullptr && vk.device != nullptr && vk.depthImageView != nullptr) {
        vk.destroyImageView(vk.device, vk.depthImageView, nullptr);
    }
    if (vk.destroyImage != nullptr && vk.device != nullptr && vk.depthImage != nullptr) {
        vk.destroyImage(vk.device, vk.depthImage, nullptr);
    }
    if (vk.freeMemory != nullptr && vk.device != nullptr && vk.depthImageMemory != nullptr) {
        vk.freeMemory(vk.device, vk.depthImageMemory, nullptr);
    }
    vk.depthImageView = nullptr;
    vk.depthImage = nullptr;
    vk.depthImageMemory = nullptr;
    vk.depthReady = false;

    if (vk.destroyRenderPass != nullptr && vk.device != nullptr && vk.renderPass != nullptr) {
        vk.destroyRenderPass(vk.device, vk.renderPass, nullptr);
    }
    vk.renderPass = nullptr;
    vk.renderPassReady = false;

    if (vk.destroyImageView != nullptr && vk.device != nullptr) {
        for (VkImageView imageView : vk.swapchainImageViews) {
            if (imageView != nullptr) {
                vk.destroyImageView(vk.device, imageView, nullptr);
            }
        }
    }
    vk.swapchainImageViews.clear();
    vk.swapchainImages.clear();

    if (vk.destroySwapchain != nullptr && vk.device != nullptr && vk.swapchain != nullptr) {
        vk.destroySwapchain(vk.device, vk.swapchain, nullptr);
        vk.swapchain = nullptr;
    }
    vk.swapchainFormat = 0;
    vk.swapchainExtent = {0, 0};
    vk.swapchainReady = false;

    if (vk.destroyFence != nullptr && vk.device != nullptr && vk.inFlightFence != nullptr) {
        vk.destroyFence(vk.device, vk.inFlightFence, nullptr);
    }
    if (vk.destroySemaphore != nullptr && vk.device != nullptr) {
        if (vk.imageAvailableSemaphore != nullptr) {
            vk.destroySemaphore(vk.device, vk.imageAvailableSemaphore, nullptr);
        }
        if (vk.renderFinishedSemaphore != nullptr) {
            vk.destroySemaphore(vk.device, vk.renderFinishedSemaphore, nullptr);
        }
    }
    vk.inFlightFence = nullptr;
    vk.imageAvailableSemaphore = nullptr;
    vk.renderFinishedSemaphore = nullptr;
    vk.syncReady = false;
    vk.framePathReady = false;
    vk.frameAcquired = false;
    vk.resizePending = false;
    vk.currentImageIndex = 0;
    vk.presentedFrames = 0;

    if (vk.freeCommandBuffers != nullptr && vk.device != nullptr && vk.commandPool != nullptr && !vk.commandBuffers.empty()) {
        vk.freeCommandBuffers(vk.device, vk.commandPool, static_cast<std::uint32_t>(vk.commandBuffers.size()), vk.commandBuffers.data());
    }
    vk.commandBuffers.clear();
    vk.commandBuffersReady = false;

    if (vk.destroyCommandPool != nullptr && vk.device != nullptr && vk.commandPool != nullptr) {
        vk.destroyCommandPool(vk.device, vk.commandPool, nullptr);
    }
    vk.commandPool = nullptr;
    vk.commandPoolReady = false;

    if (vk.destroyDevice != nullptr && vk.device != nullptr) {
        vk.destroyDevice(vk.device, nullptr);
        vk.device = nullptr;
    }
    vk.graphicsQueue = nullptr;
    vk.graphicsQueueFamilyIndex = VK_INVALID_QUEUE_FAMILY;
    vk.deviceReady = false;

    if (vk.destroySurface != nullptr && vk.surface != nullptr && vk.instance != nullptr) {
        vk.destroySurface(vk.instance, vk.surface, nullptr);
        vk.surface = nullptr;
    }
    vk.surfaceReady = false;

    if (vk.window != nullptr) {
        DestroyWindow(vk.window);
        vk.window = nullptr;
    }
    vk.hostParentWindow = nullptr;
    vk.embeddedMode = false;

    if (vk.windowClass != 0 && vk.windowInstance != nullptr) {
        UnregisterClassW(vk.className.c_str(), vk.windowInstance);
        vk.windowClass = 0;
    }

    if (vk.destroyInstance != nullptr && vk.instance != nullptr) {
        vk.destroyInstance(vk.instance, nullptr);
        vk.instance = nullptr;
    }

    if (vk.loader != nullptr) {
        FreeLibrary(vk.loader);
        vk.loader = nullptr;
    }

    vk.initialized = false;
    vk.closeRequested = true;
    vk.gpuCount = 0;
    vk.physicalDevice = nullptr;
    vk.presentationReady = false;
    vk.destroySwapchain = nullptr;
    vk.getSwapchainImages = nullptr;
    vk.createImage = nullptr;
    vk.destroyImage = nullptr;
    vk.createImageView = nullptr;
    vk.destroyImageView = nullptr;
    vk.createShaderModule = nullptr;
    vk.destroyShaderModule = nullptr;
    vk.createBuffer = nullptr;
    vk.destroyBuffer = nullptr;
    vk.getBufferMemoryRequirements = nullptr;
    vk.getImageMemoryRequirements = nullptr;
    vk.allocateMemory = nullptr;
    vk.freeMemory = nullptr;
    vk.bindBufferMemory = nullptr;
    vk.bindImageMemory = nullptr;
    vk.mapMemory = nullptr;
    vk.unmapMemory = nullptr;
    vk.createPipelineLayout = nullptr;
    vk.destroyPipelineLayout = nullptr;
    vk.createGraphicsPipelines = nullptr;
    vk.destroyPipeline = nullptr;
    vk.createRenderPass = nullptr;
    vk.destroyRenderPass = nullptr;
    vk.createFramebuffer = nullptr;
    vk.destroyFramebuffer = nullptr;
    vk.createCommandPool = nullptr;
    vk.destroyCommandPool = nullptr;
    vk.allocateCommandBuffers = nullptr;
    vk.freeCommandBuffers = nullptr;
    vk.resetCommandBuffer = nullptr;
    vk.beginCommandBuffer = nullptr;
    vk.endCommandBuffer = nullptr;
    vk.cmdBeginRenderPass = nullptr;
    vk.cmdEndRenderPass = nullptr;
    vk.cmdBindPipeline = nullptr;
    vk.cmdBindVertexBuffers = nullptr;
    vk.cmdBindIndexBuffer = nullptr;
    vk.cmdDrawIndexed = nullptr;
    vk.createSemaphore = nullptr;
    vk.destroySemaphore = nullptr;
    vk.createFence = nullptr;
    vk.destroyFence = nullptr;
    vk.waitForFences = nullptr;
    vk.resetFences = nullptr;
    vk.acquireNextImage = nullptr;
    vk.queueSubmit = nullptr;
    vk.queuePresent = nullptr;
    vk.getSurfaceCapabilities = nullptr;
    vk.getSurfaceFormats = nullptr;
    vk.getPresentModes = nullptr;
    vk.destroyDevice = nullptr;
    vk.getDeviceQueue = nullptr;
    vk.createDevice = nullptr;
    vk.createWin32Surface = nullptr;
    vk.destroySurface = nullptr;
    vk.getQueueFamilyProperties = nullptr;
    vk.getPhysicalDeviceMemoryProperties = nullptr;
    vk.getWin32PresentationSupport = nullptr;
    vk.getDeviceProcAddr = nullptr;
    vk.destroyInstance = nullptr;
    vk.enumeratePhysicalDevices = nullptr;
    vk.createInstance = nullptr;
    vk.getInstanceProcAddr = nullptr;
    rte::backendState.windowReady = false;
    rte::backendState.drawingReady = false;
    rte::backendState.frameDrawCalls = 0;
    rte::backendState.frameRenderItems = 0;
}

inline int should_close() {
    auto& vk = state();
    pump_messages();
    return vk.closeRequested ? 1 : 0;
}

inline void begin() {
    auto& vk = state();
    if (!vk.initialized) {
        throw std::runtime_error("Vulkan backend was not initialized before engine.begin().");
    }
    if (vk.resizePending) {
        recreate_swapchain_dependent_resources();
    }
    QueryPerformanceCounter(&vk.frameStart);
    vk.frameAcquired = false;
    vk.useSceneRendererThisFrame = false;
    vk.sceneIndexCount = 0;
    frame_vertices().clear();
    frame_indices().clear();
    ensure_backbuffer();
    draw_clear_color();
    rte::backendState.drawingReady = true;
    rte::backendState.frameDrawCalls = 0;
    rte::backendState.frameRenderItems = 0;
}

inline void end() {
    auto& vk = state();
    if (!vk.initialized) {
        return;
    }
    if (vk.resizePending) {
        recreate_swapchain_dependent_resources();
    }

    const bool hasSceneGeometry = vk.useSceneRendererThisFrame && !frame_vertices().empty() && !frame_indices().empty();
    if (hasSceneGeometry) {
        upload_scene_geometry();
        if (acquire_frame_image() && record_current_frame_commands()) {
            submit_and_present_frame();
        }
    } else {
        ensure_backbuffer();
        blit_backbuffer_to_window();
    }
    LARGE_INTEGER frameEnd{};
    QueryPerformanceCounter(&frameEnd);
    const double delta = static_cast<double>(frameEnd.QuadPart - vk.frameStart.QuadPart) / static_cast<double>(vk.timerFrequency.QuadPart);
    vk.lastFrameTime = static_cast<float>(delta);
    rte::backendState.drawingReady = false;

    if (rte::backendState.targetFps > 0) {
        const double target = 1.0 / static_cast<double>(rte::backendState.targetFps);
        if (delta < target) {
            const double remaining = target - delta;
            const DWORD ms = remaining > 0.002 ? static_cast<DWORD>((remaining - 0.001) * 1000.0) : 0;
            if (ms > 0) {
                Sleep(ms);
            }
            LARGE_INTEGER waitEnd{};
            do {
                QueryPerformanceCounter(&waitEnd);
            } while ((static_cast<double>(waitEnd.QuadPart - vk.frameStart.QuadPart) / static_cast<double>(vk.timerFrequency.QuadPart)) < target);
            const double finalDelta = static_cast<double>(waitEnd.QuadPart - vk.frameStart.QuadPart) / static_cast<double>(vk.timerFrequency.QuadPart);
            vk.lastFrameTime = static_cast<float>(finalDelta);
        }
    }
}

inline void clear(RTColor color) {
    state().clearColor = color;
}

inline RTVec3 vec3_add(RTVec3 a, RTVec3 b) {
    return RTVec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline RTVec3 vec3_sub(RTVec3 a, RTVec3 b) {
    return RTVec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline RTVec3 vec3_mul(RTVec3 value, float scalar) {
    return RTVec3{value.x * scalar, value.y * scalar, value.z * scalar};
}

inline RTVec3 vec3_div(RTVec3 value, float scalar) {
    if (std::fabs(scalar) < 0.0001f) {
        return RTVec3{0.0f, 0.0f, 0.0f};
    }
    return RTVec3{value.x / scalar, value.y / scalar, value.z / scalar};
}

inline float vec3_dot(RTVec3 a, RTVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline RTVec3 vec3_cross(RTVec3 a, RTVec3 b) {
    return RTVec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float vec3_length(RTVec3 value) {
    return std::sqrt(vec3_dot(value, value));
}

inline RTVec3 vec3_normalize(RTVec3 value) {
    const float length = vec3_length(value);
    if (length < 0.0001f) {
        return RTVec3{0.0f, 0.0f, 0.0f};
    }
    return vec3_mul(value, 1.0f / length);
}

inline Mat4 mat4_identity() {
    Mat4 result{};
    result.value[0] = 1.0f;
    result.value[5] = 1.0f;
    result.value[10] = 1.0f;
    result.value[15] = 1.0f;
    return result;
}

inline Mat4 mat4_mul(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.value[k * 4 + row] * b.value[column * 4 + k];
            }
            result.value[column * 4 + row] = sum;
        }
    }
    return result;
}

inline Mat4 mat4_look_at(RTVec3 eye, RTVec3 center, RTVec3 upHint) {
    const RTVec3 forward = vec3_normalize(vec3_sub(center, eye));
    RTVec3 right = vec3_normalize(vec3_cross(forward, upHint));
    if (vec3_length(right) < 0.0001f) {
        right = RTVec3{1.0f, 0.0f, 0.0f};
    }
    const RTVec3 up = vec3_normalize(vec3_cross(right, forward));

    Mat4 result = mat4_identity();
    result.value[0] = right.x;
    result.value[1] = right.y;
    result.value[2] = right.z;

    result.value[4] = up.x;
    result.value[5] = up.y;
    result.value[6] = up.z;

    result.value[8] = -forward.x;
    result.value[9] = -forward.y;
    result.value[10] = -forward.z;

    result.value[12] = -vec3_dot(right, eye);
    result.value[13] = -vec3_dot(up, eye);
    result.value[14] = vec3_dot(forward, eye);
    result.value[15] = 1.0f;
    return result;
}

inline Mat4 mat4_perspective_vulkan(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    const float clampedAspect = std::max(0.0001f, aspect);
    const float fovRadians = std::max(0.2f, fovDegrees) * 3.14159265358979323846f / 180.0f;
    const float tanHalf = std::tan(fovRadians * 0.5f);
    Mat4 result{};
    result.value[0] = 1.0f / (clampedAspect * tanHalf);
    result.value[5] = -1.0f / tanHalf;
    result.value[10] = farPlane / (nearPlane - farPlane);
    result.value[11] = -1.0f;
    result.value[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
    return result;
}

inline ScenePushConstants make_scene_push_constants() {
    auto& vk = state();
    ScenePushConstants constants{};
    const float aspect = static_cast<float>(std::max(1u, vk.swapchainExtent.width)) /
        static_cast<float>(std::max(1u, vk.swapchainExtent.height));
    const Mat4 view = mat4_look_at(vk.cameraPosition, vk.cameraTarget, vk.cameraUp);
    const Mat4 projection = mat4_perspective_vulkan(vk.cameraFov, aspect, 0.05f, 256.0f);
    const Mat4 mvp = mat4_mul(projection, view);
    std::memcpy(constants.mvp, mvp.value, sizeof(constants.mvp));
    constants.viewportPostfx[0] = static_cast<float>(std::max(1u, vk.swapchainExtent.width));
    constants.viewportPostfx[1] = static_cast<float>(std::max(1u, vk.swapchainExtent.height));
    constants.viewportPostfx[2] = rte::backendState.postfxState.exposure;
    constants.viewportPostfx[3] = rte::backendState.postfxState.vignette;
    constants.fogParams[0] = rte::backendState.postfxState.fogNear;
    constants.fogParams[1] = rte::backendState.postfxState.fogFar;
    constants.fogParams[2] = rte::backendState.postfxState.fogDensity;
    constants.fogParams[3] = rte::backendState.postfxState.volumetric;
    constants.fogColor[0] = static_cast<float>(rte::backendState.postfxState.fogR) / 255.0f;
    constants.fogColor[1] = static_cast<float>(rte::backendState.postfxState.fogG) / 255.0f;
    constants.fogColor[2] = static_cast<float>(rte::backendState.postfxState.fogB) / 255.0f;
    constants.fogColor[3] = static_cast<float>(rte::backendState.postfxState.filmGrain);
    constants.grading[0] = rte::backendState.postfxState.saturation;
    constants.grading[1] = rte::backendState.postfxState.contrast;
    constants.grading[2] = rte::backendState.postfxState.bloom;
    constants.grading[3] = rte::backendState.lightState.enabled ? std::max(0.0f, rte::backendState.lightState.intensity) : 0.0f;
    return constants;
}

inline bool project_world_point(RTVec3 world, POINT& outPoint, float& outDepth) {
    auto& vk = state();
    if (vk.backbufferWidth <= 0 || vk.backbufferHeight <= 0) {
        return false;
    }

    const RTVec3 forward = vec3_normalize(vec3_sub(vk.cameraTarget, vk.cameraPosition));
    RTVec3 right = vec3_normalize(vec3_cross(forward, vk.cameraUp));
    if (vec3_length(right) < 0.0001f) {
        right = RTVec3{1.0f, 0.0f, 0.0f};
    }
    const RTVec3 up = vec3_normalize(vec3_cross(right, forward));

    const RTVec3 relative = vec3_sub(world, vk.cameraPosition);
    const float viewX = vec3_dot(relative, right);
    const float viewY = vec3_dot(relative, up);
    const float viewZ = vec3_dot(relative, forward);
    outDepth = viewZ;
    if (viewZ <= 0.05f) {
        return false;
    }

    const float fovRadians = std::max(0.2f, vk.cameraFov) * 3.14159265358979323846f / 180.0f;
    const float focal = (static_cast<float>(vk.backbufferHeight) * 0.5f) / std::tan(fovRadians * 0.5f);
    const float centerX = static_cast<float>(vk.backbufferWidth) * 0.5f;
    const float centerY = static_cast<float>(vk.backbufferHeight) * 0.5f;

    outPoint.x = static_cast<LONG>(centerX + (viewX / viewZ) * focal);
    outPoint.y = static_cast<LONG>(centerY - (viewY / viewZ) * focal);
    return true;
}

inline void draw_line_2d(const POINT& a, const POINT& b, RTColor color, int width = 1) {
    auto& vk = state();
    if (vk.backbufferDC == nullptr) {
        return;
    }
    HPEN pen = CreatePen(PS_SOLID, width, RGB(color.r, color.g, color.b));
    HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(vk.backbufferDC, pen));
    MoveToEx(vk.backbufferDC, a.x, a.y, nullptr);
    LineTo(vk.backbufferDC, b.x, b.y);
    SelectObject(vk.backbufferDC, oldPen);
    DeleteObject(pen);
}

inline RTColor shade_color(RTColor color, float brightness) {
    brightness = std::clamp(brightness, 0.15f, 1.0f);
    auto shade = [brightness](unsigned char channel) -> unsigned char {
        const int value = static_cast<int>(std::round(static_cast<float>(channel) * brightness));
        return static_cast<unsigned char>(std::clamp(value, 0, 255));
    };
    return RTColor{shade(color.r), shade(color.g), shade(color.b), color.a};
}

inline void draw_filled_polygon_2d(const std::vector<POINT>& points, RTColor fillColor, RTColor outlineColor, int outlineWidth = 1) {
    auto& vk = state();
    if (vk.backbufferDC == nullptr || points.size() < 3) {
        return;
    }

    HPEN pen = CreatePen(PS_SOLID, outlineWidth, RGB(outlineColor.r, outlineColor.g, outlineColor.b));
    HBRUSH brush = CreateSolidBrush(RGB(fillColor.r, fillColor.g, fillColor.b));
    HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(vk.backbufferDC, pen));
    HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(vk.backbufferDC, brush));
    Polygon(vk.backbufferDC, points.data(), static_cast<int>(points.size()));
    SelectObject(vk.backbufferDC, oldBrush);
    SelectObject(vk.backbufferDC, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

inline void draw_polyline_3d(const std::vector<RTVec3>& vertices, RTColor color, bool closed = false, int width = 1) {
    if (vertices.size() < 2) {
        return;
    }
    std::vector<POINT> points(vertices.size());
    std::vector<bool> visible(vertices.size(), false);
    std::vector<float> depths(vertices.size(), 0.0f);
    for (size_t i = 0; i < vertices.size(); ++i) {
        visible[i] = project_world_point(vertices[i], points[i], depths[i]);
    }
    for (size_t i = 1; i < vertices.size(); ++i) {
        if (visible[i - 1] && visible[i]) {
            draw_line_2d(points[i - 1], points[i], color, width);
        }
    }
    if (closed && visible.front() && visible.back()) {
        draw_line_2d(points.back(), points.front(), color, width);
    }
}

inline bool project_polygon_3d(const std::vector<RTVec3>& vertices, std::vector<POINT>& outPoints, float& outDepth) {
    if (vertices.size() < 3) {
        return false;
    }

    outPoints.clear();
    outPoints.reserve(vertices.size());
    float depthSum = 0.0f;
    for (const auto& vertex : vertices) {
        POINT point{};
        float depth = 0.0f;
        if (!project_world_point(vertex, point, depth)) {
            return false;
        }
        outPoints.push_back(point);
        depthSum += depth;
    }
    outDepth = depthSum / static_cast<float>(vertices.size());
    return true;
}

inline void draw_filled_face_3d(const std::vector<RTVec3>& vertices, RTColor color, RTVec3 normalHint = RTVec3{0.0f, 1.0f, 0.0f}) {
    if (vertices.size() < 3) {
        return;
    }

    std::vector<POINT> points;
    float depth = 0.0f;
    if (!project_polygon_3d(vertices, points, depth)) {
        return;
    }

    RTVec3 faceNormal = normalHint;
    if (vertices.size() >= 3) {
        const RTVec3 edgeA = vec3_sub(vertices[1], vertices[0]);
        const RTVec3 edgeB = vec3_sub(vertices[2], vertices[0]);
        faceNormal = vec3_normalize(vec3_cross(edgeA, edgeB));
    }

    RTVec3 lightDir = vec3_normalize(RTVec3{-0.45f, 0.8f, -0.35f});
    float brightness = 0.28f + std::max(0.0f, vec3_dot(faceNormal, lightDir)) * 0.72f;
    const RTColor fill = shade_color(color, brightness);
    const RTColor outline = shade_color(color, std::min(1.0f, brightness + 0.18f));
    draw_filled_polygon_2d(points, fill, outline, 1);
}

inline void set_camera(float px, float py, float pz, float tx, float ty, float tz, float ux, float uy, float uz, float fov) {
    auto& vk = state();
    vk.cameraPosition = RTVec3{px, py, pz};
    vk.cameraTarget = RTVec3{tx, ty, tz};
    vk.cameraUp = RTVec3{ux, uy, uz};
    vk.cameraFov = fov;
    rte::backendState.mode3dReady = true;
}

inline float camera_fov() { return state().cameraFov; }
inline void set_camera_fov(float fov) { state().cameraFov = fov; }
inline void camera_orbit(float yawDegrees, float pitchDegrees, float radiusDelta) {
    auto& vk = state();
    RTVec3 offset = vec3_sub(vk.cameraPosition, vk.cameraTarget);
    float radius = vec3_length(offset);
    if (radius < 0.001f) {
        radius = 0.001f;
    }

    float yaw = std::atan2(offset.z, offset.x);
    float pitch = std::asin(offset.y / radius);
    constexpr float degToRad = 3.14159265358979323846f / 180.0f;

    yaw += yawDegrees * degToRad;
    pitch += pitchDegrees * degToRad;
    pitch = std::clamp(pitch, -1.4f, 1.4f);
    radius = std::max(0.5f, radius + radiusDelta);

    offset.x = std::cos(pitch) * std::cos(yaw) * radius;
    offset.y = std::sin(pitch) * radius;
    offset.z = std::cos(pitch) * std::sin(yaw) * radius;
    vk.cameraPosition = vec3_add(vk.cameraTarget, offset);
}

inline void set_target_fps(int fps) { (void)fps; }
inline float frame_time() { return state().lastFrameTime; }
inline int key_down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0 ? 1 : 0; }
inline int key_pressed(int key) { return (GetAsyncKeyState(key) & 0x0001) != 0 ? 1 : 0; }
inline int mouse_down(int button) { return (GetAsyncKeyState(button) & 0x8000) != 0 ? 1 : 0; }

inline float mouse_x() {
    POINT point{};
    GetCursorPos(&point);
    return static_cast<float>(point.x);
}

inline float mouse_y() {
    POINT point{};
    GetCursorPos(&point);
    return static_cast<float>(point.y);
}

inline int gpu_count() {
    return static_cast<int>(state().gpuCount);
}

inline int surface_ready() {
    return state().surfaceReady ? 1 : 0;
}

inline int device_ready() {
    return state().deviceReady ? 1 : 0;
}

inline int presentation_ready() {
    return state().presentationReady ? 1 : 0;
}

inline int queue_family_index() {
    return state().graphicsQueueFamilyIndex == VK_INVALID_QUEUE_FAMILY
        ? -1
        : static_cast<int>(state().graphicsQueueFamilyIndex);
}

inline int swapchain_ready() {
    return state().swapchainReady ? 1 : 0;
}

inline int swapchain_image_count() {
    return static_cast<int>(state().swapchainImages.size());
}

inline int swapchain_width() {
    return static_cast<int>(state().swapchainExtent.width);
}

inline int swapchain_height() {
    return static_cast<int>(state().swapchainExtent.height);
}

inline int render_pass_ready() {
    return state().renderPassReady ? 1 : 0;
}

inline int framebuffer_count() {
    return static_cast<int>(state().swapchainFramebuffers.size());
}

inline int depth_ready() {
    return state().depthReady ? 1 : 0;
}

inline int geometry_buffers_ready() {
    return state().geometryBuffersReady ? 1 : 0;
}

inline int vertex_buffer_bytes() {
    return static_cast<int>(state().demoVertexBufferBytes);
}

inline int index_buffer_bytes() {
    return static_cast<int>(state().demoIndexBufferBytes);
}

inline int shader_assets_ready() {
    return state().shaderAssetsReady ? 1 : 0;
}

inline int shader_modules_ready() {
    return state().shaderModulesReady ? 1 : 0;
}

inline int pipeline_layout_ready() {
    return state().pipelineLayoutReady ? 1 : 0;
}

inline int texture_sampler_ready() {
    return state().textureSamplerReady ? 1 : 0;
}

inline int texture_image_ready() {
    return state().textureImageReady ? 1 : 0;
}

inline int descriptor_set_ready() {
    return state().descriptorSetReady ? 1 : 0;
}

inline int graphics_pipeline_ready() {
    return state().graphicsPipelineReady ? 1 : 0;
}

inline int command_pool_ready() {
    return state().commandPoolReady ? 1 : 0;
}

inline int command_buffer_count() {
    return static_cast<int>(state().commandBuffers.size());
}

inline int sync_ready() {
    return state().syncReady ? 1 : 0;
}

inline int frame_path_ready() {
    return state().framePathReady ? 1 : 0;
}

inline int frame_acquired() {
    return state().frameAcquired ? 1 : 0;
}

inline int presented_frame_count() {
    return static_cast<int>(state().presentedFrames);
}

inline void draw_grid(int slices, float spacing) {
    const int count = std::max(1, slices);
    const float extent = count * spacing * 0.5f;
    for (int i = -count; i <= count; ++i) {
        const float offset = static_cast<float>(i) * spacing;
        draw_polyline_3d({
            RTVec3{-extent, 0.0f, offset},
            RTVec3{ extent, 0.0f, offset}
        }, RTColor{70, 102, 136, 255});
        draw_polyline_3d({
            RTVec3{offset, 0.0f, -extent},
            RTVec3{offset, 0.0f,  extent}
        }, RTColor{70, 102, 136, 255});
    }
}

inline SceneVertex make_scene_vertex(RTVec3 world, RTColor color, RTVec3 normal, float u, float v) {
    const float emissiveR = static_cast<float>(rte::backendState.materialState.emissiveR) / 255.0f;
    const float emissiveG = static_cast<float>(rte::backendState.materialState.emissiveG) / 255.0f;
    const float emissiveB = static_cast<float>(rte::backendState.materialState.emissiveB) / 255.0f;
    return SceneVertex{
        world.x,
        world.y,
        world.z,
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        normal.x,
        normal.y,
        normal.z,
        u,
        v,
        emissiveR,
        emissiveG,
        emissiveB,
        rte::backendState.materialState.roughness,
        rte::backendState.materialState.metallic,
        rte::backendState.materialState.textured ? 1.0f : 0.0f
    };
}

inline void append_scene_triangle(
    RTVec3 a, RTVec3 b, RTVec3 c,
    RTColor colorA, RTColor colorB, RTColor colorC,
    RTVec3 normalA, RTVec3 normalB, RTVec3 normalC,
    RTVec2 uvA, RTVec2 uvB, RTVec2 uvC) {
    auto& vertices = frame_vertices();
    auto& indices = frame_indices();
    if (vertices.size() > 65000u || indices.size() > 65000u) {
        return;
    }

    const SceneVertex va = make_scene_vertex(a, colorA, vec3_normalize(normalA), uvA.x, uvA.y);
    const SceneVertex vb = make_scene_vertex(b, colorB, vec3_normalize(normalB), uvB.x, uvB.y);
    const SceneVertex vc = make_scene_vertex(c, colorC, vec3_normalize(normalC), uvC.x, uvC.y);

    const std::uint16_t base = static_cast<std::uint16_t>(vertices.size());
    vertices.push_back(va);
    vertices.push_back(vb);
    vertices.push_back(vc);
    indices.push_back(base + 0u);
    indices.push_back(base + 1u);
    indices.push_back(base + 2u);
    state().useSceneRendererThisFrame = true;
    rte::backendState.frameDrawCalls += 1;
}

inline void append_scene_triangle_flat(RTVec3 a, RTVec3 b, RTVec3 c, RTColor baseColor) {
    const RTVec3 normal = vec3_cross(vec3_sub(b, a), vec3_sub(c, a));
    append_scene_triangle(
        a, b, c,
        baseColor, baseColor, baseColor,
        normal, normal, normal,
        RTVec2{0.0f, 0.0f}, RTVec2{1.0f, 0.0f}, RTVec2{1.0f, 1.0f});
}

inline void append_scene_quad(RTVec3 a, RTVec3 b, RTVec3 c, RTVec3 d, RTColor color) {
    const RTVec3 normal = vec3_cross(vec3_sub(b, a), vec3_sub(c, a));
    append_scene_triangle(
        a, b, c,
        color, color, color,
        normal, normal, normal,
        RTVec2{0.0f, 0.0f}, RTVec2{1.0f, 0.0f}, RTVec2{1.0f, 1.0f});
    append_scene_triangle(
        a, c, d,
        color, color, color,
        normal, normal, normal,
        RTVec2{0.0f, 0.0f}, RTVec2{1.0f, 1.0f}, RTVec2{0.0f, 1.0f});
}

inline void draw_cube(RTVec3 position, RTVec3 size, RTColor color) {
    const float hx = size.x * 0.5f;
    const float hy = size.y * 0.5f;
    const float hz = size.z * 0.5f;
    const std::array<RTVec3, 8> vertices = {{
        {position.x - hx, position.y - hy, position.z - hz},
        {position.x + hx, position.y - hy, position.z - hz},
        {position.x + hx, position.y + hy, position.z - hz},
        {position.x - hx, position.y + hy, position.z - hz},
        {position.x - hx, position.y - hy, position.z + hz},
        {position.x + hx, position.y - hy, position.z + hz},
        {position.x + hx, position.y + hy, position.z + hz},
        {position.x - hx, position.y + hy, position.z + hz}
    }};
    append_scene_quad(vertices[0], vertices[1], vertices[2], vertices[3], color);
    append_scene_quad(vertices[4], vertices[5], vertices[6], vertices[7], color);
    append_scene_quad(vertices[0], vertices[4], vertices[7], vertices[3], color);
    append_scene_quad(vertices[1], vertices[5], vertices[6], vertices[2], color);
    append_scene_quad(vertices[3], vertices[2], vertices[6], vertices[7], color);
    append_scene_quad(vertices[0], vertices[1], vertices[5], vertices[4], color);
}

inline void draw_plane(RTVec3 position, RTVec2 size, RTColor color) {
    const float hx = size.x * 0.5f;
    const float hz = size.y * 0.5f;
    append_scene_quad(
        RTVec3{position.x - hx, position.y, position.z - hz},
        RTVec3{position.x + hx, position.y, position.z - hz},
        RTVec3{position.x + hx, position.y, position.z + hz},
        RTVec3{position.x - hx, position.y, position.z + hz},
        color);
}

inline void draw_sphere(RTVec3 position, float radius, RTColor color) {
    const int latitudeSegments = 10;
    const int longitudeSegments = 14;
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        const float v0 = static_cast<float>(lat) / static_cast<float>(latitudeSegments);
        const float v1 = static_cast<float>(lat + 1) / static_cast<float>(latitudeSegments);
        const float phi0 = (v0 - 0.5f) * 3.14159265358979323846f;
        const float phi1 = (v1 - 0.5f) * 3.14159265358979323846f;

        for (int lon = 0; lon < longitudeSegments; ++lon) {
            const float u0 = static_cast<float>(lon) / static_cast<float>(longitudeSegments);
            const float u1 = static_cast<float>(lon + 1) / static_cast<float>(longitudeSegments);
            const float theta0 = u0 * 6.28318530717958647692f;
            const float theta1 = u1 * 6.28318530717958647692f;

            const RTVec3 n00{std::cos(phi0) * std::cos(theta0), std::sin(phi0), std::cos(phi0) * std::sin(theta0)};
            const RTVec3 n01{std::cos(phi0) * std::cos(theta1), std::sin(phi0), std::cos(phi0) * std::sin(theta1)};
            const RTVec3 n10{std::cos(phi1) * std::cos(theta0), std::sin(phi1), std::cos(phi1) * std::sin(theta0)};
            const RTVec3 n11{std::cos(phi1) * std::cos(theta1), std::sin(phi1), std::cos(phi1) * std::sin(theta1)};

            const RTVec3 p00 = vec3_add(position, vec3_mul(n00, radius));
            const RTVec3 p01 = vec3_add(position, vec3_mul(n01, radius));
            const RTVec3 p10 = vec3_add(position, vec3_mul(n10, radius));
            const RTVec3 p11 = vec3_add(position, vec3_mul(n11, radius));

            append_scene_triangle(
                p00, p10, p11,
                color, color, color,
                n00, n10, n11,
                RTVec2{u0, v0}, RTVec2{u0, v1}, RTVec2{u1, v1});
            append_scene_triangle(
                p00, p11, p01,
                color, color, color,
                n00, n11, n01,
                RTVec2{u0, v0}, RTVec2{u1, v1}, RTVec2{u1, v0});
        }
    }
}

inline void draw_text(const char* text, int x, int y, int size, RTColor color) {
    auto& vk = state();
    if (vk.window == nullptr || vk.backbufferDC == nullptr || text == nullptr) {
        return;
    }
    SetTextColor(vk.backbufferDC, RGB(color.r, color.g, color.b));
    SetBkMode(vk.backbufferDC, TRANSPARENT);
    HFONT font = CreateFontA(size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(vk.backbufferDC, font));
    TextOutA(vk.backbufferDC, x, y, text, static_cast<int>(std::strlen(text)));
    SelectObject(vk.backbufferDC, oldFont);
    DeleteObject(font);
    rte::backendState.frameDrawCalls += 1;
}

inline void draw_fps(int x, int y) {
    const int fps = state().lastFrameTime > 0.0f ? static_cast<int>(1.0f / state().lastFrameTime) : 0;
    std::ostringstream stream;
    stream << fps << " fps";
    draw_text(stream.str().c_str(), x, y, 18, RTColor{220, 236, 255, 255});
}
}

#else

namespace vulkan_backend {
inline bool runtime_available() { return false; }
inline void init(int width, int height, const char* title) {
    (void)width; (void)height; (void)title;
    throw std::runtime_error("Vulkan backend is currently only scaffolded on Windows in this build.");
}
inline void shutdown() {}
inline int should_close() { return 1; }
inline void begin() {}
inline void end() {}
inline void clear(RTColor color) { (void)color; }
inline void set_camera(float px, float py, float pz, float tx, float ty, float tz, float ux, float uy, float uz, float fov) {
    (void)px; (void)py; (void)pz; (void)tx; (void)ty; (void)tz; (void)ux; (void)uy; (void)uz; (void)fov;
}
inline float camera_fov() { return 45.0f; }
inline void set_camera_fov(float fov) { (void)fov; }
inline void camera_orbit(float yawDegrees, float pitchDegrees, float radiusDelta) { (void)yawDegrees; (void)pitchDegrees; (void)radiusDelta; }
inline void set_target_fps(int fps) { (void)fps; }
inline float frame_time() { return 0.0f; }
inline int key_down(int key) { (void)key; return 0; }
inline int key_pressed(int key) { (void)key; return 0; }
inline int mouse_down(int button) { (void)button; return 0; }
inline float mouse_x() { return 0.0f; }
inline float mouse_y() { return 0.0f; }
inline int gpu_count() { return 0; }
inline int surface_ready() { return 0; }
inline int device_ready() { return 0; }
inline int presentation_ready() { return 0; }
inline int queue_family_index() { return -1; }
inline int swapchain_ready() { return 0; }
inline int swapchain_image_count() { return 0; }
inline int swapchain_width() { return 0; }
inline int swapchain_height() { return 0; }
inline int render_pass_ready() { return 0; }
inline int framebuffer_count() { return 0; }
inline int depth_ready() { return 0; }
inline int geometry_buffers_ready() { return 0; }
inline int vertex_buffer_bytes() { return 0; }
inline int index_buffer_bytes() { return 0; }
inline int shader_assets_ready() { return 0; }
inline int shader_modules_ready() { return 0; }
inline int pipeline_layout_ready() { return 0; }
inline int texture_sampler_ready() { return 0; }
inline int texture_image_ready() { return 0; }
inline int descriptor_set_ready() { return 0; }
inline int graphics_pipeline_ready() { return 0; }
inline int command_pool_ready() { return 0; }
inline int command_buffer_count() { return 0; }
inline int sync_ready() { return 0; }
inline int frame_path_ready() { return 0; }
inline int frame_acquired() { return 0; }
inline int presented_frame_count() { return 0; }
inline void attach_host_window(void* parent_window, int x, int y, int width, int height, const char* title) {
    (void)parent_window; (void)x; (void)y; (void)width; (void)height; (void)title;
}
inline void resize_host_window(int x, int y, int width, int height) {
    (void)x; (void)y; (void)width; (void)height;
}
inline void upload_scene_texture_rgba(int width, int height, const unsigned char* pixels) {
    (void)width; (void)height; (void)pixels;
}
inline void draw_grid(int slices, float spacing) { (void)slices; (void)spacing; }
inline void draw_cube(RTVec3 position, RTVec3 size, RTColor color) { (void)position; (void)size; (void)color; }
inline void draw_plane(RTVec3 position, RTVec2 size, RTColor color) { (void)position; (void)size; (void)color; }
inline void draw_sphere(RTVec3 position, float radius, RTColor color) { (void)position; (void)radius; (void)color; }
inline void draw_text(const char* text, int x, int y, int size, RTColor color) { (void)text; (void)x; (void)y; (void)size; (void)color; }
inline void draw_fps(int x, int y) { (void)x; (void)y; }
}


#endif
