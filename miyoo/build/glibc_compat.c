/* From quake2-miyoomini (miyoo/build/glibc_compat.c, itself tyrquake-miyoo's), extended for
 * Olduvai's C++ (libstdc++ linked statically, 2026-11-06). Built without LTO. */
/* stat() became a real symbol in glibc 2.33; glibc 2.28 (the Miyoo) only has __xstat.
 * Linked with -Wl,--wrap=stat. */
#include <sys/stat.h>

extern int __xstat(int ver, const char *path, struct stat *buf);
__asm__(".symver __xstat,__xstat@GLIBC_2.4");

int __wrap_stat(const char *path, struct stat *buf)
{
	return __xstat(3, path, buf); /* _STAT_VER_LINUX */
}

/* ---- Olduvai: what the static libstdc++ (GCC 13) and std::filesystem want from glibc 2.32+,
 * bound to the Miyoo's glibc 2.28 through -Wl,--wrap=<name> (build_miyoo.sh WRAPS). */
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

/* the 64-bit stat calls: __xstat64 & co. in 2.28 (stat64 & co. are 2.33) */
struct stat64;
extern int __xstat64(int ver, const char *path, struct stat64 *buf);
extern int __lxstat64(int ver, const char *path, struct stat64 *buf);
extern int __fxstat64(int ver, int fd, struct stat64 *buf);
__asm__(".symver __xstat64,__xstat64@GLIBC_2.4");
__asm__(".symver __lxstat64,__lxstat64@GLIBC_2.4");
__asm__(".symver __fxstat64,__fxstat64@GLIBC_2.4");
int __wrap_stat64(const char *path, struct stat64 *buf) { return __xstat64(3, path, buf); }
int __wrap_lstat64(const char *path, struct stat64 *buf) { return __lxstat64(3, path, buf); }
int __wrap_fstat64(int fd, struct stat64 *buf) { return __fxstat64(3, fd, buf); }

/* pthreads: in libpthread.so.0 on the Miyoo (2.34 moved them into libc) */
__asm__(".symver pthread_create,pthread_create@GLIBC_2.4");
__asm__(".symver pthread_join,pthread_join@GLIBC_2.4");
__asm__(".symver pthread_detach,pthread_detach@GLIBC_2.4");
__asm__(".symver pthread_once,pthread_once@GLIBC_2.4");
__asm__(".symver pthread_key_create,pthread_key_create@GLIBC_2.4");
__asm__(".symver pthread_key_delete,pthread_key_delete@GLIBC_2.4");
__asm__(".symver pthread_getspecific,pthread_getspecific@GLIBC_2.4");
__asm__(".symver pthread_setspecific,pthread_setspecific@GLIBC_2.4");
__asm__(".symver pthread_mutex_trylock,pthread_mutex_trylock@GLIBC_2.4");
int __wrap_pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg) { return pthread_create(t, a, f, arg); }
int __wrap_pthread_join(pthread_t t, void **ret) { return pthread_join(t, ret); }
int __wrap_pthread_detach(pthread_t t) { return pthread_detach(t); }
int __wrap_pthread_once(pthread_once_t *o, void (*f)(void)) { return pthread_once(o, f); }
int __wrap_pthread_key_create(pthread_key_t *k, void (*d)(void *)) { return pthread_key_create(k, d); }
int __wrap_pthread_key_delete(pthread_key_t k) { return pthread_key_delete(k); }
void *__wrap_pthread_getspecific(pthread_key_t k) { return pthread_getspecific(k); }
int __wrap_pthread_setspecific(pthread_key_t k, const void *v) { return pthread_setspecific(k, v); }
int __wrap_pthread_mutex_trylock(pthread_mutex_t *m) { return pthread_mutex_trylock(m); }

/* C23's strtol (2.38: also takes 0b binary literals; nothing here parses those) */
__asm__(".symver strtol,strtol@GLIBC_2.4");
__asm__(".symver strtoul,strtoul@GLIBC_2.4");
long __wrap___isoc23_strtol(const char *s, char **end, int base) { return strtol(s, end, base); }
unsigned long __wrap___isoc23_strtoul(const char *s, char **end, int base) { return strtoul(s, end, base); }

__asm__(".symver hypotf,hypotf@GLIBC_2.4");
float __wrap_hypotf(float x, float y) { return hypotf(x, y); }

/* std::random_device (2.36's arc4random): the kernel's getrandom, else /dev/urandom */
uint32_t __wrap_arc4random(void)
{
	uint32_t v = 0;
#ifdef SYS_getrandom
	if (syscall(SYS_getrandom, &v, sizeof(v), 0) == (long)sizeof(v))
		return v;
#endif
	{
		const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
		if (fd >= 0)
		{
			if (read(fd, &v, sizeof(v)) != (ssize_t)sizeof(v))
				v = 0;
			close(fd);
		}
	}
	return v;
}

/* libstdc++'s "is the process single-threaded" flag (2.32): ours, always "no", so its shared
 * pointers and locale counts always use atomics (the engine runs threads anyway) */
char __libc_single_threaded = 0;
