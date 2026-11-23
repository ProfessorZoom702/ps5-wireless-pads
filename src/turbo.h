/* Hold to mash: while a chosen button is held, the console sees it pressed
 * and released several times a second, for games that ask for rapid taps
 * (doors, chests, fountains). Off unless turned on from the web page.
 *
 * Saved in /data/padbridge/turbo.ini:
 *
 *   on      = 1
 *   buttons = r2,circle     names as in config.h
 *   hz      = 10            taps per second (TURBO_HZ_MIN..TURBO_HZ_MAX)
 */
#ifndef PADBRIDGE_TURBO_H
#define PADBRIDGE_TURBO_H

#include <stddef.h>
#include <stdint.h>

#define TURBO_HZ_MIN      4
#define TURBO_HZ_MAX     20
#define TURBO_HZ_DEFAULT 10

typedef struct {
    int on;
    uint32_t mask;          /* PAD_* bits that mash while held */
    int hz;
} turbo_cfg;

/* Per pad: when the mashing buttons were first held (0: not held). */
typedef struct {
    long since;
} turbo_track;

void turbo_defaults(turbo_cfg *c);

/* Missing file: defaults, returns 1. Returns 0 only if it exists but cannot be read. */
int  turbo_load(turbo_cfg *c, const char *path);
int  turbo_save(const turbo_cfg *c, const char *path);

/* Validates and applies; returns 0 (nothing changed) for an out-of-range hz
 * or a mask with no buttons while on. */
int  turbo_set(turbo_cfg *c, int on, uint32_t mask, int hz);

/* Applies mashing to one outgoing frame. The first half-period of a hold is
 * "pressed", so a short tap still reaches the game. Returns -1 if nothing
 * that mashes is held, else 1 (pressed this instant) or 0 (released). */
int  turbo_filter(const turbo_cfg *c, turbo_track *t, long now,
                  uint32_t *buttons, uint8_t *l2, uint8_t *r2);

#endif
