/* SDL2's callback audio (SDL_OpenAudioDevice & co.) on the Miyoo Mini (Plus): the engine mixes
 * in its callback (src/presentation/audio/audio.cpp); a thread of ours calls it for blocks of
 * 256 frames and sends them to Onion's audioserver.
 *
 * From quake2-miyoomini's miyoo/platform/miyoo_audio.c (2026-11-06), itself redriver2-miyoo's
 * audioserver code (src_rebuild/platform/Miyoo/al_miyoo.cpp, MIT); changed here to the device
 * API (SDL_OpenAudioDevice, one device, id 1). The device's sound goes through audioserver (it
 * owns MI_AO; never stop it). This speaks its protocol directly, so no libpadsp preload:
 *   /tmp/audio_fifo_server     PCM, as written to /dev/dsp
 *   /tmp/audio_fifo_ioctl_req  OSS ioctls: u32 request, 16 bytes of argument, u32 its size
 *   /tmp/audio_fifo_ioctl_res  answers: i32 result, i32 errno, 20 bytes of argument
 * The format is S16 stereo 48 kHz (the engine's default rate); others are refused.
 * Without audioserver or with MIYOO_AUDIO=0 the device doesn't open and the game runs silent. */

#include <SDL.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/soundcard.h>
#include <unistd.h>

#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ 1031
#endif

#define OUT_RATE	48000	/* audioserver's rate: no resampling there */
#define OUT_BLOCK	256		/* frames per callback (5.3 ms) */
#define PIPE_BYTES	4096	/* the FIFO's size (libpadsp sets it small too): ~21 ms queued */
#define DEVICE_ID	1

static int s_fd = -1, s_req = -1, s_res = -1;
static pthread_t s_thread;
static volatile int s_running, s_paused = 1;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static SDL_AudioCallback s_callback;
static void *s_userdata;

/* ---- audioserver (redriver2-miyoo) */

static int WaitFd(int fd, short events, int ms)
{
	/* (a signal is not a timeout: wait again) */
	struct pollfd p = { fd, events, 0 };
	int r;
	do
		r = poll(&p, 1, ms);
	while (r < 0 && errno == EINTR);
	return r == 1;
}

/* one OSS ioctl through audioserver; -1 when it doesn't answer */
static int ServerIoctl(uint32_t code, void *arg, uint32_t size)
{
	uint8_t req[24], res[28];
	int got = 0, ret;

	memset(req, 0, sizeof(req));
	memcpy(req, &code, 4);
	if (arg && size)
		memcpy(req + 4, arg, size);
	memcpy(req + 20, &size, 4);

	if (write(s_req, req, sizeof(req)) != (ssize_t)sizeof(req))
		return -1;
	while (got < (int)sizeof(res))
	{
		if (!WaitFd(s_res, POLLIN, 500))
			return -1;
		const ssize_t n = read(s_res, res + got, sizeof(res) - got);
		if (n <= 0)
			return -1;
		got += (int)n;
	}
	memcpy(&ret, res, 4);
	if (arg && size)
		memcpy(arg, res + 8, size);
	return ret;
}

static void CloseServer(void)
{
	if (s_fd >= 0) close(s_fd);
	if (s_req >= 0) close(s_req);
	if (s_res >= 0) close(s_res);
	s_fd = s_req = s_res = -1;
}

static int OpenServer(void)
{
	const char *env = getenv("MIYOO_AUDIO");
	int fmt = AFMT_S16_LE, channels = 2, rate = OUT_RATE;

	if (env && !strcmp(env, "0"))
		return 0;

	/* (O_RDWR, as libpadsp: opening a FIFO so never waits for the other end) */
	s_fd = open("/tmp/audio_fifo_server", O_RDWR | O_NONBLOCK | O_CLOEXEC);
	s_req = open("/tmp/audio_fifo_ioctl_req", O_RDWR | O_NONBLOCK | O_CLOEXEC);
	s_res = open("/tmp/audio_fifo_ioctl_res", O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (s_fd < 0 || s_req < 0 || s_res < 0)
	{
		CloseServer();
		fprintf(stderr, "miyoo: no audioserver (no /tmp/audio_fifo_*): silent\n");
		return 0;
	}
	fcntl(s_fd, F_SETPIPE_SZ, PIPE_BYTES);

	if (ServerIoctl(SNDCTL_DSP_SETFMT, &fmt, 4) < 0 ||
		ServerIoctl(SNDCTL_DSP_CHANNELS, &channels, 4) < 0 ||
		ServerIoctl(SNDCTL_DSP_SPEED, &rate, 4) < 0)
	{
		CloseServer();
		fprintf(stderr, "miyoo: audioserver doesn't answer: silent\n");
		return 0;
	}
	fprintf(stderr, "miyoo: sound through audioserver, format 0x%x, %d channels, %d Hz\n", fmt, channels, rate);
	if (fmt != AFMT_S16_LE || channels != 2 || rate != OUT_RATE)
	{
		CloseServer();
		return 0;
	}
	return 1;
}

/* ---- the callback thread */

static void *AudioThread(void *arg)
{
	static int16_t out[OUT_BLOCK * 2];
	(void)arg;
	prctl(PR_SET_NAME, "sdl_audio");

	while (s_running)
	{
		size_t off = 0;

		pthread_mutex_lock(&s_lock);
		if (s_paused || !s_callback)
			memset(out, 0, sizeof(out));
		else
			s_callback(s_userdata, (Uint8 *)out, sizeof(out));
		pthread_mutex_unlock(&s_lock);

		/* the FIFO's room paces this thread; audioserver gone: drop the block, don't hang */
		while (s_running && off < sizeof(out))
		{
			const ssize_t n = write(s_fd, (const char *)out + off, sizeof(out) - off);
			if (n > 0)
				off += n;
			else if (n < 0 && errno != EAGAIN && errno != EINTR)
				break;
			else if (!WaitFd(s_fd, POLLOUT, 200))
				break;
		}
	}
	return NULL;
}

/* ---- SDL */

static char s_error[160];

const char *Miyoo_AudioError(void) { return s_error; }

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture, const SDL_AudioSpec *desired,
	SDL_AudioSpec *obtained, int allowed_changes)
{
	(void)device;
	(void)allowed_changes;
	if (s_running || iscapture)
		return 0;
	if (desired->freq != OUT_RATE || desired->format != AUDIO_S16SYS || desired->channels != 2)
	{
		snprintf(s_error, sizeof(s_error), "sound must be 48000 Hz 16-bit stereo, not %d Hz 0x%x %d ch",
			desired->freq, desired->format, desired->channels);
		return 0;
	}
	if (!OpenServer())
	{
		snprintf(s_error, sizeof(s_error), "no audioserver");
		return 0;
	}

	s_callback = desired->callback;
	s_userdata = desired->userdata;
	s_paused = 1;
	if (obtained)
	{
		*obtained = *desired;
		obtained->samples = OUT_BLOCK;
		obtained->size = OUT_BLOCK * 4;
	}
	s_running = 1;
	if (pthread_create(&s_thread, NULL, AudioThread, NULL) != 0)
	{
		s_running = 0;
		CloseServer();
		return 0;
	}
	return DEVICE_ID;
}

void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on)
{
	if (dev != DEVICE_ID)
		return;
	pthread_mutex_lock(&s_lock);
	s_paused = pause_on;
	pthread_mutex_unlock(&s_lock);
}

void SDL_LockAudioDevice(SDL_AudioDeviceID dev) { (void)dev; pthread_mutex_lock(&s_lock); }
void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) { (void)dev; pthread_mutex_unlock(&s_lock); }

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev)
{
	if (dev != DEVICE_ID || !s_running)
		return;
	s_running = 0;
	pthread_join(s_thread, NULL);
	CloseServer();
	s_callback = NULL;
}

const char *SDL_GetCurrentAudioDriver(void)
{
	return s_running ? "audioserver" : NULL;
}
