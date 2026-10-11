#include "common/common.h"
#include "common/options.h"
#include "funcs.h"
#include "macros.h"
#include "tiles.h"
#include "vars.h"

#include "graphics/grap_buf.h"
#include "wide_view.h"

#if defined(TARGET_SDL)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/file.h"

// Wide view, phase 2: which tiles to show in the three extra columns on each
// side of the map (see ComposeFrame in graphics/grap_sdl.c). The game's own
// 11x11 view, with its line of sight and lighting, is left untouched; the extra
// squares get their own, simpler test using the same ingredients:
//   - lighting: a square is lit if its squared distance from the party is at
//     most the light level D_58a5 (as ULTIMA_6ff0/ULTIMA_5a28 do for 11x11).
//     In full daylight (50, which exactly covers the original 11x11) the extra
//     columns are treated as lit, or the wide view would be a circle.
//   - line of sight: no view-blocking tile (D_6a86, see ULTIMA_5dfe) on the
//     straight line between the party and the square.
// Only outdoors and in towns; dungeons and combat keep black strips.

int g_wideExtraTiles[WIDE_EXTRA_ROWS][2 * WIDE_EXTRA_COLUMNS];
bool g_wideExtraTilesReady; // false until the map has been drawn once

#define DAYLIGHT 0x32

// The overworld is kept in memory as a 32x32 block around the party (see
// ULTIMA_4402_GetTileAddr and OUTSUBS_01b4); the extra columns reach past it
// near its edges. For those squares, read the whole world map once, in the
// same chunk layout as the files: BRIT.DAT holds the land chunks listed in
// D_3876 (0xff is open sea, tile 1), UNDER.DAT all 256 chunks in order.
static byte* s_worlds[2];

static const byte* LoadWorld(bool underworld)
{
    byte** world = &s_worlds[underworld ? 1 : 0];
    if (*world != NULL)
        return *world;

    byte* data = malloc(0x10000);
    if (data == NULL)
        return NULL;

    FILE* fp = FILE_Open(underworld ? "UNDER.DAT" : "BRIT.DAT", "rb");
    for (int chunk = 0; chunk < 0x100; chunk++)
    {
        byte* dst = &data[chunk * 0x100];
        int fileChunk = underworld ? chunk : D_3876[chunk];

        memset(dst, 1, 0x100);
        if (fp != NULL && fileChunk != 0xff && fseek(fp, (long)fileChunk * 0x100, SEEK_SET) == 0)
        {
            if (fread(dst, 1, 0x100, fp) != 0x100)
                memset(dst, 1, 0x100);
        }
    }
    if (fp != NULL)
        fclose(fp);

    *world = data;
    return data;
}

static int WorldTile(int x, int y)
{
    const byte* world = LoadWorld(D_5895_map_level > 0x7f);
    x &= 0xff;
    y &= 0xff;
    if (world == NULL)
        return -1;
    return world[((y >> 4) * 0x10 + (x >> 4)) * 0x100 + (y & 0x0f) * 0x10 + (x & 0x0f)];
}

static bool BlocksView(byte tile)
{
    return memchr(D_6a86, tile, sizeof(D_6a86)) != NULL;
}

// World tile at an offset from the party, or -1 off the edge of a town map.
static int TileAt(int dx, int dy)
{
    int x = (int)D_5896_map_x + dx;
    int y = (int)D_5897_map_y + dy;

    if (D_5893_map_id != 0 && (x < 0 || y < 0 || x > 0x1f || y > 0x1f))
        return -1;
    if (D_5893_map_id == 0)
    {
        // Outside the loaded 32x32 block, read the world map instead (the
        // block lookup would wrap around to the far side of the block).
        if (((x - D_589b) & 0xff) >= 0x20 || ((y - D_589c) & 0xff) >= 0x20)
            return WorldTile(x, y);
        x &= 0xff;
        y &= 0xff;
    }
    return *ULTIMA_4402_GetTileAddr(x, y);
}

// Straight line from the party to (dx, dy): visible unless a square strictly
// between them blocks the view.
static bool InLineOfSight(int dx, int dy)
{
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

    for (int i = 1; i < steps; i++)
    {
        // Nearest square to the point i/steps along the line.
        int x = (dx * i * 2 + (dx >= 0 ? steps : -steps)) / (2 * steps);
        int y = (dy * i * 2 + (dy >= 0 ? steps : -steps)) / (2 * steps);
        int tile = TileAt(x, y);

        if (tile < 0 || BlocksView((byte)tile))
            return false;
    }
    return true;
}

static int ExtraTile(int dx, int dy)
{
    int light = D_58a5;

    if (light < DAYLIGHT && dx * dx + dy * dy > light)
        return -1;
    if (!InLineOfSight(dx, dy))
        return -1;

    int tile = TileAt(dx, dy);
    if (tile < 0)
        return -1;

    // Far desert uses the plain sand tile, as ULTIMA_5394 does beyond 5.
    if (tile == TILE_MAP_DD)
        tile = TILE_MAP_1C;

    // Anyone standing there, drawn instead of the terrain like the 11x11 view.
    for (int i = 0; i < 0x20; i++)
    {
        ActorFmt* actor = &D_5c5a[i];
        if (actor->_0_tile != 0 && actor->_1_animTile != 0 && actor->_4_z == D_5895_map_level &&
            actor->_2_x == (byte)(D_5896_map_x + dx) && actor->_3_y == (byte)(D_5897_map_y + dy))
        {
            return 0x100 + actor->_1_animTile;
        }
    }

    return D_b11e[tile];
}

void WIDE_UpdateExtraTiles(void)
{
    // Outdoors (0) and towns (1..0x20) only.
    bool active = g_options.wideView && D_5893_map_id <= 0x20;

    for (int row = 0; row < WIDE_EXTRA_ROWS; row++)
    {
        int dy = row - WIDE_EXTRA_ROWS / 2;
        for (int col = 0; col < 2 * WIDE_EXTRA_COLUMNS; col++)
        {
            // Left strip: 8..6 squares left of the party; right strip: 6..8 right.
            int dx = col < WIDE_EXTRA_COLUMNS ? col - (WIDE_EXTRA_COLUMNS + 5)
                                              : col - WIDE_EXTRA_COLUMNS + 6;
            g_wideExtraTiles[row][col] = active ? ExtraTile(dx, dy) : -1;
        }
    }
    g_wideExtraTilesReady = true;
}

#endif
