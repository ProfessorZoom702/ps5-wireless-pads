#include "turbo.h"
#include "config.h"
#include "log.h"
#include "pad.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void turbo_defaults(turbo_cfg *c)
{
    c->on = 0;
    c->mask = 0;
    c->hz = TURBO_HZ_DEFAULT;
}

int turbo_set(turbo_cfg *c, int on, uint32_t mask, int hz)
{
    if (hz < TURBO_HZ_MIN || hz > TURBO_HZ_MAX) return 0;
    if (on && !mask) return 0;
    c->on = on ? 1 : 0;
    c->mask = mask & ~(uint32_t)PAD_PS;     /* PS is never mashed */
    c->hz = hz;
    return 1;
}

int turbo_load(turbo_cfg *c, const char *path)
{
    char line[160];
    FILE *f;
    turbo_cfg n;

    turbo_defaults(c);
    if (!path || !(f = fopen(path, "r"))) return 1;
    n = *c;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), *val, *e;
        if (!eq || line[0] == '#') continue;
        *eq = '\0';
        val = eq + 1;
        while (*val == ' ') val++;
        e = val + strlen(val);
        while (e > val && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = '\0';
        for (e = eq; e > line && e[-1] == ' '; ) *--e = '\0';
        if (strcmp(line, "on") == 0) n.on = atoi(val) ? 1 : 0;
        else if (strcmp(line, "hz") == 0) n.hz = atoi(val);
        else if (strcmp(line, "buttons") == 0) {
            uint32_t m = 0;
            if (!*val || config_parse_buttons(val, &m)) n.mask = m;
        }
    }
    fclose(f);
    if (!turbo_set(c, n.on, n.mask, n.hz))
        log_line("turbo: %s is not valid, mashing stays off", path);
    return 1;
}

int turbo_save(const turbo_cfg *c, const char *path)
{
    char names[160];
    FILE *f;

    if (!path || !(f = fopen(path, "w"))) return 0;
    config_buttons_key(c->mask, names, sizeof names);
    fprintf(f, "# PadBridge hold to mash, written by the web page\non = %d\nbuttons = %s\nhz = %d\n",
            c->on, names, c->hz);
    return fclose(f) == 0;
}

int turbo_filter(const turbo_cfg *c, turbo_track *t, long now,
                 uint32_t *buttons, uint8_t *l2, uint8_t *r2)
{
    uint32_t held;
    long half;

    if (!c->on || !c->mask || !(held = *buttons & c->mask)) {
        t->since = 0;
        return -1;
    }
    if (!t->since) t->since = now ? now : 1;
    half = 500 / (c->hz > 0 ? c->hz : TURBO_HZ_DEFAULT);
    if (half < 1) half = 1;
    if (((now - t->since) / half) % 2 == 0) return 1;
    *buttons &= ~held;
    if (held & PAD_L2) *l2 = 0;
    if (held & PAD_R2) *r2 = 0;
    return 0;
}
