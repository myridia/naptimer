#include "idle.h"

#include <stdlib.h>

#if defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

#if defined(__linux__)

typedef struct _XDisplay Display;
typedef unsigned long XDrawable;
typedef struct
{
    XDrawable window;
    int state;
    int kind;
    unsigned long till;
    unsigned long idle;
    unsigned long event_mask;
} NtXScreenSaverInfo;

typedef Display *(*NtXOpenDisplay)(const char *);
typedef int (*NtXCloseDisplay)(Display *);
typedef XDrawable (*NtXRootWindow)(Display *, int);
typedef NtXScreenSaverInfo *(*NtXAllocInfo)(void);
typedef int (*NtXQueryInfo)(Display *, XDrawable, NtXScreenSaverInfo *);
typedef int (*NtXFree)(void *);

typedef struct
{
    Display *display;
    NtXCloseDisplay close_display;
    NtXRootWindow root_window;
    NtXAllocInfo alloc_info;
    NtXQueryInfo query_info;
    NtXFree xfree;
    int initialized;
} X11Idle;

static X11Idle i_x11;

static void i_init_x11(void)
{
    void *x11;
    void *xss;

    if (i_x11.initialized)
        return;
    i_x11.initialized = 1;
    x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    xss = dlopen("libXss.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (x11 == NULL || xss == NULL)
        return;
    i_x11.close_display = (NtXCloseDisplay)dlsym(x11, "XCloseDisplay");
    i_x11.root_window = (NtXRootWindow)dlsym(x11, "XRootWindow");
    i_x11.alloc_info = (NtXAllocInfo)dlsym(xss, "XScreenSaverAllocInfo");
    i_x11.query_info = (NtXQueryInfo)dlsym(xss, "XScreenSaverQueryInfo");
    i_x11.xfree = (NtXFree)dlsym(x11, "XFree");
    if (i_x11.close_display == NULL || i_x11.root_window == NULL ||
        i_x11.alloc_info == NULL || i_x11.query_info == NULL || i_x11.xfree == NULL)
        return;
    {
        NtXOpenDisplay open_display = (NtXOpenDisplay)dlsym(x11, "XOpenDisplay");
        if (open_display == NULL)
            return;
        i_x11.display = open_display(NULL);
    }
}

#endif

int nt_idle_available(void)
{
#if defined(__APPLE__)
    return 1;
#elif defined(_WIN32)
    return 1;
#elif defined(__linux__)
    const char *wayland = getenv("WAYLAND_DISPLAY");
    if (wayland != NULL && wayland[0] != '\0' && getenv("DISPLAY") == NULL)
        return 0;
    i_init_x11();
    return i_x11.display != NULL;
#else
    return 0;
#endif
}

int64_t nt_idle_seconds(void)
{
#if defined(__APPLE__)
    /* kCGAnyInputEventType is not exposed by newer SDKs.  The documented
       raw value for "any input event" is ~0. */
    double seconds = CGEventSourceSecondsSinceLastEventType(
        kCGEventSourceStateCombinedSessionState, (CGEventType)0xffffffffU);
    if (seconds < 0 || seconds > 86400.0 * 30)
        return -1;
    return (int64_t)seconds;
#elif defined(_WIN32)
    LASTINPUTINFO info;
    info.cbSize = sizeof(info);
    if (GetLastInputInfo(&info) == 0)
        return -1;
    return (int64_t)(GetTickCount() - info.dwTime) / 1000;
#elif defined(__linux__)
    NtXScreenSaverInfo *info;
    int result;
    int64_t seconds;

    i_init_x11();
    if (i_x11.display == NULL)
        return -1;
    info = i_x11.alloc_info();
    if (info == NULL)
        return -1;
    result = i_x11.query_info(i_x11.display,
                              i_x11.root_window(i_x11.display, 0), info);
    seconds = result != 0 ? (int64_t)(info->idle / 1000) : -1;
    i_x11.xfree(info);
    return seconds;
#else
    return -1;
#endif
}
