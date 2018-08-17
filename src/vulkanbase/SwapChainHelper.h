#pragma once

#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>
#include "vulkanbase/VulkanHelper.h"
#include "vulkanbase/VulkanStrings.h"
#include "utils/ErrorHelper.h"


namespace ct {
	namespace vulkan {
		namespace swapchain {

			struct SwapChain {
				uint32_t current_buffer = 0;

				VkSwapchainKHR swapchain = VK_NULL_HANDLE;
				VkInstance instance;

				VkFormat color_format;
				VkColorSpaceKHR color_space;
				
				uint32_t imagecount;

				std::vector<VkImage> images;
				std::vector<VkImageView> views;

				PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
				PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
				PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
				PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
				PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
				PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
				PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
				PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
				PFN_vkQueuePresentKHR fpQueuePresentKHR;
			};


			// Macro to get a procedure address based on a vulkan instance
#define GET_INSTANCE_PROC_ADDR(inst, entrypoint, container)\
			{\
				container.fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetInstanceProcAddr(inst, "vk"#entrypoint));\
				if (container.fp##entrypoint == NULL) {\
					ct::error::exit("Swapchain (instance) function entry point is NULL.", 1);\
				}\
			}\


			// Macro to get a procedure address based on a vulkan device
#define GET_DEVICE_PROC_ADDR(dev, entrypoint, container)\
			{\
				container.fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetDeviceProcAddr(dev, "vk"#entrypoint));\
				if (container.fp##entrypoint == NULL) {\
					ct::error::exit("Swapchain (device) function entry point is NULL.", 1);\
				}\
			}\

			inline void connect(VkInstance &instance, VkDevice &device, ct::vulkan::swapchain::SwapChain &swapchain) {
				GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceSupportKHR, swapchain);
				GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceCapabilitiesKHR, swapchain);
				GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceFormatsKHR, swapchain);
				GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfacePresentModesKHR, swapchain);
				GET_DEVICE_PROC_ADDR(device, CreateSwapchainKHR, swapchain);
				GET_DEVICE_PROC_ADDR(device, DestroySwapchainKHR, swapchain);
				GET_DEVICE_PROC_ADDR(device, GetSwapchainImagesKHR, swapchain);
				GET_DEVICE_PROC_ADDR(device, AcquireNextImageKHR, swapchain);
				GET_DEVICE_PROC_ADDR(device, QueuePresentKHR, swapchain);
			}

			inline void check_present_support(VkPhysicalDevice &physical_device, VkSurfaceKHR &surface, ct::vulkan::swapchain::SwapChain &swapchain) {
				uint32_t queueCount;
				vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueCount, NULL);
				assert(queueCount >= 1);
				std::cout << "n-queue-families: " << queueCount << std::endl;

				std::vector<VkQueueFamilyProperties> queueProps(queueCount);
				vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueCount, queueProps.data());

				std::vector<VkBool32> supportsPresent(queueCount);
				for (uint32_t i = 0; i < queueCount; i++)
					swapchain.fpGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supportsPresent[i]);

				uint32_t graphicsQueueNodeIndex = UINT32_MAX;
				uint32_t presentQueueNodeIndex = UINT32_MAX;
				for (uint32_t i = 0; i < queueCount; i++) {
					if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
						if (graphicsQueueNodeIndex == UINT32_MAX)
							graphicsQueueNodeIndex = i;

					if (supportsPresent[i] == VK_TRUE) {
						graphicsQueueNodeIndex = i;
						presentQueueNodeIndex = i;
						break;
					}
				}

				if (presentQueueNodeIndex == UINT32_MAX)
					for (uint32_t i = 0; i < queueCount; ++i)
						if (supportsPresent[i] == VK_TRUE) {
							presentQueueNodeIndex = i;
							break;
						}

				// Exit if either a graphics or a presenting queue hasn't been found
				if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
					ct::error::exit("Could not find a graphics and/or presenting queue!", -1);

				// todo : Add support for separate graphics and presenting queue
				if (graphicsQueueNodeIndex != presentQueueNodeIndex)
					ct::error::exit("Separate graphics and presenting queues are not supported yet!", -1);

				// Get list of supported surface formats
				uint32_t formatCount;
				VK_CHECK_RESULT(swapchain.fpGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, NULL));
				assert(formatCount > 0);

				std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
				VK_CHECK_RESULT(swapchain.fpGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, surfaceFormats.data()));


				// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
				// there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
				std::cout << "n_colorformat: " << formatCount << std::endl;
				if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED)) {
					swapchain.color_format = VK_FORMAT_B8G8R8A8_UNORM;
					swapchain.color_space = surfaceFormats[0].colorSpace;
					printf("selected-color-format: %s, default-color-format: %s\n", "VK_FORMAT_UNDEFINED", "VK_FORMAT_B8G8R8A8_UNORM");
				} else {
					// iterate over the list of available surface format and
					// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
					bool found_B8G8R8A8_UNORM = false;
					for (auto&& surfaceFormat : surfaceFormats) {
						if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM) {
							swapchain.color_format = surfaceFormat.format;
							swapchain.color_space = surfaceFormat.colorSpace;
							found_B8G8R8A8_UNORM = true;
							printf("selected-color-format: %s\n", "VK_FORMAT_B8G8R8A8_UNORM");
							break;
						}
					}

					// in case VK_FORMAT_B8G8R8A8_UNORM is not available
					// select the first available color format
					if (!found_B8G8R8A8_UNORM) {
						printf("selected-color-format: %s\n", "NOT VK_FORMAT_B8G8R8A8_UNORM");
						swapchain.color_format = surfaceFormats[0].format;
						swapchain.color_space = surfaceFormats[0].colorSpace;
					}
				}
			}

			inline void create(uint32_t width, uint32_t height, bool is_vsync, 
					VkPhysicalDevice &physical_device, VkDevice &device, VkSurfaceKHR &surface, SwapChain &swapchain) {

				VkSwapchainKHR oldSwapchain = swapchain.swapchain;

				// Get physical device surface properties and formats
				VkSurfaceCapabilitiesKHR surfCaps;
				VK_CHECK_RESULT(swapchain.fpGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surfCaps));

				// Get available present modes
				uint32_t presentModeCount;
				VK_CHECK_RESULT(swapchain.fpGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &presentModeCount, NULL));
				assert(presentModeCount > 0);

				std::vector<VkPresentModeKHR> presentModes(presentModeCount);
				VK_CHECK_RESULT(swapchain.fpGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &presentModeCount, presentModes.data()));

				VkExtent2D swapchainExtent = {};
				swapchainExtent.width = width;
				swapchainExtent.height = height;

				VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

				uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
				if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
					desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
				std::cout << "desiredNumberOfSwapchainImages: " << desiredNumberOfSwapchainImages << std::endl;

				VkSurfaceTransformFlagsKHR preTransform;
				if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
					preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
				else
					preTransform = surfCaps.currentTransform;

				// Find a supported composite alpha format (not all devices support alpha opaque)
				VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				// Simply select the first composite alpha format available
				std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
					VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
					VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
					VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
					VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
				};
				for (auto& compositeAlphaFlag : compositeAlphaFlags) {
					if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
						compositeAlpha = compositeAlphaFlag;
						break;
					};
				}


				VkSwapchainCreateInfoKHR swapchainCI = {};
				swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
				swapchainCI.pNext = NULL;
				swapchainCI.surface = surface;
				swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
				swapchainCI.imageFormat = swapchain.color_format;
				swapchainCI.imageColorSpace = swapchain.color_space;
				swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
				swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
				swapchainCI.imageArrayLayers = 1;
				swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				swapchainCI.queueFamilyIndexCount = 0;
				swapchainCI.pQueueFamilyIndices = NULL;
				swapchainCI.presentMode = swapchainPresentMode;
				swapchainCI.oldSwapchain = oldSwapchain;
				// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
				swapchainCI.clipped = VK_TRUE;
				swapchainCI.compositeAlpha = compositeAlpha;


				// Enable transfer source on swap chain images if supported
				if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
					swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

				// Enable transfer destination on swap chain images if supported
				if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
					swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

				VK_CHECK_RESULT(swapchain.fpCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain.swapchain));


				VK_CHECK_RESULT(swapchain.fpGetSwapchainImagesKHR(device, swapchain.swapchain, &swapchain.imagecount, NULL));

				// Get the swap chain images
				swapchain.images.resize(swapchain.imagecount);
				swapchain.views.resize(swapchain.imagecount);
				VK_CHECK_RESULT(swapchain.fpGetSwapchainImagesKHR(device, swapchain.swapchain, &swapchain.imagecount, swapchain.images.data()));

				// Get the swap chain buffers containing the image and imageview
				std::cout << "n-swapchain-images: " << swapchain.imagecount << std::endl; 
				for (uint32_t i = 0; i < swapchain.imagecount; i++) {
					VkImageViewCreateInfo colorAttachmentView = {};
					colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					colorAttachmentView.pNext = NULL;
					colorAttachmentView.format = swapchain.color_format;
					colorAttachmentView.components = {
						VK_COMPONENT_SWIZZLE_R,
						VK_COMPONENT_SWIZZLE_G,
						VK_COMPONENT_SWIZZLE_B,
						VK_COMPONENT_SWIZZLE_A
					};
					colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					colorAttachmentView.subresourceRange.baseMipLevel = 0;
					colorAttachmentView.subresourceRange.levelCount = 1;
					colorAttachmentView.subresourceRange.baseArrayLayer = 0;
					colorAttachmentView.subresourceRange.layerCount = 1;
					colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
					colorAttachmentView.flags = 0;

					colorAttachmentView.image = swapchain.images[i];

					VK_CHECK_RESULT(vkCreateImageView(device, &colorAttachmentView, nullptr, &swapchain.views[i]));
				}
			}

			inline void acquire_next_image(VkDevice &device, VkSemaphore &present_complete, ct::vulkan::swapchain::SwapChain &swapchain) {
				VK_CHECK_RESULT(swapchain.fpAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, present_complete, (VkFence)nullptr, &swapchain.current_buffer));
			}
			

			inline void render_and_swap(ct::vulkan::LogicalDevice &logical_device, ct::vulkan::swapchain::SwapChain &swapchain, ct::vulkan::Synchronization &synchronization) {
				// Use a fence to wait until the command buffer has finished execution before using it again
				VK_CHECK_RESULT(vkWaitForFences(logical_device.device, 1, &synchronization.wait_fences[swapchain.current_buffer], VK_TRUE, UINT64_MAX));
				VK_CHECK_RESULT(vkResetFences(logical_device.device, 1, &synchronization.wait_fences[swapchain.current_buffer]));

				// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
				VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				// The submit info structure specifices a command buffer queue submission batch
				VkSubmitInfo submitInfo = {};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
				submitInfo.pWaitSemaphores = &synchronization.present_complete;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
				submitInfo.waitSemaphoreCount = 1;												// One wait semaphore
				submitInfo.pSignalSemaphores = &synchronization.render_complete;						// Semaphore(s) to be signaled when command buffers have completed
				submitInfo.signalSemaphoreCount = 1;											// One signal semaphore
				submitInfo.pCommandBuffers = &logical_device.command_buffer[swapchain.current_buffer];					// Command buffers(s) to execute in this batch (submission)
				submitInfo.commandBufferCount = 1;												// One command buffer

				// Submit to the graphics queue passing a wait fence
				VK_CHECK_RESULT(vkQueueSubmit(logical_device.queue_graphics, 1, &submitInfo, synchronization.wait_fences[swapchain.current_buffer]));
				
				VkPresentInfoKHR presentInfo = {};
				presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				presentInfo.pNext = NULL;
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = &swapchain.swapchain;
				presentInfo.pImageIndices = &swapchain.current_buffer;
				// Check if a wait semaphore has been specified to wait for before presenting the image
				if (synchronization.render_complete != VK_NULL_HANDLE) {
					presentInfo.pWaitSemaphores = &synchronization.render_complete;
					presentInfo.waitSemaphoreCount = 1;
				} 
				
				VK_CHECK_RESULT(swapchain.fpQueuePresentKHR(logical_device.queue_graphics, &presentInfo));

			}



		}
	}
}

