/*
 * Minimal libXi (X Input Extension 1.x) surface. There is no X server and no
 * extra input devices behind libx11-compat: the core pointer and keyboard are
 * delivered as ordinary core events, so device enumeration reports an empty
 * list, XOpenDevice yields NULL, and the remaining per-device calls are
 * inert. That is what toolkits probing for optional hardware
 * (spaceballs, dial boxes, tablets) expect, e.g. Open Inventor's SoXtSpaceball.
 */
#include <X11/Xlibint.h>
#include <X11/extensions/XInput.h>
#include <stdlib.h>

XExtensionVersion *XGetExtensionVersion(Display *dpy, _Xconst char *name)
{
    (void) dpy;
    (void) name;
    XExtensionVersion *ver = calloc(1, sizeof(*ver));
    if (ver)
        ver->present = False;
    return ver;
}

XDeviceInfo *XListInputDevices(Display *dpy, int *ndevices)
{
    (void) dpy;
    if (ndevices)
        *ndevices = 0;
    return NULL;
}

void XFreeDeviceList(XDeviceInfo *list)
{
    free(list);
}

XDevice *XOpenDevice(Display *dpy, XID id)
{
    (void) dpy;
    (void) id;
    return NULL;
}

int XCloseDevice(Display *dpy, XDevice *device)
{
    (void) dpy;
    (void) device;
    return Success;
}

int XSetDeviceValuators(Display *dpy,
                        XDevice *device,
                        int *valuators,
                        int first_valuator,
                        int num_valuators)
{
    (void) dpy;
    (void) device;
    (void) valuators;
    (void) first_valuator;
    (void) num_valuators;
    return BadRequest;
}

int XSelectExtensionEvent(Display *dpy,
                          Window w,
                          XEventClass *event_list,
                          int count)
{
    (void) dpy;
    (void) w;
    (void) event_list;
    (void) count;
    return Success;
}
