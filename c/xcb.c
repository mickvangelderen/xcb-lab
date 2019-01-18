#include <stdint.h>
#include <stdio.h>
#include <xcb/xcb.h>

int main() {
    xcb_connection_t *c = xcb_connect(0, 0);
    xcb_screen_t *s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_window_t w = xcb_generate_id(c);
    const uint32_t values = XCB_EVENT_MASK_EXPOSURE;
    xcb_create_window(c, 0, w, (*s).root, 0, 0, 128, 128, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, (*s).root_visual,
                      XCB_CW_EVENT_MASK, &values);

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(c, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t cookie2 =
        xcb_intern_atom(c, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(c, cookie, 0);
    xcb_intern_atom_reply_t *reply2 = xcb_intern_atom_reply(c, cookie2, 0);

    xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, (*reply).atom, 4, 32, 1,
                        &(*reply2).atom);

    xcb_map_window(c, w);
    /* xcb_flush(c); */

    uint8_t keep_running = 1;
    while (keep_running) {
        xcb_generic_event_t *event;
        while ((event = xcb_wait_for_event(c))) {
            puts("Event occurred");
            switch ((*event).response_type & ~0x80) {
            case XCB_EXPOSE:
                puts("Expose");
                break;
            case XCB_CLIENT_MESSAGE: {
                puts("Client Message");
                if ((*(xcb_client_message_event_t *)event).data.data32[0] ==
                    (*reply2).atom) {
                    puts("Kill client");
                    keep_running = 0;
                }
                break;
            }
            }
        }
    }

    return 0;
}
