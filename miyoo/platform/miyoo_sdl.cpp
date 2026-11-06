/* The part of SDL2 that Olduvai calls, for the Miyoo Mini (Plus) without SDL.
 *
 * The engine is compiled against the real SDL2 headers and this file implements the functions
 * it calls on the Miyoo's Linux directly (the device's own SDL2 needs the vendor's libraries
 * and an older glibc). After quake2-miyoomini's miyoo/platform/miyoo_sdl.c (2026-11-06), itself
 * after redriver2-miyoo's src_rebuild/platform/Miyoo/miyoo_sdl.cpp:
 *   - time: POSIX clocks;
 *   - buttons: /dev/input/event0 (gpio-keys, reported as keyboard keys) as one Nintendo-style
 *     game controller (A right, B bottom, as printed on the Miyoo), MENU as the Escape key
 *     (the engine's pause menu); SIGTERM / SIGINT become SDL_QUIT (Onion's runtime, a kill);
 *   - event watches (the engine turns controller events into keys in one);
 *   - window: the panel, always 640x480, whatever size is asked for;
 *   - renderer: new here. A software SDL_Renderer for what the engine draws: textures
 *     (RGBA32 / ARGB8888, kept as 0xAARRGGBB), render targets, the logical size with
 *     letterboxing and integer scaling, nearest-pixel scaling, blend modes NONE and BLEND,
 *     clear, read-back. The window's picture is a 640x480 frame in memory the 2D engine reads
 *     (MiyooFb_AllocFrame); SDL_RenderPresent hands it to miyoo_fb.c, which turns it 180
 *     degrees onto the panel with MI_GFX (no CPU work) on its pan thread;
 *   - surfaces: what screenshots and bug reports need (RGBA32 / RGB24, BMP save);
 *   - crashes: the faulting pc and the code addresses on the stack (for addr2line).
 * The sound is in miyoo_audio.c. */

#include <SDL.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/resource.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include <linux/input.h>

#include <vector>

#include "miyoo_platform.h"

#define PANEL_W 640
#define PANEL_H 480

/* ---- time */

static uint64_t NowNs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t s_startNs;

Uint32 SDL_GetTicks(void)
{
	if (!s_startNs)
		s_startNs = NowNs();
	return (Uint32)((NowNs() - s_startNs) / 1000000ull);
}

Uint64 SDL_GetPerformanceCounter(void) { return NowNs(); }
Uint64 SDL_GetPerformanceFrequency(void) { return 1000000000ull; }

void SDL_Delay(Uint32 ms)
{
	struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		;
}

/* ---- misc */

const char *SDL_GetError(void) { return "miyoo"; }
void SDL_free(void *mem) { free(mem); }
SDL_bool SDL_SetHint(const char *name, const char *value) { (void)name; (void)value; return SDL_TRUE; }
char *SDL_getenv(const char *name) { return getenv(name); }
int SDL_setenv(const char *name, const char *value, int overwrite) { return setenv(name, value, overwrite); }
void SDL_GetVersion(SDL_version *ver) { SDL_VERSION(ver); }
int SDL_OpenURL(const char *url) { (void)url; return -1; }

/* the executable's folder, with a trailing slash (the engine finds fonts/ there) */
char *SDL_GetBasePath(void)
{
	char path[1024];
	ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
	char *slash;

	if (n <= 0)
		return strdup("./");
	path[n] = 0;
	slash = strrchr(path, '/');
	if (slash)
		slash[1] = 0;
	return strdup(path);
}

int SDL_ShowMessageBox(const SDL_MessageBoxData *data, int *buttonid)
{
	fprintf(stderr, "miyoo: message box \"%s\": %s\n", data && data->title ? data->title : "",
		data && data->message ? data->message : "");
	if (buttonid)
		*buttonid = -1;
	return -1;	/* (no box: the engine falls back to its own screen) */
}

/* ---- keys: names and codes, for the engine's key bindings (play.json) */

struct KeyName { SDL_Scancode sc; SDL_Keycode key; const char *name; };
static const KeyName s_keys[] = {
	{ SDL_SCANCODE_RETURN, SDLK_RETURN, "Return" }, { SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, "Escape" },
	{ SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE, "Backspace" }, { SDL_SCANCODE_TAB, SDLK_TAB, "Tab" },
	{ SDL_SCANCODE_SPACE, SDLK_SPACE, "Space" }, { SDL_SCANCODE_MINUS, SDLK_MINUS, "-" },
	{ SDL_SCANCODE_EQUALS, SDLK_EQUALS, "=" }, { SDL_SCANCODE_LEFTBRACKET, SDLK_LEFTBRACKET, "[" },
	{ SDL_SCANCODE_RIGHTBRACKET, SDLK_RIGHTBRACKET, "]" }, { SDL_SCANCODE_BACKSLASH, SDLK_BACKSLASH, "\\" },
	{ SDL_SCANCODE_SEMICOLON, SDLK_SEMICOLON, ";" }, { SDL_SCANCODE_APOSTROPHE, SDLK_QUOTE, "'" },
	{ SDL_SCANCODE_GRAVE, SDLK_BACKQUOTE, "`" }, { SDL_SCANCODE_COMMA, SDLK_COMMA, "," },
	{ SDL_SCANCODE_PERIOD, SDLK_PERIOD, "." }, { SDL_SCANCODE_SLASH, SDLK_SLASH, "/" },
	{ SDL_SCANCODE_CAPSLOCK, SDLK_CAPSLOCK, "CapsLock" },
	{ SDL_SCANCODE_F1, SDLK_F1, "F1" }, { SDL_SCANCODE_F2, SDLK_F2, "F2" }, { SDL_SCANCODE_F3, SDLK_F3, "F3" },
	{ SDL_SCANCODE_F4, SDLK_F4, "F4" }, { SDL_SCANCODE_F5, SDLK_F5, "F5" }, { SDL_SCANCODE_F6, SDLK_F6, "F6" },
	{ SDL_SCANCODE_F7, SDLK_F7, "F7" }, { SDL_SCANCODE_F8, SDLK_F8, "F8" }, { SDL_SCANCODE_F9, SDLK_F9, "F9" },
	{ SDL_SCANCODE_F10, SDLK_F10, "F10" }, { SDL_SCANCODE_F11, SDLK_F11, "F11" }, { SDL_SCANCODE_F12, SDLK_F12, "F12" },
	{ SDL_SCANCODE_PRINTSCREEN, SDLK_PRINTSCREEN, "PrintScreen" }, { SDL_SCANCODE_PAUSE, SDLK_PAUSE, "Pause" },
	{ SDL_SCANCODE_INSERT, SDLK_INSERT, "Insert" }, { SDL_SCANCODE_HOME, SDLK_HOME, "Home" },
	{ SDL_SCANCODE_PAGEUP, SDLK_PAGEUP, "PageUp" }, { SDL_SCANCODE_DELETE, SDLK_DELETE, "Delete" },
	{ SDL_SCANCODE_END, SDLK_END, "End" }, { SDL_SCANCODE_PAGEDOWN, SDLK_PAGEDOWN, "PageDown" },
	{ SDL_SCANCODE_RIGHT, SDLK_RIGHT, "Right" }, { SDL_SCANCODE_LEFT, SDLK_LEFT, "Left" },
	{ SDL_SCANCODE_DOWN, SDLK_DOWN, "Down" }, { SDL_SCANCODE_UP, SDLK_UP, "Up" },
	{ SDL_SCANCODE_KP_ENTER, SDLK_KP_ENTER, "Keypad Enter" },
	{ SDL_SCANCODE_LCTRL, SDLK_LCTRL, "Left Ctrl" }, { SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, "Left Shift" },
	{ SDL_SCANCODE_LALT, SDLK_LALT, "Left Alt" }, { SDL_SCANCODE_LGUI, SDLK_LGUI, "Left GUI" },
	{ SDL_SCANCODE_RCTRL, SDLK_RCTRL, "Right Ctrl" }, { SDL_SCANCODE_RSHIFT, SDLK_RSHIFT, "Right Shift" },
	{ SDL_SCANCODE_RALT, SDLK_RALT, "Right Alt" }, { SDL_SCANCODE_RGUI, SDLK_RGUI, "Right GUI" },
};

static SDL_Scancode LetterOrDigitScancode(char c)
{
	if (c >= 'a' && c <= 'z')
		return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'a'));
	if (c >= '1' && c <= '9')
		return (SDL_Scancode)(SDL_SCANCODE_1 + (c - '1'));
	if (c == '0')
		return SDL_SCANCODE_0;
	return SDL_SCANCODE_UNKNOWN;
}

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key)
{
	if (key & SDLK_SCANCODE_MASK)
		return (SDL_Scancode)(key & ~SDLK_SCANCODE_MASK);
	SDL_Scancode sc = LetterOrDigitScancode((char)key);
	if (sc != SDL_SCANCODE_UNKNOWN)
		return sc;
	for (const KeyName &k : s_keys)
		if (k.key == key)
			return k.sc;
	return SDL_SCANCODE_UNKNOWN;
}

static SDL_Keycode KeyFromScancode(SDL_Scancode sc)
{
	if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
		return 'a' + (sc - SDL_SCANCODE_A);
	if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
		return '1' + (sc - SDL_SCANCODE_1);
	if (sc == SDL_SCANCODE_0)
		return '0';
	for (const KeyName &k : s_keys)
		if (k.sc == sc)
			return k.key;
	return SDL_SCANCODE_TO_KEYCODE(sc);
}

SDL_Scancode SDL_GetScancodeFromName(const char *name)
{
	if (!name || !*name)
		return SDL_SCANCODE_UNKNOWN;
	if (!name[1])
	{
		const char c = (char)(name[0] >= 'A' && name[0] <= 'Z' ? name[0] - 'A' + 'a' : name[0]);
		const SDL_Scancode sc = LetterOrDigitScancode(c);
		if (sc != SDL_SCANCODE_UNKNOWN)
			return sc;
	}
	for (const KeyName &k : s_keys)
		if (!strcasecmp(k.name, name))
			return k.sc;
	/* (SDL's other spellings the engine may use) */
	if (!strcasecmp(name, "Enter"))
		return SDL_SCANCODE_RETURN;
	if (!strcasecmp(name, "Esc"))
		return SDL_SCANCODE_ESCAPE;
	return SDL_SCANCODE_UNKNOWN;
}

SDL_Keycode SDL_GetKeyFromName(const char *name)
{
	const SDL_Scancode sc = SDL_GetScancodeFromName(name);
	return sc == SDL_SCANCODE_UNKNOWN ? SDLK_UNKNOWN : KeyFromScancode(sc);
}

const char *SDL_GetScancodeName(SDL_Scancode sc)
{
	static char one[2];
	if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
	{
		one[0] = (char)('A' + (sc - SDL_SCANCODE_A));
		return one;
	}
	if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0)
	{
		one[0] = sc == SDL_SCANCODE_0 ? '0' : (char)('1' + (sc - SDL_SCANCODE_1));
		return one;
	}
	for (const KeyName &k : s_keys)
		if (k.sc == sc)
			return k.name;
	return "";
}

static Uint8 s_keyState[SDL_NUM_SCANCODES];

const Uint8 *SDL_GetKeyboardState(int *numkeys)
{
	if (numkeys)
		*numkeys = SDL_NUM_SCANCODES;
	return s_keyState;
}

SDL_Keymod SDL_GetModState(void) { return KMOD_NONE; }
void SDL_StartTextInput(void) {}
void SDL_StopTextInput(void) {}
Uint32 SDL_GetMouseState(int *x, int *y) { if (x) *x = 0; if (y) *y = 0; return 0; }
int SDL_ShowCursor(int toggle) { (void)toggle; return 0; }

/* ---- events: a queue, the watches, the Miyoo's buttons */

#define EVENT_QUEUE 128
static SDL_Event s_events[EVENT_QUEUE];
static int s_eventHead, s_eventCount;

struct Watch { SDL_EventFilter fn; void *userdata; };
static std::vector<Watch> s_watches;

void SDL_AddEventWatch(SDL_EventFilter filter, void *userdata)
{
	s_watches.push_back({ filter, userdata });
}

void SDL_DelEventWatch(SDL_EventFilter filter, void *userdata)
{
	for (size_t i = 0; i < s_watches.size(); i++)
		if (s_watches[i].fn == filter && s_watches[i].userdata == userdata)
		{
			s_watches.erase(s_watches.begin() + (long)i);
			return;
		}
}

/* as SDL's: the watches see each event when it is added to the queue */
static void AddEvent(SDL_Event *e)
{
	const std::vector<Watch> watches = s_watches;	/* (a watch may add or remove one) */
	for (const Watch &w : watches)
		w.fn(w.userdata, e);
	if (s_eventCount == EVENT_QUEUE)
		return;
	s_events[(s_eventHead + s_eventCount) % EVENT_QUEUE] = *e;
	s_eventCount++;
}

int SDL_PushEvent(SDL_Event *event)
{
	if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)
	{
		const SDL_Scancode sc = event->key.keysym.scancode;
		if ((unsigned)sc < SDL_NUM_SCANCODES)
			s_keyState[sc] = event->type == SDL_KEYDOWN;
	}
	AddEvent(event);
	return 1;
}

static int s_inputFd = -1;
static volatile sig_atomic_t s_quitSignal;

/* The Miyoo's buttons are one game controller, a Nintendo-style one (the engine then names the
 * buttons A/B/X/Y as printed on the Miyoo). SDL's buttons are positions: SDL_CONTROLLER_BUTTON_A
 * is the bottom one (the Miyoo's B), B the right one (A), X the left one (Y), Y the top one (X).
 * L2/R2 are the triggers (axes). MENU stays a key: Escape (the engine's pause menu). As
 * quake2-miyoomini's PadButton. */
enum { PAD_NONE = -1, PAD_TRIG_LEFT = 100, PAD_TRIG_RIGHT };

static int PadButton(int code)
{
	switch (code)
	{
	case KEY_UP:		return SDL_CONTROLLER_BUTTON_DPAD_UP;
	case KEY_DOWN:		return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
	case KEY_LEFT:		return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
	case KEY_RIGHT:		return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
	case KEY_LEFTCTRL:	return SDL_CONTROLLER_BUTTON_A;			/* B, bottom */
	case KEY_SPACE:		return SDL_CONTROLLER_BUTTON_B;			/* A, right */
	case KEY_LEFTALT:	return SDL_CONTROLLER_BUTTON_X;			/* Y, left */
	case KEY_LEFTSHIFT:	return SDL_CONTROLLER_BUTTON_Y;			/* X, top */
	case KEY_E:			return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;	/* L1 */
	case KEY_T:			return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;	/* R1 */
	case KEY_TAB:		return PAD_TRIG_LEFT;					/* L2 */
	case KEY_BACKSPACE:	return PAD_TRIG_RIGHT;					/* R2 */
	case KEY_RIGHTCTRL:	return SDL_CONTROLLER_BUTTON_BACK;		/* SELECT */
	case KEY_ENTER:		return SDL_CONTROLLER_BUTTON_START;		/* START */
	}
	return PAD_NONE;
}

#define PAD_JOYSTICK_ID 0
static Uint8 s_padButtons[SDL_CONTROLLER_BUTTON_MAX];
static Sint16 s_padTriggers[2];

static void ReadButtons(void)
{
	struct input_event ev[16];
	ssize_t n;

	if (s_quitSignal == 1)
	{
		SDL_Event e;
		memset(&e, 0, sizeof(e));
		e.type = SDL_QUIT;
		s_quitSignal = 2;
		AddEvent(&e);
	}
	if (s_inputFd < 0)
		return;
	while ((n = read(s_inputFd, ev, sizeof(ev))) > 0)
	{
		for (int i = 0; i < (int)(n / sizeof(ev[0])); i++)
		{
			const int down = ev[i].value != 0;
			SDL_Event e;
			int b;

			if (ev[i].type != EV_KEY || ev[i].value == 2)	/* (2: auto-repeat) */
				continue;
			memset(&e, 0, sizeof(e));
			if (ev[i].code == KEY_ESC)	/* MENU */
			{
				e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
				e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
				e.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
				e.key.keysym.sym = SDLK_ESCAPE;
				SDL_PushEvent(&e);
				continue;
			}
			b = PadButton(ev[i].code);
			if (b == PAD_NONE)
				continue;
			if (b >= PAD_TRIG_LEFT)
			{
				const int t = b - PAD_TRIG_LEFT;
				s_padTriggers[t] = down ? SDL_JOYSTICK_AXIS_MAX : 0;
				e.type = SDL_CONTROLLERAXISMOTION;
				e.caxis.which = PAD_JOYSTICK_ID;
				e.caxis.axis = t ? SDL_CONTROLLER_AXIS_TRIGGERRIGHT : SDL_CONTROLLER_AXIS_TRIGGERLEFT;
				e.caxis.value = s_padTriggers[t];
			}
			else
			{
				s_padButtons[b] = (Uint8)down;
				e.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
				e.cbutton.which = PAD_JOYSTICK_ID;
				e.cbutton.button = (Uint8)b;
				e.cbutton.state = down ? SDL_PRESSED : SDL_RELEASED;
			}
			AddEvent(&e);
		}
	}
}

int SDL_PollEvent(SDL_Event *event)
{
	ReadButtons();
	if (!s_eventCount)
		return 0;
	if (event)
		*event = s_events[s_eventHead];
	s_eventHead = (s_eventHead + 1) % EVENT_QUEUE;
	s_eventCount--;
	return 1;
}

int SDL_WaitEventTimeout(SDL_Event *event, int timeout)
{
	const Uint32 t0 = SDL_GetTicks();
	for (;;)
	{
		if (SDL_PollEvent(event))
			return 1;
		if (timeout >= 0 && (int)(SDL_GetTicks() - t0) >= timeout)
			return 0;
		SDL_Delay(5);
	}
}

int SDL_WaitEvent(SDL_Event *event) { return SDL_WaitEventTimeout(event, -1); }

/* ---- crashes (redriver2-miyoo's CrashHandler): map the addresses with addr2line on the
 * unstripped build (build/miyoo/olduvai.unstripped) */

extern "C" char __executable_start, etext;

static void CrashHandler(int sig, siginfo_t *info, void *ctx)
{
	const ucontext_t *uc = (const ucontext_t *)ctx;
	const mcontext_t *m = &uc->uc_mcontext;
	const uintptr_t lo = (uintptr_t)&__executable_start, hi = (uintptr_t)&etext;
	const uint32_t *sp = (const uint32_t *)m->arm_sp;
	char line[160];
	int n, found = 0;

	n = snprintf(line, sizeof(line), "\n*** signal %d at pc %08lx (lr %08lx, sp %08lx), address %p\n",
		sig, m->arm_pc, m->arm_lr, m->arm_sp, info->si_addr);
	if (write(2, line, n) < 0)
		_exit(1);
	/* words on the stack that point into the code: likely return addresses */
	if (write(2, "stack:", 6) < 0)
		_exit(1);
	for (int i = 0; i < 2048 && found < 24; i++)
	{
		const uint32_t v = sp[i];
		if (v >= lo && v < hi)
		{
			n = snprintf(line, sizeof(line), " %08x", v);
			if (write(2, line, n) < 0)
				break;
			found++;
		}
	}
	if (write(2, "\n", 1) < 0)
		_exit(1);
	MiyooFb_Close();	/* (Onion's menu must find the 2D engine free) */
	signal(sig, SIG_DFL);
	raise(sig);
}

static void QuitSignal(int sig)
{
	(void)sig;
	if (!s_quitSignal)
		s_quitSignal = 1;
}

static void InstallSignalHandlers(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = CrashHandler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
	signal(SIGTERM, QuitSignal);
	signal(SIGINT, QuitSignal);
}

/* ---- init */

static Uint32 s_inited;

int SDL_InitSubSystem(Uint32 flags)
{
	static int handlers;
	if (!handlers)
	{
		InstallSignalHandlers();
		handlers = 1;
	}
	SDL_GetTicks();
	if ((flags & (SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS)) && s_inputFd < 0)
	{
		s_inputFd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (s_inputFd < 0)
			fprintf(stderr, "miyoo: can't open /dev/input/event0 (%s)\n", strerror(errno));
	}
	if (flags & SDL_INIT_GAMECONTROLLER)
		flags |= SDL_INIT_JOYSTICK;
	if (flags & (SDL_INIT_VIDEO | SDL_INIT_JOYSTICK))
		flags |= SDL_INIT_EVENTS;
	s_inited |= flags;
	return 0;
}

int SDL_Init(Uint32 flags) { return SDL_InitSubSystem(flags); }

Uint32 SDL_WasInit(Uint32 flags)
{
	return s_inited & (flags ? flags : SDL_INIT_EVERYTHING);
}

void SDL_QuitSubSystem(Uint32 flags)
{
	s_inited &= ~flags;
}

void SDL_Quit(void)
{
	if (s_inputFd >= 0)
		close(s_inputFd);
	s_inputFd = -1;
	s_inited = 0;
	MiyooFb_Close();
}

/* ---- displays and the window: the panel */

const char *SDL_GetCurrentVideoDriver(void) { return (s_inited & SDL_INIT_VIDEO) ? "miyoo" : NULL; }
int SDL_GetNumVideoDrivers(void) { return 1; }
const char *SDL_GetVideoDriver(int index) { return index == 0 ? "miyoo" : NULL; }

static void FillMode(SDL_DisplayMode *mode)
{
	memset(mode, 0, sizeof(*mode));
	mode->format = SDL_PIXELFORMAT_ARGB8888;
	mode->w = PANEL_W;
	mode->h = PANEL_H;
	mode->refresh_rate = 60;
}

int SDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode) { (void)displayIndex; FillMode(mode); return 0; }
int SDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode *mode) { (void)displayIndex; FillMode(mode); return 0; }

int SDL_GetDisplayUsableBounds(int displayIndex, SDL_Rect *rect)
{
	(void)displayIndex;
	rect->x = rect->y = 0;
	rect->w = PANEL_W;
	rect->h = PANEL_H;
	return 0;
}

struct SDL_Window { int dummy; };
static SDL_Window s_window;

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
	(void)title; (void)x; (void)y; (void)flags;
	fprintf(stderr, "miyoo: window %dx%d asked, the panel is %dx%d\n", w, h, PANEL_W, PANEL_H);
	return &s_window;
}

void SDL_DestroyWindow(SDL_Window *window) { (void)window; }
int SDL_GetWindowDisplayIndex(SDL_Window *window) { (void)window; return 0; }
Uint32 SDL_GetWindowFlags(SDL_Window *window)
{
	(void)window;
	return SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_FULLSCREEN_DESKTOP;
}
void SDL_GetWindowSize(SDL_Window *window, int *w, int *h) { (void)window; if (w) *w = PANEL_W; if (h) *h = PANEL_H; }
int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags) { (void)window; (void)flags; return 0; }
void SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon) { (void)window; (void)icon; }

/* ---- surfaces: RGBA32 and RGB24, for screenshots and bug reports */

static int BytesPerPixel(Uint32 format)
{
	return format == SDL_PIXELFORMAT_RGB24 ? 3 : 4;
}

static SDL_Surface *NewSurface(void *pixels, int w, int h, int pitch, Uint32 format)
{
	SDL_Surface *s = (SDL_Surface *)calloc(1, sizeof(SDL_Surface) + sizeof(SDL_PixelFormat));
	SDL_PixelFormat *f = (SDL_PixelFormat *)(s + 1);
	f->format = format;
	f->BytesPerPixel = (Uint8)BytesPerPixel(format);
	f->BitsPerPixel = (Uint8)(f->BytesPerPixel * 8);
	s->format = f;
	s->w = w;
	s->h = h;
	s->pitch = pitch;
	s->refcount = 1;
	if (pixels)
	{
		s->pixels = pixels;
		s->flags = SDL_PREALLOC;
	}
	else
		s->pixels = calloc((size_t)pitch * h, 1);
	return s;
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, Uint32 format)
{
	(void)flags; (void)depth;
	if (format != SDL_PIXELFORMAT_RGBA32 && format != SDL_PIXELFORMAT_ARGB8888 && format != SDL_PIXELFORMAT_RGB24)
		return NULL;
	return NewSurface(NULL, width, height, width * BytesPerPixel(format), format);
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int width, int height, int depth, int pitch, Uint32 format)
{
	(void)depth;
	if (format != SDL_PIXELFORMAT_RGBA32 && format != SDL_PIXELFORMAT_ARGB8888 && format != SDL_PIXELFORMAT_RGB24)
		return NULL;
	return NewSurface(pixels, width, height, pitch, format);
}

void SDL_FreeSurface(SDL_Surface *s)
{
	if (!s)
		return;
	if (!(s->flags & SDL_PREALLOC))
		free(s->pixels);
	free(s);
}

/* one pixel of a surface as R, G, B */
static void GetRGB(const SDL_Surface *s, int x, int y, Uint8 *rgb)
{
	const Uint8 *p = (const Uint8 *)s->pixels + (size_t)y * s->pitch + (size_t)x * s->format->BytesPerPixel;
	if (s->format->format == SDL_PIXELFORMAT_ARGB8888)
	{
		const Uint32 v = *(const Uint32 *)p;
		rgb[0] = (Uint8)(v >> 16);
		rgb[1] = (Uint8)(v >> 8);
		rgb[2] = (Uint8)v;
	}
	else	/* RGBA32 and RGB24: R, G, B bytes first */
	{
		rgb[0] = p[0];
		rgb[1] = p[1];
		rgb[2] = p[2];
	}
}

SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 format, Uint32 flags)
{
	(void)flags;
	if (!src || format != SDL_PIXELFORMAT_RGB24)
		return NULL;
	SDL_Surface *d = NewSurface(NULL, src->w, src->h, src->w * 3, format);
	for (int y = 0; y < src->h; y++)
		for (int x = 0; x < src->w; x++)
			GetRGB(src, x, y, (Uint8 *)d->pixels + (size_t)y * d->pitch + (size_t)x * 3);
	return d;
}

/* files: only for SDL_SaveBMP (SDL_SaveBMP_RW + SDL_RWFromFile) */
static int SDLCALL RwClose(SDL_RWops *rw)
{
	FILE *f = (FILE *)rw->hidden.unknown.data1;
	const int r = f ? fclose(f) : 0;
	free(rw);
	return r;
}

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode)
{
	FILE *f = fopen(file, mode);
	if (!f)
		return NULL;
	SDL_RWops *rw = (SDL_RWops *)calloc(1, sizeof(SDL_RWops));
	rw->hidden.unknown.data1 = f;
	rw->close = RwClose;
	return rw;
}

static void Put32(Uint8 *p, Uint32 v) { p[0] = (Uint8)v; p[1] = (Uint8)(v >> 8); p[2] = (Uint8)(v >> 16); p[3] = (Uint8)(v >> 24); }

int SDL_SaveBMP_RW(SDL_Surface *s, SDL_RWops *dst, int freedst)
{
	int ok = -1;
	if (s && dst)
	{
		FILE *f = (FILE *)dst->hidden.unknown.data1;
		const int row = (s->w * 3 + 3) & ~3;
		Uint8 h[54];
		std::vector<Uint8> line((size_t)row, 0);

		memset(h, 0, sizeof(h));
		h[0] = 'B';
		h[1] = 'M';
		Put32(h + 2, 54u + (Uint32)row * s->h);
		Put32(h + 10, 54);
		Put32(h + 14, 40);
		Put32(h + 18, (Uint32)s->w);
		Put32(h + 22, (Uint32)s->h);
		h[26] = 1;
		h[28] = 24;
		Put32(h + 34, (Uint32)row * s->h);
		ok = fwrite(h, 1, sizeof(h), f) == sizeof(h) ? 0 : -1;
		for (int y = s->h - 1; y >= 0 && ok == 0; y--)
		{
			for (int x = 0; x < s->w; x++)
			{
				Uint8 rgb[3];
				GetRGB(s, x, y, rgb);
				line[(size_t)x * 3] = rgb[2];
				line[(size_t)x * 3 + 1] = rgb[1];
				line[(size_t)x * 3 + 2] = rgb[0];
			}
			if (fwrite(line.data(), 1, line.size(), f) != line.size())
				ok = -1;
		}
	}
	if (dst && freedst)
		RwClose(dst);
	return ok;
}

/* ---- the renderer: software, into 0xAARRGGBB pixels */

struct SDL_Texture
{
	int w, h;
	Uint32 format;
	int access;
	uint32_t *px;		/* w x h, 0xAARRGGBB */
	SDL_BlendMode blend;
	SDL_ScaleMode scale;
};

struct SDL_Renderer
{
	SDL_Texture back;			/* the window's picture (panel-sized, MMA) */
	SDL_Texture *target;		/* NULL: back */
	int logicalW, logicalH;
	SDL_bool integerScale;
	Uint8 r, g, b, a;
};

static SDL_Renderer s_renderer;
static int s_rendererOpen;

/* the viewport of the current target in its pixels, and the scale from logical units */
struct View { int x, y, w, h; float scale; };

static SDL_Texture *Target(SDL_Renderer *ren)
{
	return ren->target ? ren->target : &ren->back;
}

/* SDL2's logical-size letterboxing (SDL_RenderSetLogicalSize, UpdateLogicalSize): the largest
 * scale that fits both ways, whole when integer scaling is on; a target is drawn 1:1 */
static View CurrentView(const SDL_Renderer *ren)
{
	View v;
	if (ren->target || ren->logicalW <= 0 || ren->logicalH <= 0)
	{
		const SDL_Texture *t = ren->target ? ren->target : &ren->back;
		v.x = v.y = 0;
		v.w = t->w;
		v.h = t->h;
		v.scale = 1.0f;
		return v;
	}
	const float sx = (float)ren->back.w / ren->logicalW, sy = (float)ren->back.h / ren->logicalH;
	float s = sx < sy ? sx : sy;
	if (ren->integerScale && s >= 1.0f)
		s = (float)(int)s;
	v.w = (int)(ren->logicalW * s + 0.5f);
	v.h = (int)(ren->logicalH * s + 0.5f);
	v.x = (ren->back.w - v.w) / 2;
	v.y = (ren->back.h - v.h) / 2;
	v.scale = s;
	return v;
}

SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, int index, Uint32 flags)
{
	(void)window; (void)index; (void)flags;
	if (s_rendererOpen)
		return &s_renderer;
	/* MIYOO_HEADLESS=1 (tests on the PC under qemu-arm, with MIYOO_SHOTS): no panel, the
	   frames are made in memory all the same */
	if (!MiyooFb_Open() && !getenv("MIYOO_HEADLESS"))
		return NULL;
	MiyooFb_SetMaxFps(60.0f);	/* (each frame one refresh: the engine paces itself) */
	SDL_Renderer *ren = &s_renderer;
	memset(ren, 0, sizeof(*ren));
	ren->back.w = PANEL_W;
	ren->back.h = PANEL_H;
	ren->back.format = SDL_PIXELFORMAT_ARGB8888;
	ren->back.px = MiyooFb_AllocFrame(PANEL_W, PANEL_H);	/* (MMA memory: shown by the 2D engine) */
	if (!ren->back.px)
		return NULL;
	ren->a = 255;
	s_rendererOpen = 1;
	return ren;
}

void SDL_DestroyRenderer(SDL_Renderer *ren)
{
	/* kept: the engine makes a new one when the display settings change, and the panel's
	   frame memory is handed back at exit (MiyooFb_Close) */
	if (ren)
	{
		ren->target = NULL;
		ren->logicalW = ren->logicalH = 0;
	}
}

int SDL_GetRendererInfo(SDL_Renderer *ren, SDL_RendererInfo *info)
{
	(void)ren;
	memset(info, 0, sizeof(*info));
	info->name = "miyoo";
	info->flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;
	info->num_texture_formats = 2;
	info->texture_formats[0] = SDL_PIXELFORMAT_RGBA32;
	info->texture_formats[1] = SDL_PIXELFORMAT_ARGB8888;
	info->max_texture_width = info->max_texture_height = 4096;
	return 0;
}

int SDL_GetRendererOutputSize(SDL_Renderer *ren, int *w, int *h)
{
	(void)ren;
	if (w) *w = PANEL_W;
	if (h) *h = PANEL_H;
	return 0;
}

SDL_bool SDL_RenderTargetSupported(SDL_Renderer *ren) { (void)ren; return SDL_TRUE; }
int SDL_RenderSetVSync(SDL_Renderer *ren, int vsync) { (void)ren; (void)vsync; return 0; }

int SDL_RenderSetLogicalSize(SDL_Renderer *ren, int w, int h)
{
	ren->logicalW = w > 0 ? w : 0;
	ren->logicalH = h > 0 ? h : 0;
	return 0;
}

void SDL_RenderGetLogicalSize(SDL_Renderer *ren, int *w, int *h)
{
	if (w) *w = ren->logicalW;
	if (h) *h = ren->logicalH;
}

int SDL_RenderSetIntegerScale(SDL_Renderer *ren, SDL_bool enable) { ren->integerScale = enable; return 0; }

void SDL_RenderGetScale(SDL_Renderer *ren, float *sx, float *sy)
{
	const View v = CurrentView(ren);
	if (sx) *sx = v.scale;
	if (sy) *sy = v.scale;
}

/* in logical units, as SDL2's */
void SDL_RenderGetViewport(SDL_Renderer *ren, SDL_Rect *rect)
{
	const View v = CurrentView(ren);
	if (!rect)
		return;
	rect->x = rect->y = 0;
	rect->w = ren->target || !ren->logicalW ? v.w : ren->logicalW;
	rect->h = ren->target || !ren->logicalH ? v.h : ren->logicalH;
}

int SDL_SetRenderDrawColor(SDL_Renderer *ren, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	ren->r = r;
	ren->g = g;
	ren->b = b;
	ren->a = a;
	return 0;
}

/* the whole target, as SDL2's (the viewport doesn't limit a clear) */
int SDL_RenderClear(SDL_Renderer *ren)
{
	SDL_Texture *t = Target(ren);
	const uint32_t c = (uint32_t)ren->a << 24 | (uint32_t)ren->r << 16 | (uint32_t)ren->g << 8 | ren->b;
	const size_t n = (size_t)t->w * t->h;
	if (c == 0)
		memset(t->px, 0, n * 4);
	else
		for (size_t i = 0; i < n; i++)
			t->px[i] = c;
	return 0;
}

int SDL_SetRenderTarget(SDL_Renderer *ren, SDL_Texture *texture)
{
	if (texture && texture->access != SDL_TEXTUREACCESS_TARGET)
		return -1;
	ren->target = texture;
	return 0;
}

SDL_Texture *SDL_CreateTexture(SDL_Renderer *ren, Uint32 format, int access, int w, int h)
{
	(void)ren;
	if ((format != SDL_PIXELFORMAT_RGBA32 && format != SDL_PIXELFORMAT_ARGB8888) || w <= 0 || h <= 0)
	{
		fprintf(stderr, "miyoo: texture format 0x%x %dx%d not supported\n", (unsigned)format, w, h);
		return NULL;
	}
	SDL_Texture *t = (SDL_Texture *)calloc(1, sizeof(SDL_Texture));
	t->px = (uint32_t *)calloc((size_t)w * h, 4);
	if (!t->px)
	{
		free(t);
		return NULL;
	}
	t->w = w;
	t->h = h;
	t->format = format;
	t->access = access;
	t->blend = SDL_BLENDMODE_NONE;
	t->scale = SDL_ScaleModeNearest;
	return t;
}

void SDL_DestroyTexture(SDL_Texture *texture)
{
	if (!texture)
		return;
	if (s_renderer.target == texture)
		s_renderer.target = NULL;
	free(texture->px);
	free(texture);
}

int SDL_QueryTexture(SDL_Texture *texture, Uint32 *format, int *access, int *w, int *h)
{
	if (!texture)
		return -1;
	if (format) *format = texture->format;
	if (access) *access = texture->access;
	if (w) *w = texture->w;
	if (h) *h = texture->h;
	return 0;
}

int SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode mode)
{
	if (!texture || (mode != SDL_BLENDMODE_NONE && mode != SDL_BLENDMODE_BLEND))
		return -1;
	texture->blend = mode;
	return 0;
}

int SDL_SetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode mode)
{
	if (!texture)
		return -1;
	texture->scale = mode;	/* (drawn nearest-pixel whatever the mode: see SDL_RenderCopy) */
	return 0;
}

/* RGBA32 in memory is R, G, B, A: 0xAABBGGRR as a little-endian word; kept as 0xAARRGGBB */
static inline uint32_t SwapRB(uint32_t v)
{
	return (v & 0xFF00FF00u) | ((v >> 16) & 0xFFu) | ((v & 0xFFu) << 16);
}

int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch)
{
	if (!texture || !pixels)
		return -1;
	SDL_Rect r = rect ? *rect : SDL_Rect{ 0, 0, texture->w, texture->h };
	if (r.x < 0 || r.y < 0 || r.x + r.w > texture->w || r.y + r.h > texture->h)
		return -1;
	for (int y = 0; y < r.h; y++)
	{
		const uint32_t *s = (const uint32_t *)((const Uint8 *)pixels + (size_t)y * pitch);
		uint32_t *d = texture->px + (size_t)(r.y + y) * texture->w + r.x;
		if (texture->format == SDL_PIXELFORMAT_ARGB8888)
			memcpy(d, s, (size_t)r.w * 4);
		else
			for (int x = 0; x < r.w; x++)
				d[x] = SwapRB(s[x]);
	}
	return 0;
}

static inline uint32_t Blend(uint32_t s, uint32_t d)
{
	const uint32_t a = s >> 24;
	if (a == 255)
		return s;
	if (a == 0)
		return d;
	const uint32_t ia = 255 - a;
	/* (SDL's software blend: dst = src * a + dst * (1 - a), alpha: a + dstA * (1 - a)) */
	const uint32_t rb = (((s & 0xFF00FFu) * a + (d & 0xFF00FFu) * ia + 0x800080u) >> 8) & 0xFF00FFu;
	const uint32_t g = (((s & 0x00FF00u) * a + (d & 0x00FF00u) * ia + 0x008000u) >> 8) & 0x00FF00u;
	const uint32_t da = d >> 24;
	const uint32_t oa = a + ((da * ia + 127) / 255);
	return oa << 24 | rb | g;
}

int SDL_RenderCopy(SDL_Renderer *ren, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect)
{
	if (!texture)
		return -1;
	SDL_Texture *t = Target(ren);
	if (t == texture)
		return -1;
	const View v = CurrentView(ren);
	const SDL_Rect src = srcrect ? *srcrect : SDL_Rect{ 0, 0, texture->w, texture->h };
	if (src.w <= 0 || src.h <= 0 || src.x < 0 || src.y < 0 || src.x + src.w > texture->w || src.y + src.h > texture->h)
		return -1;

	/* the destination in the target's pixels */
	float dx, dy, dw, dh;
	if (dstrect)
	{
		dx = (float)dstrect->x;
		dy = (float)dstrect->y;
		dw = (float)dstrect->w;
		dh = (float)dstrect->h;
	}
	else
	{
		dx = dy = 0.0f;
		dw = v.w / v.scale;
		dh = v.h / v.scale;
	}
	const int x0 = v.x + (int)(dx * v.scale + 0.5f), x1 = v.x + (int)((dx + dw) * v.scale + 0.5f);
	const int y0 = v.y + (int)(dy * v.scale + 0.5f), y1 = v.y + (int)((dy + dh) * v.scale + 0.5f);
	const int ow = x1 - x0, oh = y1 - y0;
	if (ow <= 0 || oh <= 0)
		return 0;
	/* clipped to the viewport */
	const int cx0 = x0 > v.x ? x0 : v.x, cx1 = x1 < v.x + v.w ? x1 : v.x + v.w;
	const int cy0 = y0 > v.y ? y0 : v.y, cy1 = y1 < v.y + v.h ? y1 : v.y + v.h;
	if (cx0 >= cx1 || cy0 >= cy1)
		return 0;

	/* nearest pixel: the source pixel under each destination pixel's centre */
	static std::vector<int> xmap;
	xmap.resize((size_t)(cx1 - cx0));
	for (int x = cx0; x < cx1; x++)
		xmap[(size_t)(x - cx0)] = src.x + (int)(((int64_t)(x - x0) * 2 + 1) * src.w / (2 * (int64_t)ow));
	const bool oneToOne = ow == src.w;
	const bool blend = texture->blend == SDL_BLENDMODE_BLEND;

	for (int y = cy0; y < cy1; y++)
	{
		const int sy = src.y + (int)(((int64_t)(y - y0) * 2 + 1) * src.h / (2 * (int64_t)oh));
		const uint32_t *s = texture->px + (size_t)sy * texture->w;
		uint32_t *d = t->px + (size_t)y * t->w;
		if (oneToOne && !blend)
			memcpy(d + cx0, s + xmap[0], (size_t)(cx1 - cx0) * 4);
		else if (!blend)
			for (int x = cx0; x < cx1; x++)
				d[x] = s[xmap[(size_t)(x - cx0)]];
		else
			for (int x = cx0; x < cx1; x++)
				d[x] = Blend(s[xmap[(size_t)(x - cx0)]], d[x]);
	}
	return 0;
}

int SDL_RenderReadPixels(SDL_Renderer *ren, const SDL_Rect *rect, Uint32 format, void *pixels, int pitch)
{
	const SDL_Texture *t = Target(ren);
	const View v = CurrentView(ren);
	const SDL_Rect r = rect ? SDL_Rect{ v.x + (int)(rect->x * v.scale), v.y + (int)(rect->y * v.scale),
		(int)(rect->w * v.scale), (int)(rect->h * v.scale) } : SDL_Rect{ v.x, v.y, v.w, v.h };
	if (format != SDL_PIXELFORMAT_RGBA32 && format != SDL_PIXELFORMAT_ARGB8888)
		return -1;
	if (r.x < 0 || r.y < 0 || r.x + r.w > t->w || r.y + r.h > t->h)
		return -1;
	for (int y = 0; y < r.h; y++)
	{
		const uint32_t *s = t->px + (size_t)(r.y + y) * t->w + r.x;
		uint32_t *d = (uint32_t *)((Uint8 *)pixels + (size_t)y * pitch);
		for (int x = 0; x < r.w; x++)
			d[x] = format == SDL_PIXELFORMAT_ARGB8888 ? s[x] : SwapRB(s[x]);
	}
	return 0;
}

/* Frame times for stutter checks (tools-miyoo analysis/frametimes.py): MIYOO_FRAMES=file
 * writes the time from each present to the next, in microseconds, one per line, kept in
 * memory and written at exit, with the process's getrusage counts during that frame (major
 * and minor page faults, voluntary and involuntary context switches). From quake2-miyoomini. */
#define FRAMES_MAX 216000	/* an hour at 60 fps */
struct FrameRec { uint32_t us; uint16_t majflt, minflt, nvcsw, nivcsw; };
static FrameRec *s_frames;
static int s_frameCount;
static const char *s_framesPath;

static void SaveFrames(void)
{
	FILE *f = fopen(s_framesPath, "w");
	if (!f)
		return;
	for (int i = 0; i < s_frameCount; i++)
		fprintf(f, "%u %u %u %u %u\n", (unsigned)s_frames[i].us, s_frames[i].majflt,
			s_frames[i].minflt, s_frames[i].nvcsw, s_frames[i].nivcsw);
	fclose(f);
}

static uint16_t Delta16(long now, long *last)
{
	long d = now - *last;
	*last = now;
	return (uint16_t)(d > 65535 ? 65535 : (d < 0 ? 0 : d));
}

static void FrameTime(void)
{
	static int init;
	static uint64_t last;
	static long lmaj, lmin, lv, liv;
	const uint64_t now = NowNs();
	struct rusage ru;

	if (!init)
	{
		init = 1;
		s_framesPath = getenv("MIYOO_FRAMES");
		if (s_framesPath && (s_frames = (FrameRec *)malloc(FRAMES_MAX * sizeof(FrameRec))))
			atexit(SaveFrames);
	}
	if (!s_frames)
		return;
	getrusage(RUSAGE_SELF, &ru);
	if (last && s_frameCount < FRAMES_MAX)
	{
		FrameRec *r = &s_frames[s_frameCount++];
		r->us = (uint32_t)((now - last) / 1000);
		r->majflt = Delta16(ru.ru_majflt, &lmaj);
		r->minflt = Delta16(ru.ru_minflt, &lmin);
		r->nvcsw = Delta16(ru.ru_nvcsw, &lv);
		r->nivcsw = Delta16(ru.ru_nivcsw, &liv);
	}
	else
	{
		lmaj = ru.ru_majflt;
		lmin = ru.ru_minflt;
		lv = ru.ru_nvcsw;
		liv = ru.ru_nivcsw;
	}
	last = now;
}

/* MIYOO_SHOTS=N: every Nth present saved as <MIYOO_SHOTS_DIR>/NNNNN.argb (640x480, 0xAARRGGBB
 * little-endian, the picture before the turn), for tools-miyoo analysis/shots_compare.py */
static void MaybeShot(const SDL_Texture *t)
{
	static int every = -1;
	static unsigned presents;
	if (every < 0)
	{
		const char *e = getenv("MIYOO_SHOTS");
		every = e ? atoi(e) : 0;
	}
	const unsigned k = presents++;
	if (every <= 0 || !k || k % (unsigned)every)
		return;
	const char *dir = getenv("MIYOO_SHOTS_DIR");
	char path[512];
	snprintf(path, sizeof(path), "%s/%05u.argb", dir ? dir : ".", k);
	FILE *f = fopen(path, "wb");
	if (f)
	{
		fwrite(t->px, 4, (size_t)t->w * t->h, f);
		fclose(f);
	}
}

void SDL_RenderPresent(SDL_Renderer *ren)
{
	FrameTime();
	MaybeShot(&ren->back);
	MiyooFb_Present(ren->back.px, ren->back.w, ren->back.h, ren->back.w);
}

/* ---- joysticks, game controllers: one, the Miyoo's buttons (see PadButton) */

struct _SDL_Joystick { int id; };
struct _SDL_GameController { int id; };
static struct _SDL_Joystick s_padJoystick = { PAD_JOYSTICK_ID };
static struct _SDL_GameController s_padController = { PAD_JOYSTICK_ID };

int SDL_NumJoysticks(void) { return 1; }
SDL_Joystick *SDL_JoystickOpen(int device_index) { return device_index == 0 ? &s_padJoystick : NULL; }
void SDL_JoystickClose(SDL_Joystick *joystick) { (void)joystick; }
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *joystick) { return joystick ? joystick->id : -1; }
SDL_bool SDL_IsGameController(int joystick_index) { return joystick_index == 0 ? SDL_TRUE : SDL_FALSE; }
SDL_GameController *SDL_GameControllerOpen(int joystick_index) { return joystick_index == 0 ? &s_padController : NULL; }
void SDL_GameControllerClose(SDL_GameController *gamecontroller) { (void)gamecontroller; }
const char *SDL_GameControllerName(SDL_GameController *gamecontroller) { (void)gamecontroller; return "Miyoo Mini Plus"; }
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *gamecontroller) { return gamecontroller ? &s_padJoystick : NULL; }
SDL_GameControllerType SDL_GameControllerGetType(SDL_GameController *gamecontroller)
{
	(void)gamecontroller;
	return SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO;
}

Uint8 SDL_GameControllerGetButton(SDL_GameController *gamecontroller, SDL_GameControllerButton button)
{
	(void)gamecontroller;
	return (unsigned)button < SDL_CONTROLLER_BUTTON_MAX ? s_padButtons[button] : 0;
}

Sint16 SDL_GameControllerGetAxis(SDL_GameController *gamecontroller, SDL_GameControllerAxis axis)
{
	(void)gamecontroller;
	return axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? s_padTriggers[0] :
		axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? s_padTriggers[1] : 0;
}

/* SDL's names (SDL_gamecontroller.c map_StringForControllerButton / Axis) */
static const char *const s_buttonNames[] = {
	"a", "b", "x", "y", "back", "guide", "start", "leftstick", "rightstick", "leftshoulder",
	"rightshoulder", "dpup", "dpdown", "dpleft", "dpright", "misc1", "paddle1", "paddle2",
	"paddle3", "paddle4", "touchpad",
};
static const char *const s_axisNames[] = { "leftx", "lefty", "rightx", "righty", "lefttrigger", "righttrigger" };

SDL_GameControllerButton SDL_GameControllerGetButtonFromString(const char *str)
{
	if (!str)
		return SDL_CONTROLLER_BUTTON_INVALID;
	for (int i = 0; i < (int)(sizeof(s_buttonNames) / sizeof(s_buttonNames[0])) && i < SDL_CONTROLLER_BUTTON_MAX; i++)
		if (!strcasecmp(str, s_buttonNames[i]))
			return (SDL_GameControllerButton)i;
	return SDL_CONTROLLER_BUTTON_INVALID;
}

const char *SDL_GameControllerGetStringForButton(SDL_GameControllerButton button)
{
	if ((int)button >= 0 && (int)button < (int)(sizeof(s_buttonNames) / sizeof(s_buttonNames[0])))
		return s_buttonNames[button];
	return NULL;
}

SDL_GameControllerAxis SDL_GameControllerGetAxisFromString(const char *str)
{
	if (!str)
		return SDL_CONTROLLER_AXIS_INVALID;
	if (*str == '+' || *str == '-')
		str++;
	for (int i = 0; i < (int)(sizeof(s_axisNames) / sizeof(s_axisNames[0])); i++)
		if (!strcasecmp(str, s_axisNames[i]))
			return (SDL_GameControllerAxis)i;
	return SDL_CONTROLLER_AXIS_INVALID;
}
