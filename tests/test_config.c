/* config, hotkey and netinfo. */
#include "../src/config.h"
#include "../src/hotkey.h"
#include "../src/netinfo.h"
#include "../src/turbo.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void test_config(void)
{
    padbridge_config c;
    uint32_t m;
    char text[96];
    FILE *f;

    printf("config: defaults, parsing, bad lines\n");
    config_defaults(&c);
    CHECK(c.menu_combo == (PAD_OPTIONS | PAD_L2 | PAD_R2) && c.menu_hold_ms == 1500);
    config_combo_text(c.menu_combo, text, sizeof text);
    CHECK(strcmp(text, "L2 + R2 + OPTIONS") == 0);

    CHECK(config_parse_combo("L1+R1", &m) && m == (PAD_L1 | PAD_R1));
    CHECK(config_parse_combo("options + create", &m) && m == (PAD_OPTIONS | PAD_CREATE));
    CHECK(!config_parse_combo("l1", &m));               /* one button: refused */
    CHECK(!config_parse_combo("l1+l1", &m));
    CHECK(!config_parse_combo("l1+ps", &m));            /* the PS button cannot be read */
    CHECK(!config_parse_combo("l1+banana", &m));
    CHECK(!config_parse_combo("", &m));

    CHECK(config_load(&c, "build/no_such_config.ini") == 0);        /* missing: fine */
    f = fopen("build/test_config.ini", "w");
    fputs("# comment\nmenu_combo = l2+r2+triangle\nmenu_hold_ms = 2000\n"
          "menu_hold_ms = 99\nmenu_combo = l1\nnonsense\nunknown_key = 1\n", f);
    fclose(f);
    CHECK(config_load(&c, "build/test_config.ini") == 4);
    CHECK(c.menu_combo == (PAD_L2 | PAD_R2 | PAD_TRIANGLE));        /* the bad later ones did not win */
    CHECK(c.menu_hold_ms == 2000);
}

static void test_hotkey(void)
{
    const uint32_t combo = PAD_OPTIONS | PAD_L2 | PAD_R2;
    hotkey h;
    long t = 10000;

    printf("hotkey: held long enough, once, with letting go and a cooldown\n");
    hotkey_init(&h, combo, 1500);
    CHECK(!hotkey_update(&h, 1, combo, t));
    CHECK(!hotkey_update(&h, 1, combo, t + 1400));
    CHECK(hotkey_update(&h, 1, combo, t + 1500));                   /* fires */
    CHECK(!hotkey_update(&h, 1, combo, t + 3000));                  /* still held: not again */
    CHECK(!hotkey_update(&h, 1, 0, t + 3100));                      /* let go */
    CHECK(!hotkey_update(&h, 1, combo, t + 3200));                  /* held again, cooldown */
    CHECK(!hotkey_update(&h, 1, combo, t + 4800));
    CHECK(hotkey_update(&h, 1, combo, t + 6600));                   /* held 3.4 s, cooldown over */

    hotkey_init(&h, combo, 1500);
    t = 50000;
    hotkey_update(&h, 1, combo, t);
    CHECK(!hotkey_update(&h, 1, PAD_L2 | PAD_R2, t + 1000));        /* one let go: restart */
    CHECK(!hotkey_update(&h, 1, combo, t + 1100));
    CHECK(!hotkey_update(&h, 1, combo, t + 2500));                  /* only 1.4 s since the restart */
    CHECK(hotkey_update(&h, 1, combo, t + 2600));

    hotkey_init(&h, combo, 1500);
    t = 90000;
    hotkey_update(&h, 1, combo, t);
    CHECK(!hotkey_update(&h, 0, 0, t + 1000));                      /* the pad stopped answering */
    CHECK(!hotkey_update(&h, 1, combo, t + 2000));                  /* back, but not armed until let go */
    CHECK(!hotkey_update(&h, 1, combo, t + 4000));
    hotkey_update(&h, 1, 0, t + 4100);
    hotkey_update(&h, 1, combo, t + 4200);
    CHECK(hotkey_update(&h, 1, combo, t + 5800));
}

static void test_turbo(void)
{
    turbo_cfg c;
    turbo_track t = { 0 };
    uint32_t b, m;
    uint8_t l2 = 0, r2 = 200;
    char names[64];
    const char *path = "build/test_turbo.ini";

    printf("turbo: parsing, phases, saving\n");
    CHECK(config_parse_buttons("r2", &m) && m == PAD_R2);
    CHECK(config_parse_buttons("R2,circle", &m) && m == (PAD_R2 | PAD_CIRCLE));
    CHECK(!config_parse_buttons("", &m) && !config_parse_buttons("nope", &m));
    config_buttons_key(PAD_R2 | PAD_CIRCLE, names, sizeof names);
    CHECK(strcmp(names, "r2,circle") == 0);

    turbo_defaults(&c);
    CHECK(!c.on && c.hz == TURBO_HZ_DEFAULT);
    CHECK(!turbo_set(&c, 1, 0, 10));                 /* on with no button: refused */
    CHECK(!turbo_set(&c, 1, PAD_R2, 99));
    CHECK(turbo_set(&c, 1, PAD_R2 | PAD_PS, 10) && c.mask == PAD_R2);

    b = PAD_R2 | PAD_CROSS;                          /* 10/s: 50 ms pressed, 50 released */
    CHECK(turbo_filter(&c, &t, 1000, &b, &l2, &r2) == 1 && b == (PAD_R2 | PAD_CROSS) && r2 == 200);
    b = PAD_R2 | PAD_CROSS;
    CHECK(turbo_filter(&c, &t, 1060, &b, &l2, &r2) == 0 && b == PAD_CROSS && r2 == 0);
    b = PAD_R2 | PAD_CROSS; r2 = 200;
    CHECK(turbo_filter(&c, &t, 1110, &b, &l2, &r2) == 1 && b == (PAD_R2 | PAD_CROSS));
    b = PAD_CROSS;                                   /* let go: nothing changes */
    CHECK(turbo_filter(&c, &t, 1160, &b, &l2, &r2) == -1 && b == PAD_CROSS && t.since == 0);
    c.on = 0;
    b = PAD_R2;
    CHECK(turbo_filter(&c, &t, 1200, &b, &l2, &r2) == -1 && b == PAD_R2);

    turbo_set(&c, 1, PAD_R2 | PAD_CIRCLE, 12);
    CHECK(turbo_save(&c, path));
    turbo_defaults(&c);
    CHECK(turbo_load(&c, path) && c.on && c.mask == (PAD_R2 | PAD_CIRCLE) && c.hz == 12);
    remove(path);
    CHECK(turbo_load(&c, path) && !c.on);            /* missing file: off */
}

static void test_netinfo(void)
{
    char ip[16];
    int ok = local_ip(ip);

    printf("netinfo: an address, or loopback without a route\n");
    CHECK(strlen(ip) >= 7 && strlen(ip) <= 15);
    if (!ok) CHECK(strcmp(ip, "127.0.0.1") == 0);
}

int main(void)
{
    test_config();
    test_hotkey();
    test_turbo();
    test_netinfo();
    printf(g_fail ? "%d check(s) failed\n" : "all config checks passed\n", g_fail);
    return g_fail != 0;
}
