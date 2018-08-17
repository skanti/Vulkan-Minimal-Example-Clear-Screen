#pragma once

#include <iostream>
#include <vector>
#include <cstring>

#include <xcb/xcb.h>
#include "utils/ErrorHelper.h"


namespace ct {
	namespace windowmanager {
		namespace xcb {

			struct Window {
				bool is_alive;
				VkSurfaceKHR surface;
				xcb_connection_t *connection;
				xcb_screen_t *screen;
				xcb_window_t window;
				xcb_intern_atom_reply_t *atom_wm_delete_window;
			};

			inline xcb_intern_atom_reply_t* intern_atom_helper(xcb_connection_t *conn, bool only_if_exists, const char *str) {
				xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(str), str);
				return xcb_intern_atom_reply(conn, cookie, NULL);
			}

			inline void init(Window &window) {
				const xcb_setup_t *setup;
				xcb_screen_iterator_t iter;
				int scr;

				window.connection = xcb_connect(NULL, &scr);
				if (window.connection == NULL) {
					ct::error::exit("Could not find a compatible ICD.", 1);
				}

				setup = xcb_get_setup(window.connection);
				iter = xcb_setup_roots_iterator(setup);
				while (scr-- > 0)
					xcb_screen_next(&iter);
				window.screen = iter.data;
			};

			inline void setup_window(Window &window, std::string title_window, int width_window, int height_window) {
				uint32_t value_mask;
				uint32_t value_list[32];

				window.window = xcb_generate_id(window.connection);

				value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
				value_list[0] = window.screen->black_pixel;
				value_list[1] =
					XCB_EVENT_MASK_KEY_RELEASE |
					XCB_EVENT_MASK_KEY_PRESS |
					XCB_EVENT_MASK_EXPOSURE |
					XCB_EVENT_MASK_STRUCTURE_NOTIFY |
					XCB_EVENT_MASK_POINTER_MOTION |
					XCB_EVENT_MASK_BUTTON_PRESS |
					XCB_EVENT_MASK_BUTTON_RELEASE;

				xcb_create_window(window.connection,
						XCB_COPY_FROM_PARENT,
						window.window, window.screen->root,
						0, 0, width_window, height_window, 0,
						XCB_WINDOW_CLASS_INPUT_OUTPUT,
						window.screen->root_visual,
						value_mask, value_list);


			xcb_intern_atom_reply_t* reply = intern_atom_helper(window.connection, true, "WM_PROTOCOLS");
			window.atom_wm_delete_window = intern_atom_helper(window.connection, false, "WM_DELETE_WINDOW");

			xcb_change_property(window.connection, XCB_PROP_MODE_REPLACE,
					window.window, (*reply).atom, 4, 32, 1,
					&(*window.atom_wm_delete_window).atom);

			xcb_change_property(window.connection, XCB_PROP_MODE_REPLACE,
					window.window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
					title_window.size(), title_window.c_str());
			free(reply);

			xcb_map_window(window.connection, window.window);
			}

			inline void init_surface(VkInstance instance, Window &window) {
				VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
				surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
				surfaceCreateInfo.connection = window.connection;
				surfaceCreateInfo.window = window.window;
				VK_CHECK_RESULT(vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &window.surface));
			}


			inline void flush(xcb_connection_t* connection) {
				xcb_flush(connection);
			};

			void handle_events(const xcb_generic_event_t *event, ct::windowmanager::xcb::Window &window) {
				switch (event->response_type & 0x7f) {
					case XCB_CLIENT_MESSAGE:
						if ((*(xcb_client_message_event_t*)event).data.data32[0] == (*window.atom_wm_delete_window).atom)
							window.is_alive = false;
						break;
				}
			};


		}
	}
}
