#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// -lX11
#include <X11/Xlib.h>
// -lX11-xcb
#include <X11/Xlib-xcb.h>
// -lxcb
#include <xcb/xcb.h>
// -lGL
#define GLX_GLXEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glx.h>

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

/*
  Attribs filter the list of FBConfigs returned by glXChooseFBConfig().
  Visual attribs further described in glXGetFBConfigAttrib(3)
*/
static int visual_attribs[] = {
    //
    GLX_X_RENDERABLE, True,
    //
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    //
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    //
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    //
    GLX_RED_SIZE, 8,
    //
    GLX_GREEN_SIZE, 8,
    //
    GLX_BLUE_SIZE, 8,
    //
    GLX_ALPHA_SIZE, 8,
    //
    GLX_DEPTH_SIZE, 24,
    //
    GLX_STENCIL_SIZE, 8,
    //
    GLX_DOUBLEBUFFER, True,
    // GLX_SAMPLE_BUFFERS  , 1,
    // GLX_SAMPLES         , 4,
    None};

static Atom wm_protocols;
static Atom wm_delete_window;

void draw() {
    glClearColor(0.2, 0.4, 0.9, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

#define WM_PROTOCOLS "WM_PROTOCOLS"
#define WM_DELETE_WINDOW "WM_DELETE_WINDOW"

int main_loop(Display *display, xcb_connection_t *connection,
              xcb_window_t window, GLXDrawable drawable) {
    int running = 1;
    Instant frame_start = time_now();

    while (running) {
        // Update
        Instant update_start = time_now();
        xcb_generic_event_t *event;
        while ((event = xcb_poll_for_event(connection))) {
            switch (event->response_type & ~0x80) {
            case XCB_KEY_PRESS:
                /* Quit on key press */
                running = 0;
                break;
            case XCB_EXPOSE:
                // We are drawing continuously.
                break;
            case XCB_CLIENT_MESSAGE: {
                printf("xcb_client_message\n");
                abort();
                running = 0;
                xcb_client_message_event_t *e =
                    (xcb_client_message_event_t *)event;
                if (e->data.data32[0] == wm_delete_window) {
                    running = 0;
                }
                break;
            }
            default:
                printf("event %d\n", event->response_type);
                abort();
                break;
            }

            free(event);
        }
        Duration update_elapsed = time_elapsed(update_start);

        // Draw
        Instant draw_start = time_now();
        draw();
        Duration draw_elapsed = time_elapsed(draw_start);

        glXSwapBuffers(display, drawable);

        Instant frame_end = time_now();
        Duration frame_elapsed = instant_sub(frame_end, frame_start);
        frame_start = frame_end;

        if (duration_nsec(frame_elapsed) < 10000) {
            // NOTE(mick): I can't figure out how to get a close event and
            // properly stop the loop when the close button on the window is
            // clicked. If the frame is suddenly super short it is probably not
            // actually updating and drawing anymore.
            /* abort(); */
        }

        printf("frame: %8luns, update: %8luns, draw: %8luns\n",
               duration_nsec(frame_elapsed), duration_nsec(update_elapsed),
               duration_nsec(draw_elapsed));
    }

    return 0;
}

int setup_and_run(Display *display, xcb_connection_t *connection,
                  int default_screen, xcb_screen_t *screen) {
    int visualID = 0;

    /* Query framebuffer configurations that match visual_attribs */
    GLXFBConfig *fb_configs = 0;
    int num_fb_configs = 0;
    fb_configs = glXChooseFBConfig(display, default_screen, visual_attribs,
                                   &num_fb_configs);
    if (!fb_configs || num_fb_configs == 0) {
        fprintf(stderr, "glXGetFBConfigs failed\n");
        return -1;
    }

    printf("Found %d matching FB configs\n", num_fb_configs);

    /* Select first framebuffer config and query visualID */
    GLXFBConfig fb_config = fb_configs[0];
    glXGetFBConfigAttrib(display, fb_config, GLX_VISUAL_ID, &visualID);

    GLXContext context;

    /* Create OpenGL context */
    context = glXCreateNewContext(display, fb_config, GLX_RGBA_TYPE, 0, True);
    if (!context) {
        fprintf(stderr, "glXCreateNewContext failed\n");
        return -1;
    }

    /* Create XID's for colormap and window */
    xcb_colormap_t colormap = xcb_generate_id(connection);
    xcb_window_t window = xcb_generate_id(connection);

    /* Create colormap */
    xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap,
                        screen->root, visualID);

    /* Create window */
    uint32_t eventmask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS |
                         XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                         XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
    uint32_t valuelist[] = {eventmask, colormap, 0};
    uint32_t valuemask = XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;

    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0,
                      0, 150, 150, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visualID,
                      valuemask, valuelist);

    // NOTE(mick): Don't know if this'll work.
    {
        Atom protocols[] = {wm_delete_window};
        XSetWMProtocols(display, window, protocols,
                        sizeof(protocols) / sizeof(Atom));
    }

    // NOTE: window must be mapped before glXMakeContextCurrent
    xcb_map_window(connection, window);

    /* Create GLX Window */
    GLXDrawable drawable = 0;

    GLXWindow glxwindow = glXCreateWindow(display, fb_config, window, 0);

    if (!window) {
        xcb_destroy_window(connection, window);
        glXDestroyContext(display, context);

        fprintf(stderr, "glXDestroyContext failed\n");
        return -1;
    }

    drawable = glxwindow;

    {
        uint32_t interval;
        glXQueryDrawable(display, drawable, GLX_SWAP_INTERVAL_EXT, &interval);
        printf("VSYNC interval: %d\n", interval);
    }

    {
        uint32_t interval;
        glXQueryDrawable(display, drawable, GLX_MAX_SWAP_INTERVAL_EXT,
                         &interval);
        printf("VSYNC max interval: %d\n", interval);
    }

    glXSwapIntervalEXT(display, drawable, 1);

    /* make OpenGL context current */
    if (!glXMakeContextCurrent(display, drawable, drawable, context)) {
        xcb_destroy_window(connection, window);
        glXDestroyContext(display, context);

        fprintf(stderr, "glXMakeContextCurrent failed\n");
        return -1;
    }

    /* run main loop */
    int retval = main_loop(display, connection, window, drawable);

    /* Cleanup */
    glXDestroyWindow(display, glxwindow);

    xcb_destroy_window(connection, window);

    glXDestroyContext(display, context);

    return retval;
}

int main(int argc, char *argv[]) {
    Display *display;
    int default_screen;

    /* Open Xlib Display */
    display = XOpenDisplay(0);
    if (!display) {
        fprintf(stderr, "Can't open display\n");
        return -1;
    }

    default_screen = DefaultScreen(display);

    /* Get the XCB connection from the display */
    xcb_connection_t *connection = XGetXCBConnection(display);
    if (!connection) {
        XCloseDisplay(display);
        fprintf(stderr, "Can't get xcb connection from display\n");
        return -1;
    }

    /* Acquire event queue ownership */
    XSetEventQueueOwner(display, XCBOwnsEventQueue);

    {
        xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(
            connection, False, sizeof(WM_PROTOCOLS), WM_PROTOCOLS);
        xcb_intern_atom_cookie_t wm_delete_window_cookie = xcb_intern_atom(
            connection, False, sizeof(WM_DELETE_WINDOW), WM_DELETE_WINDOW);

        xcb_intern_atom_reply_t *wm_protocols_reply =
            xcb_intern_atom_reply(connection, wm_protocols_cookie, NULL);
        wm_protocols = wm_protocols_reply->atom;
        free(wm_protocols_reply);

        xcb_intern_atom_reply_t *wm_delete_window_reply =
            xcb_intern_atom_reply(connection, wm_delete_window_cookie, NULL);
        wm_delete_window = wm_delete_window_reply->atom;
        free(wm_delete_window_reply);
    }

    /* Find XCB screen */
    xcb_screen_t *screen = 0;
    xcb_screen_iterator_t screen_iter =
        xcb_setup_roots_iterator(xcb_get_setup(connection));
    for (int screen_num = default_screen; screen_iter.rem && screen_num > 0;
         --screen_num, xcb_screen_next(&screen_iter))
        ;
    screen = screen_iter.data;

    /* Initialize window and OpenGL context, run main loop and deinitialize */
    int retval = setup_and_run(display, connection, default_screen, screen);

    /* Cleanup */
    XCloseDisplay(display);

    return retval;
}
