#include <sys/wait.h>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* --- Configuration --- */
#define MODMASK Mod4Mask
#define NUM_WS  9

static const char *termcmd[] = { "st", NULL };
static const char *menucmd[] = { "dmenu_run", NULL };

typedef union { int i; float f; const char **v; } Arg;
typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const Arg *);
    Arg arg;
} Key;

static void focusnext(const Arg *arg);
static void focusprev(const Arg *arg);
static void killclient(const Arg *arg);
static void movetows(const Arg *arg);
static void quit(const Arg *arg);
static void setmfact(const Arg *arg);
static void spawn(const Arg *arg);
static void togglefullscreen(const Arg *arg);
static void view(const Arg *arg);

static Key keys[] = {
    { MODMASK,             XK_Return, spawn,            {.v = termcmd} },
    { MODMASK,             XK_space,  spawn,            {.v = menucmd} },
    { MODMASK,             XK_j,      focusnext,        {0} },
    { MODMASK,             XK_k,      focusprev,        {0} },
    { MODMASK,             XK_h,      setmfact,         {.f = -0.05} },
    { MODMASK,             XK_l,      setmfact,         {.f = +0.05} },
    { MODMASK,             XK_q,      killclient,       {0} },
    { MODMASK|ShiftMask,   XK_q,      quit,             {0} },
    { MODMASK,             XK_f,      togglefullscreen, {0} },
    { MODMASK,             XK_1,      view,             {.i = 0} },
    { MODMASK,             XK_2,      view,             {.i = 1} },
    { MODMASK,             XK_3,      view,             {.i = 2} },
    { MODMASK|ShiftMask,   XK_1,      movetows,         {.i = 0} },
    { MODMASK|ShiftMask,   XK_2,      movetows,         {.i = 1} },
    { MODMASK|ShiftMask,   XK_3,      movetows,         {.i = 2} },
};

/* --- State & Structures --- */
typedef struct Client Client;
struct Client {
    Window win;
    int ws;
    int isfullscreen;
    Client *next;
};

static Display *dpy;
static Window root;
static int sw, sh, running = 1, curws = 0;
static float mfact = 0.5;
static Client *clients = NULL, *sel = NULL;

/* --- Core Functionality --- */
static void
arrange(void)
{
    Client *c;
    int n = 0, i = 0, mw, bh;

    for (c = clients; c; c = c->next)
        if (c->ws == curws) n++;

    mw = (n <= 1) ? sw : sw * mfact;

    for (c = clients; c; c = c->next) {
        if (c->ws != curws) {
            XMoveWindow(dpy, c->win, sw * 2, 0); 
            continue;
        }
        if (c->isfullscreen) {
            XMoveResizeWindow(dpy, c->win, 0, 0, sw, sh);
            XRaiseWindow(dpy, c->win);
        } else {
            if (i == 0) {
                XMoveResizeWindow(dpy, c->win, 0, 0, mw, sh);
            } else {
                bh = sh / (n - 1);
                XMoveResizeWindow(dpy, c->win, mw, (i - 1) * bh, sw - mw, bh);
            }
            i++;
        }
    }
    XSync(dpy, False);
}

static void
focus(Client *c)
{
    sel = (c && c->ws == curws) ? c : NULL;
    if (sel) {
        XSetInputFocus(dpy, sel->win, RevertToPointerRoot, CurrentTime);
        XRaiseWindow(dpy, sel->win);
    } else {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    }
}

static void
destroynotify(XEvent *e)
{
    Client **tc, *c;
    for (tc = &clients; *tc && (*tc)->win != e->xdestroywindow.window; tc = &(*tc)->next);
    if ((c = *tc)) {
        *tc = c->next;
        free(c);
        if (sel == c) sel = NULL; 
        arrange();
        if (!sel) focusnext(NULL);
        else focus(sel);
    }
}

static void
enternotify(XEvent *e)
{
    Client *c;
    if (e->xcrossing.mode != NotifyNormal || e->xcrossing.detail == NotifyInferior) return;
    for (c = clients; c; c = c->next)
        if (c->win == e->xcrossing.window && c->ws == curws) {
            focus(c);
            return;
        }
}

static void
focusnext(const Arg *arg)
{
    Client *c;
    (void)arg;
    if (!sel) {
        for (c = clients; c && c->ws != curws; c = c->next);
        focus(c);
        return;
    }
    for (c = sel->next; c && c->ws != curws; c = c->next);
    if (!c) {
        for (c = clients; c && c->ws != curws; c = c->next);
    }
    focus(c);
}

static void
focusprev(const Arg *arg)
{
    Client *c, *p = NULL;
    (void)arg;
    if (!sel) return;
    for (c = clients; c && c != sel; c = c->next)
        if (c->ws == curws) p = c;
    if (!p) {
        for (c = clients; c; c = c->next)
            if (c->ws == curws) p = c;
    }
    focus(p);
}

static void
keypress(XEvent *e)
{
    KeySym keysym = XLookupKeysym(&e->xkey, 0);
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (keysym == keys[i].keysym && keys[i].mod == e->xkey.state && keys[i].func)
            keys[i].func(&keys[i].arg);
}

static void
killclient(const Arg *arg)
{
    (void)arg;
    if (!sel) return;
    XKillClient(dpy, sel->win);
}

static void
manage(Window w, XWindowAttributes *wa)
{
    Client *c;
    if (!(c = calloc(1, sizeof(Client)))) err(1, "calloc");
    c->win = w;
    c->ws = curws;
    c->isfullscreen = 0;
    c->next = clients;
    clients = c;
    XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
    XMapWindow(dpy, w);
    arrange();
    focus(c);
}

static void
maprequest(XEvent *e)
{
    XWindowAttributes wa;
    if (!XGetWindowAttributes(dpy, e->xmaprequest.window, &wa) || wa.override_redirect) return;
    manage(e->xmaprequest.window, &wa);
}

static void
movetows(const Arg *arg)
{
    if (sel && arg->i >= 0 && arg->i < NUM_WS && arg->i != curws) {
        sel->ws = arg->i;
        arrange();
        Client *c;
        for (c = clients; c && c->ws != curws; c = c->next);
        focus(c);
    }
}

static void
quit(const Arg *arg)
{
    (void)arg;
    running = 0;
}

static void
setmfact(const Arg *arg)
{
    mfact += arg->f;
    if (mfact < 0.1) mfact = 0.1;
    if (mfact > 0.9) mfact = 0.9;
    arrange();
}

static void
spawn(const Arg *arg)
{
    if (fork() == 0) {
        if (dpy) close(ConnectionNumber(dpy));
        setsid();
        execvp(((char **)arg->v)[0], (char **)arg->v);
        err(1, "execvp %s", ((char **)arg->v)[0]);
    }
}

static void
togglefullscreen(const Arg *arg)
{
    (void)arg;
    if (sel) {
        sel->isfullscreen = !sel->isfullscreen;
        arrange();
    }
}

static void
view(const Arg *arg)
{
    if (arg->i >= 0 && arg->i < NUM_WS && arg->i != curws) {
        curws = arg->i;
        arrange();
        Client *c;
        for (c = clients; c && c->ws != curws; c = c->next);
        focus(c);
    }
}

int
main(void)
{
    XEvent ev;

    if (!(dpy = XOpenDisplay(NULL))) 
        errx(1, "cannot open display");
    
    signal(SIGCHLD, SIG_IGN);

#ifdef __OpenBSD__
    if (pledge("stdio rpath proc exec", NULL) == -1) 
        err(1, "pledge");
#endif

    root = DefaultRootWindow(dpy);
    sw = DisplayWidth(dpy, DefaultScreen(dpy));
    sh = DisplayHeight(dpy, DefaultScreen(dpy));

    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
    
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        XGrabKey(dpy, XKeysymToKeycode(dpy, keys[i].keysym), keys[i].mod, root, True, GrabModeAsync, GrabModeAsync);

    while (running && !XNextEvent(dpy, &ev)) {
        if (ev.type == MapRequest) maprequest(&ev);
        else if (ev.type == DestroyNotify) destroynotify(&ev);
        else if (ev.type == KeyPress) keypress(&ev);
        else if (ev.type == EnterNotify) enternotify(&ev);
    }

    XCloseDisplay(dpy);
    return 0;
}