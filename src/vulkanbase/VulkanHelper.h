#pragma once

#include <iostream>
#include <vector>
#include <cassert>

#include <vulkan/vulkan.h>
#include "vulkanbase/VulkanStrings.h"
#include "utils/ErrorHelper.h"
#include "loader/LoaderBinary.h"

namespace ct {
	namespace vulkan {
#define DEFAULT_FENCE_TIMEOUT 100000000000

#if defined(__ANDROID__)
#define VK_CHECK_RESULT(f) \
		{																									\
			VkResult res = (f);																					\
			if (res != VK_SUCCESS) {																			\
				LOGE("Fatal : VkResult is \" %s \" in %s at line %d", ct::vulkan::error2string(res).c_str(), __FILE__, __LINE__); \
				assert(res == VK_SUCCESS);																		\
			}																									\
		}
#else
#define VK_CHECK_RESULT(f) \
		{																										\
			VkResult res = (f);																					\
			if (res != VK_SUCCESS) {																			\
				std::cout << "Fatal : VkResult is \"" << ct::vulkan::error2string(res) << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
				assert(res == VK_SUCCESS);																		\
			}																									\
		}
#endif

		struct LogicalDevice {
			VkPhysicalDevice physical_device;
			VkDevice device;
			VkPhysicalDeviceProperties properties;
			VkPhysicalDeviceFeatures features;
			VkPhysicalDeviceFeatures features_enabled;
			std::vector<const char*> extensions;
			std::vector<const char*> extensions_enabled;
			std::vector<std::string> extensions_supported;
			VkPhysicalDeviceMemoryProperties memory_properties;
			VkCommandPool command_pool;
			std::vector<VkCommandBuffer> command_buffer;
			VkQueue queue_graphics;
			VkQueue queue_compute;

			struct {
				uint32_t graphics;
				uint32_t compute;
				uint32_t transfer;
			} queue_family_indices;

		};

		struct DepthStencil {
			VkImage image;
			VkDeviceMemory mem;
			VkImageView view;

			VkFormat depth_format;
		};

		struct Color {
			VkFormat color_format;
			VkColorSpaceKHR color_space;

		};

		struct Framebuffer {
			uint32_t width;
			uint32_t height;
			uint32_t size;
			VkRenderPass render_pass;
			std::vector<VkFramebuffer> framebuffer;
			ct::vulkan::DepthStencil depth_stencil;
			ct::vulkan::Color color;
		};

		struct Pipeline {
			VkPipeline pipeline;
			VkPipelineCache pipeline_cache;
			VkPipelineLayout pipeline_layout;
			VkDescriptorSetLayout descriptor_set_layout;
				
			VkDescriptorPool descriptor_pool;
			VkDescriptorSet descriptor_set;
		};


		struct CommandSubmit {
			VkSubmitInfo info;
		};

		struct Synchronization {
			VkSemaphore present_complete;
			VkSemaphore render_complete;
			VkSemaphore overlay_complete;

			std::vector<VkFence> wait_fences;
		};

		inline void create_instance(std::string title, VkInstance &instance) {
			VkApplicationInfo appInfo = {};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = title.c_str();
			appInfo.pEngineName = title.c_str();
			appInfo.apiVersion = VK_API_VERSION_1_1;


			std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
			instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

			VkInstanceCreateInfo instanceCreateInfo = {};
			instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
			instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
			instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instanceCreateInfo.pNext = NULL;
			instanceCreateInfo.pApplicationInfo = &appInfo;


			VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));
		}

		VkBool32 get_supported_depth_format(VkPhysicalDevice physical_device, VkFormat &depthFormat) {
			// Since all depth formats may be optional, we need to find a suitable depth format to use
			// Start with the highest precision packed format
			std::vector<VkFormat> depthFormats = {
				VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_FORMAT_D32_SFLOAT,
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_FORMAT_D16_UNORM_S8_UINT,
				VK_FORMAT_D16_UNORM
			};

			for (auto& format : depthFormats) {
				VkFormatProperties formatProps;
				vkGetPhysicalDeviceFormatProperties(physical_device, format, &formatProps);
				// Format must support depth stencil attachment for optimal tiling
				if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
					depthFormat = format;
					return true;
				}
			}

			return false;
		}

		inline uint32_t get_memory_type(VkPhysicalDeviceMemoryProperties &memory_properties, uint32_t typeBits, VkMemoryPropertyFlags properties) {
			for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
				if ((typeBits & 1) == 1) {
					if ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
						return i;
					}
				}
				typeBits >>= 1;
			}

			ct::error::exit("Could not find a matching memory type", 1);
		}

		inline uint32_t get_queue_family_index(VkPhysicalDevice &physicalDevice, VkQueueFlagBits queueFlags) {
			uint32_t queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
			assert(queueFamilyCount > 0);
			std::vector<VkQueueFamilyProperties> queueFamilyProperties;
			queueFamilyProperties.resize(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

			// Dedicated queue for compute
			// Try to find a queue family index that supports compute but not graphics
			if (queueFlags & VK_QUEUE_COMPUTE_BIT) {
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
						return i;
						break;
					}
				}
			}

			// Dedicated queue for transfer
			// Try to find a queue family index that supports transfer but not graphics and compute
			if (queueFlags & VK_QUEUE_TRANSFER_BIT) {
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
						return i;
						break;
					}
				}
			}

			// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
				if (queueFamilyProperties[i].queueFlags & queueFlags) {
					return i;
					break;
				}
			}

			throw std::runtime_error("Could not find a matching queue family index");
		}

		inline void search_and_pick_gpu(VkInstance &instance, LogicalDevice &logical_device) {

			// -> physical device
			uint32_t n_gpus = 0;
			vkEnumeratePhysicalDevices(instance, &n_gpus, nullptr);
			std::cout << "n-gpus found: " << n_gpus << std::endl;

			std::vector<VkPhysicalDevice> devices(n_gpus);
			 VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &n_gpus, devices.data()));
			assert(n_gpus > 0);
			// <-

			// -> print out device
			logical_device.physical_device = devices[0];
			VkPhysicalDeviceProperties device_properties;
			vkGetPhysicalDeviceProperties(logical_device.physical_device, &device_properties);
			std::cout << "Device: " << device_properties.deviceName << std::endl;
			std::cout << "Type: " << ct::vulkan::physicaldevicetype2string(device_properties.deviceType) << std::endl;
			std::cout << "API: " << (device_properties.apiVersion >> 22) << "." << ((device_properties.apiVersion >> 12) & 0x3ff) << "." << (device_properties.apiVersion & 0xfff) << std::endl;
			// <-
			
			// -> Query device properties
			vkGetPhysicalDeviceProperties(logical_device.physical_device, &logical_device.properties);
			vkGetPhysicalDeviceFeatures(logical_device.physical_device, &logical_device.features);
			vkGetPhysicalDeviceMemoryProperties(logical_device.physical_device, &logical_device.memory_properties);
			
			std::cout << "n-memory-types: " << logical_device.memory_properties.memoryTypeCount << std::endl;
			// <-
		}


		inline void create_device(LogicalDevice &logical_device) {
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
			float queue_priority = 0.0f;
			// -> graphics queue
			logical_device.queue_family_indices.graphics = get_queue_family_index(logical_device.physical_device, VK_QUEUE_GRAPHICS_BIT);
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = logical_device.queue_family_indices.graphics;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &queue_priority;
			queueCreateInfos.push_back(queueInfo);
			// <-

			// -> compute queue
			queueInfo = {};
			logical_device.queue_family_indices.compute = get_queue_family_index(logical_device.physical_device, VK_QUEUE_COMPUTE_BIT);
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = logical_device.queue_family_indices.compute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &queue_priority;
			queueCreateInfos.push_back(queueInfo);
			// <-
			
			// -> transfer queue
			logical_device.queue_family_indices.transfer = logical_device.queue_family_indices.graphics;
			// <-


			// -> add swapchain extension
			logical_device.extensions = std::vector<const char*>(logical_device.extensions_enabled);
			logical_device.extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			// <-

			// -> create logical device
			VkDeviceCreateInfo deviceCreateInfo = {};
			deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
			deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
			//deviceCreateInfo.pEnabledFeatures = &logical_device.features_enabled;

			if (logical_device.extensions.size() > 0) {
				deviceCreateInfo.enabledExtensionCount = (uint32_t)logical_device.extensions.size();
				deviceCreateInfo.ppEnabledExtensionNames = logical_device.extensions.data();
			}

			VK_CHECK_RESULT(vkCreateDevice(logical_device.physical_device, &deviceCreateInfo, nullptr, &logical_device.device));
			// <-
		}

		inline void create_queues(LogicalDevice &logical_device, VkQueue &queue_graphics, VkQueue &queue_compute) {
			// -> create graphics queue from device
			vkGetDeviceQueue(logical_device.device, logical_device.queue_family_indices.graphics, 0, &queue_graphics);
			vkGetDeviceQueue(logical_device.device, logical_device.queue_family_indices.compute, 0, &queue_compute);
			// <-
		}

		inline void create_command_pool(VkDevice &device, uint32_t queue_family_index, VkCommandPool &command_pool) {
			VkCommandPoolCreateInfo cmdPoolInfo = {};
			cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			cmdPoolInfo.queueFamilyIndex = queue_family_index;
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &command_pool));
		}


		inline void create_command_buffer(uint32_t imagecount, VkDevice &device, VkCommandPool &command_pool, std::vector<VkCommandBuffer> &command_buffer) {
			command_buffer.resize(imagecount);

			VkCommandBufferAllocateInfo commandBufferAllocateInfo {};
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.commandPool = command_pool;
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = imagecount;

			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, command_buffer.data()));
		}


		inline void create_synchronization(VkDevice &device, std::vector<VkCommandBuffer> &command_buffer, Synchronization &sync) {
			VkSemaphoreCreateInfo semaphoreCreateInfo {};
			semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &sync.present_complete);
			vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &sync.render_complete);
			vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &sync.overlay_complete);

			// Wait fences to sync command buffer access
			VkFenceCreateInfo fenceCreateInfo {};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			sync.wait_fences.resize(command_buffer.size());
			for (auto& fence : sync.wait_fences) {
				VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));

			}
		}


		inline void setup_depth_stencil(uint32_t width, uint32_t height, VkPhysicalDevice &physical_device, VkDevice &device, VkPhysicalDeviceMemoryProperties &memory_properties,
				DepthStencil &depth_stencil) {
			VkBool32 validDepthFormat = get_supported_depth_format(physical_device, depth_stencil.depth_format);
			assert(validDepthFormat);

			VkImageCreateInfo image = {};
			image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image.pNext = NULL;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.format = depth_stencil.depth_format;
			image.extent = { width, height, 1 };
			image.mipLevels = 1;
			image.arrayLayers = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			image.flags = 0;

			VkMemoryAllocateInfo mem_alloc = {};
			mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			mem_alloc.pNext = NULL;
			mem_alloc.allocationSize = 0;
			mem_alloc.memoryTypeIndex = 0;

			VkImageViewCreateInfo depthStencilView = {};
			depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			depthStencilView.pNext = NULL;
			depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			depthStencilView.format = depth_stencil.depth_format;
			depthStencilView.flags = 0;
			depthStencilView.subresourceRange = {};
			depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			depthStencilView.subresourceRange.baseMipLevel = 0;
			depthStencilView.subresourceRange.levelCount = 1;
			depthStencilView.subresourceRange.baseArrayLayer = 0;
			depthStencilView.subresourceRange.layerCount = 1;

			VkMemoryRequirements memReqs;

			VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &depth_stencil.image));
			vkGetImageMemoryRequirements(device, depth_stencil.image, &memReqs);
			mem_alloc.allocationSize = memReqs.size;
			mem_alloc.memoryTypeIndex = get_memory_type(memory_properties, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &mem_alloc, nullptr, &depth_stencil.mem));
			VK_CHECK_RESULT(vkBindImageMemory(device, depth_stencil.image, depth_stencil.mem, 0));

			depthStencilView.image = depth_stencil.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &depth_stencil.view));

		}

		inline void setup_render_pass(VkFormat &color_format, VkFormat &depth_format, VkDevice &device, VkRenderPass &render_pass) {
			VkAttachmentDescription attachments[2];
			// Color attachment
			attachments[0].format = color_format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			// Depth attachment
			attachments[1].format = depth_format;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorReference = {};
			colorReference.attachment = 0;
			colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthReference = {};
			depthReference.attachment = 1;
			depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpassDescription = {};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;
			subpassDescription.pDepthStencilAttachment = &depthReference;
			subpassDescription.inputAttachmentCount = 0;
			subpassDescription.pInputAttachments = nullptr;
			subpassDescription.preserveAttachmentCount = 0;
			subpassDescription.pPreserveAttachments = nullptr;
			subpassDescription.pResolveAttachments = nullptr;

			// Subpass dependencies for layout transitions
			VkSubpassDependency dependencies[2];

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = 2;
			renderPassInfo.pAttachments = attachments;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDescription;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies;

			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &render_pass));

		}

		inline void create_pipeline_cache(VkDevice &device, VkPipelineCache &pipeline_cache) {
			VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
			pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipeline_cache));
		}


		inline void setup_framebuffer_from_swapchain(uint32_t width, uint32_t height, uint32_t imagecount, VkDevice &device, std::vector<VkImageView> &color_views,
				VkFormat &color_format, VkColorSpaceKHR &color_space, ct::vulkan::Framebuffer &framebuffer) {

			framebuffer.color.color_format = color_format;
			framebuffer.color.color_space = color_space;
			framebuffer.size = imagecount;
			framebuffer.width = width;
			framebuffer.height = height;

			VkImageView attachments[2];

			// Depth/Stencil attachment is the same for all frame buffers
			attachments[1] = framebuffer.depth_stencil.view;

			VkFramebufferCreateInfo frameBufferCreateInfo = {};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.pNext = NULL;
			frameBufferCreateInfo.renderPass = framebuffer.render_pass;
			frameBufferCreateInfo.attachmentCount = 2;
			frameBufferCreateInfo.pAttachments = attachments;
			frameBufferCreateInfo.width = width;
			frameBufferCreateInfo.height = height;
			frameBufferCreateInfo.layers = 1;

			// Create frame buffers for every swap chain image
			framebuffer.framebuffer.resize(imagecount);
			for (uint32_t i = 0; i < framebuffer.framebuffer.size(); i++) {
				attachments[0] = color_views[i];
				VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &framebuffer.framebuffer[i]));
			}
		}

		VkCommandBuffer get_command_buffer(bool should_begin, VkDevice &device, VkCommandPool &command_pool) {
			VkCommandBuffer cmdBuffer;

			VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
			cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			cmdBufAllocateInfo.commandPool = command_pool;
			cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmdBufAllocateInfo.commandBufferCount = 1;

			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

			// If requested, also start the new command buffer
			if (should_begin) {
				VkCommandBufferBeginInfo cmdBufferBeginInfo {};
				cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
			}

			return cmdBuffer;
		}

		inline void flush_command_buffer(VkDevice &device, VkCommandPool &command_pool, VkQueue &queue, VkCommandBuffer &command_buffer) {
			assert(command_buffer != VK_NULL_HANDLE);

			VK_CHECK_RESULT(vkEndCommandBuffer(command_buffer));

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &command_buffer;

			// Create fence to ensure that the command buffer has finished executing
			VkFenceCreateInfo fenceCreateInfo = {};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = 0;
			VkFence fence;
			VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));

			// Submit to the queue
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
			// Wait for the fence to signal that command buffer has finished executing
			VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

			vkDestroyFence(device, fence, nullptr);
			vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
		}

		inline VkShaderModule load_spirv(VkDevice &device, const std::string filename) {
			std::vector<char> shader_code;
			ct::load_binary(filename, shader_code);
			if (!shader_code.empty()) {
				// Create a new shader module that will be used for pipeline creation
				VkShaderModuleCreateInfo moduleCreateInfo{};
				moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				moduleCreateInfo.codeSize = shader_code.size();
				moduleCreateInfo.pCode = (uint32_t*)shader_code.data();

				VkShaderModule shaderModule;
				VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

				return shaderModule;
			} else
				ct::error::exit("Could not open file: " + filename, 1);
		}



	}
}
