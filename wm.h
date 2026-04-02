#define MODKEY   Mod4Mask
#define WS(n)                                      \
        { MODKEY, XK_##n, VIEW, {.i=n-1} },        \
        { MODKEY|ShiftMask, XK_##n, SEND, {.i=n-1} }

enum { EXEC, VIEW, CYCLE, SWAP, SEND, RESIZE, FULLSCR, CLOSE, QUIT };
static const char *termcmd[] = { "st", NULL };
static const char *menucmd[] = { "dmenu_run", NULL };

static Key keys[] = {
        { MODKEY,            XK_Return, EXEC,    {.v = termcmd}  },
        { MODKEY,            XK_space,  EXEC,    {.v = menucmd}  },
        { MODKEY,            XK_j,      CYCLE,   {.i = +1}       },
        { MODKEY,            XK_k,      CYCLE,   {.i = -1}       },
        { MODKEY|ShiftMask,  XK_j,      SWAP,    {.i = +1}       },
        { MODKEY|ShiftMask,  XK_k,      SWAP,    {.i = -1}       },
        { MODKEY,            XK_h,      RESIZE,  {.f = -0.05f}   },
        { MODKEY,            XK_l,      RESIZE,  {.f = +0.05f}   },
        { MODKEY,            XK_q,      CLOSE,   {0}             },
        { MODKEY|ShiftMask,  XK_q,      QUIT,    {0}             },
        { MODKEY,            XK_f,      FULLSCR, {0}             },
        WS(1), WS(2), WS(3), WS(4), WS(5), WS(6), WS(7), WS(8), WS(9),
};
