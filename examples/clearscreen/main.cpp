#include <cmath>
#include <random>
#include <chrono>
#include <unordered_map>

#include <omp.h>

#include "vulkanbase/VulkanHelper.h"
#include "vulkanbase/SwapChainHelper.h"
#include "utils/ErrorHelper.h"

#if defined(VK_USE_PLATFORM_XCB_KHR)
#include "windowmanager/XCBWindowHelper.h"
#endif


#define WINDOW_TITLE "Dummy Clear Screen"
#define WINDOW_HEIGHT 960
#define WINDOW_WIDTH 1280


class ToyWorld {
public:

	void init(ct::vulkan::LogicalDevice &logical_device_, ct::vulkan::Framebuffer &framebuffer_) {
		logical_device = &logical_device_;
		framebuffer = &framebuffer_;

		build_command_buffer();
	}

	void build_command_buffer() {
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.pNext = nullptr;

		// Set clear values for all framebuffer attachments with loadOp set to clear
		// We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.3f, 0.3f, 0.5f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = framebuffer->render_pass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = framebuffer->width;
		renderPassBeginInfo.renderArea.extent.height = framebuffer->height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < logical_device->command_buffer.size(); ++i) {
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = framebuffer->framebuffer[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(logical_device->command_buffer[i], &cmdBufInfo));

			// Start the first sub pass specified in our default render pass setup by the base class
			// This will clear the color and depth attachment
			vkCmdBeginRenderPass(logical_device->command_buffer[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Actually do nothing

			// Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system
			vkCmdEndRenderPass(logical_device->command_buffer[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(logical_device->command_buffer[i]));
		}

	}

	void draw() {
    	// command buffer already build. So nothing to do here
    }

	void advance(std::size_t iteration_counter, double ms_per_frame) {
		// do nothing here as well
    }

private:
	ct::vulkan::LogicalDevice *logical_device;
	ct::vulkan::Framebuffer *framebuffer;

};



int main() {
	VkInstance vulkan_instance;
	ct::vulkan::Framebuffer framebuffer;
	ct::vulkan::LogicalDevice logical_device;
	ct::vulkan::Synchronization synchronization;
	ct::vulkan::swapchain::SwapChain swapchain;
	ct::windowmanager::xcb::Window window;

	ct::vulkan::create_instance(WINDOW_TITLE, vulkan_instance);
	#if defined(VK_USE_PLATFORM_XCB_KHR)
	ct::windowmanager::xcb::init(window);
	ct::windowmanager::xcb::setup_window(window, WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT);
	ct::windowmanager::xcb::init_surface(vulkan_instance, window);
	#endif
	ct::vulkan::search_and_pick_gpu(vulkan_instance, logical_device);
	ct::vulkan::create_device(logical_device);
	ct::vulkan::swapchain::connect(vulkan_instance, logical_device.device, swapchain);
	ct::vulkan::swapchain::check_present_support(logical_device.physical_device, window.surface, swapchain);
	ct::vulkan::swapchain::create(WINDOW_WIDTH, WINDOW_HEIGHT, true, logical_device.physical_device, logical_device.device, window.surface, swapchain);

	ct::vulkan::create_command_pool(logical_device.device, logical_device.queue_family_indices.graphics, logical_device.command_pool);
	ct::vulkan::create_queues(logical_device, logical_device.queue_graphics, logical_device.queue_compute);
	ct::vulkan::create_command_buffer(swapchain.imagecount, logical_device.device, logical_device.command_pool, logical_device.command_buffer);
	ct::vulkan::create_synchronization(logical_device.device, logical_device.command_buffer, synchronization);
	ct::vulkan::setup_depth_stencil(WINDOW_WIDTH, WINDOW_HEIGHT, logical_device.physical_device, logical_device.device, logical_device.memory_properties, framebuffer.depth_stencil);
	ct::vulkan::setup_render_pass(swapchain.color_format, framebuffer.depth_stencil.depth_format, logical_device.device, framebuffer.render_pass);
	//ct::vulkan::create_pipeline_cache(logical_device.device, pipeline.pipeline_cache);
	ct::vulkan::setup_framebuffer_from_swapchain(WINDOW_WIDTH, WINDOW_HEIGHT, swapchain.imagecount, logical_device.device, 
			swapchain.views, swapchain.color_format, swapchain.color_space, framebuffer);

    ToyWorld world;
	world.init(logical_device, framebuffer);


	window.is_alive = true;
	ct::windowmanager::xcb::flush(window.connection);
    std::chrono::high_resolution_clock clock;
    std::size_t iteration_counter = 0;
    std::chrono::high_resolution_clock::time_point t0 = clock.now();
    std::chrono::milliseconds mspf;
	while (window.is_alive) {
		auto tStart = std::chrono::high_resolution_clock::now();
		xcb_generic_event_t *event;
		while ((event = xcb_poll_for_event(window.connection))) {
			ct::windowmanager::xcb::handle_events(event, window);
			free(event);
		}
		world.advance(iteration_counter++, mspf.count());

		ct::vulkan::swapchain::acquire_next_image(logical_device.device, synchronization.present_complete, swapchain);
		world.draw();
		ct::vulkan::swapchain::render_and_swap(logical_device, swapchain, synchronization);

		mspf = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t0);
		t0 = clock.now();
		if (iteration_counter% 60 == 0) {
			std::cout << "ms_per_frame: " << mspf.count() << std::endl;
		}
	}
	if (logical_device.device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(logical_device.device);
	}


    return 0;
}
