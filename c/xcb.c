#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>

#define NS_PER_S (1000 * 1000 * 1000)

typedef struct timespec Instant;
typedef struct timespec Duration;

Duration instant_sub(Instant x, Instant y) {
    Duration d;
    if (x.tv_nsec < y.tv_nsec) {
        d.tv_nsec = NS_PER_S + x.tv_nsec - y.tv_nsec;
        if ((x.tv_sec - 1) < y.tv_sec) {
            abort();
        } else {
            d.tv_sec = x.tv_sec - 1 - y.tv_sec;
        }
    } else {
        d.tv_nsec = x.tv_nsec - y.tv_nsec;
        if (x.tv_sec < y.tv_sec) {
            abort();
        } else {
            d.tv_sec = x.tv_sec - y.tv_sec;
        }
    }
    return d;
}

Instant time_now(void) {
    Instant i;
    int r = clock_gettime(CLOCK_MONOTONIC, &i);
    if (r == -1) {
        // Time has stopped existing.
        abort();
    } else {
        return i;
    }
}

Duration time_elapsed(Instant epoch) { return instant_sub(time_now(), epoch); }

uint64_t duration_nsec(Duration duration) {
    return duration.tv_sec * NS_PER_S + duration.tv_nsec;
}

int main() {
    xcb_connection_t *c = xcb_connect(0, 0);
    xcb_screen_t *s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_window_t w = xcb_generate_id(c);
    const uint32_t values = XCB_EVENT_MASK_EXPOSURE;
    xcb_create_window(c, 0, w, (*s).root, 0, 0, 128, 128, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, (*s).root_visual,
                      XCB_CW_EVENT_MASK, &values);

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(c, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(c, cookie, 0);

    xcb_intern_atom_cookie_t cookie2 =
        xcb_intern_atom(c, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *reply2 = xcb_intern_atom_reply(c, cookie2, 0);

    xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, (*reply).atom, 4, 32, 1,
                        &(*reply2).atom);

    xcb_map_window(c, w);
    xcb_flush(c);

    uint8_t keep_running = 1;
    Instant frame_start = time_now();
    while (keep_running) {
        xcb_generic_event_t *event;
        while ((event = xcb_poll_for_event(c))) {
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
        Instant frame_end = time_now();
        Duration frame_duration = instant_sub(frame_end, frame_start);
        frame_start = frame_end;
        printf("frame time %08" PRIu64 "ns\n", duration_nsec(frame_duration));
    }

    return 0;
}
