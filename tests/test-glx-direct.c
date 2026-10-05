/*
 * Direct desktop-GL path: Xlib + glX* from libx11-compat, gl* from GLVND's
 * libOpenGL (gl-only, no glX*), rendering on the system Mesa desktop context
 * that src/glx.c creates (CTX_API_AUTO -> EGL_OPENGL_API). No gl4es, no
 * libGL.so.1/libGLX in the process, so the legacy GL 1.x surface classic
 * scene-graph clients rely on (Open Inventor's picking, display lists, attrib
 * stack) runs on a real compatibility-profile implementation.
 *
 * Checks, each fatal: the context is desktop GL (not GLES), GL_SELECT returns
 * the pushed name, GL_FEEDBACK emits a polygon token, and a display list drawn
 * inside glPushAttrib/glPopAttrib lands non-background pixels.
 *
 * --once exits after the checks; without it the scene keeps redrawing so
 * scripts/glx-snapshot.sh can capture a frame.
 */
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SIZE 256

#define CHECK(cond, msg)                                               \
    do {                                                               \
        if (!(cond)) {                                                 \
            fprintf(stderr, "test-glx-direct FAIL: %s (%s:%d)\n", msg, \
                    __FILE__, __LINE__);                               \
            exit(1);                                                   \
        }                                                              \
    } while (0)

static void drawScene(void)
{
    glBegin(GL_TRIANGLES);
    glColor3f(1, 0, 0);
    glVertex2f(-0.8f, -0.8f);
    glColor3f(0, 1, 0);
    glVertex2f(0.8f, -0.8f);
    glColor3f(0, 0, 1);
    glVertex2f(0.0f, 0.8f);
    glEnd();
}

static void clearBackground(void)
{
    glClearColor(0.2f, 0.2f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

int main(int argc, char **argv)
{
    int once = argc > 1 && strcmp(argv[1], "--once") == 0;

    Display *dpy = XOpenDisplay(NULL);
    CHECK(dpy, "XOpenDisplay");
    int attrs[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 16, None};
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), attrs);
    CHECK(vi, "glXChooseVisual (no EGL provider?)");

    Window root = RootWindow(dpy, vi->screen);
    XSetWindowAttributes swa = {0};
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = ExposureMask | StructureNotifyMask;
    Window win =
        XCreateWindow(dpy, root, 0, 0, SIZE, SIZE, 0, vi->depth, InputOutput,
                      vi->visual, CWColormap | CWEventMask, &swa);
    XStoreName(dpy, win, "test-glx-direct");
    XMapWindow(dpy, win);

    GLXContext ctx = glXCreateContext(dpy, vi, NULL, True);
    CHECK(ctx, "glXCreateContext");
    CHECK(glXMakeCurrent(dpy, win, ctx), "glXMakeCurrent");

    const char *vendor = (const char *) glGetString(GL_VENDOR);
    const char *renderer = (const char *) glGetString(GL_RENDERER);
    const char *version = (const char *) glGetString(GL_VERSION);
    CHECK(version, "glGetString(GL_VERSION) (gl* not bound to the context?)");
    printf("GL_VENDOR   = %s\nGL_RENDERER = %s\nGL_VERSION  = %s\n",
           vendor ? vendor : "?", renderer ? renderer : "?", version);
    CHECK(strncmp(version, "OpenGL ES", 9) != 0,
          "context is GLES, expected desktop GL");
    CHECK(!renderer || !strstr(renderer, "gl4es"),
          "renderer is gl4es, expected the direct path");
    glViewport(0, 0, SIZE, SIZE);

    GLuint sel[64];
    glSelectBuffer(64, sel);
    glRenderMode(GL_SELECT);
    glInitNames();
    glPushName(42);
    drawScene();
    GLint hits = glRenderMode(GL_RENDER);
    printf("GL_SELECT   hits=%d name=%u\n", hits, hits > 0 ? sel[3] : 0);
    CHECK(hits == 1 && sel[0] == 1 && sel[3] == 42, "GL_SELECT hit record");

    GLfloat fb[256];
    glFeedbackBuffer(256, GL_2D, fb);
    glRenderMode(GL_FEEDBACK);
    drawScene();
    GLint nfb = glRenderMode(GL_RENDER);
    printf("GL_FEEDBACK values=%d\n", nfb);
    CHECK(nfb > 0 && fb[0] == GL_POLYGON_TOKEN, "GL_FEEDBACK polygon token");

    GLuint list = glGenLists(1);
    CHECK(list != 0, "glGenLists");
    glNewList(list, GL_COMPILE);
    drawScene();
    glEndList();

    glClearColor(0, 0, 0, 1);
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    clearBackground();
    glCallList(list);
    glPopAttrib();
    GLfloat restored[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, restored);
    CHECK(restored[0] == 0 && restored[3] == 1, "glPopAttrib clear color");

    unsigned char px[4] = {0};
    glReadPixels(SIZE / 2, SIZE / 2 - 28, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    printf("triangle px = %u,%u,%u\n", px[0], px[1], px[2]);
    CHECK(!(px[0] == 51 && px[1] == 51 && px[2] == 51),
          "display list drew nothing over the background");
    CHECK(glGetError() == GL_NO_ERROR, "glGetError");
    fflush(stdout);

    if (once) {
        glXMakeCurrent(dpy, None, NULL);
        glXDestroyContext(dpy, ctx);
        XCloseDisplay(dpy);
        printf("test-glx-direct: OK\n");
        return 0;
    }

    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
        }
        clearBackground();
        glCallList(list);
        glXSwapBuffers(dpy, win);
        usleep(16000);
    }
}
