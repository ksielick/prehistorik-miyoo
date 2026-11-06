/* The Miyoo Mini (Plus) platform layer of the Olduvai port (miyoo/platform/): the part of SDL2
 * the engine calls, on the device's Linux directly. After quake2-miyoomini's miyoo/platform/. */
#ifndef MIYOO_PLATFORM_H
#define MIYOO_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* miyoo_fb.c: the panel (from quake2-miyoomini) */
int MiyooFb_Open(void);
void MiyooFb_Present(const uint32_t *argb, int w, int h, int pitch);
void MiyooFb_Close(void);
/* frames in memory the 2D engine reads, when it is there (open the panel first) */
uint32_t *MiyooFb_AllocFrame(int w, int h);
void MiyooFb_FreeFrame(uint32_t *px);
/* <= 45: two refreshes a frame, <= 100: one, else unlimited */
void MiyooFb_SetMaxFps(float fps);
int MiyooFb_I8Usable(void);
uint8_t *MiyooFb_GetI8Buffers(int w, int h);
int MiyooFb_PresentI8(const uint8_t *idx, const uint32_t *pal, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
