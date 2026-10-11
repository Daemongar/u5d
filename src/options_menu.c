#include "common/common.h"
#include "common/options.h"
#include "funcs.h"
#include "macros.h"
#include "vars.h"

#include "options_menu.h"
#include "graphics/grap_buf.h"
#include "key/key.h"
#include "time/time.h"

#if defined(TARGET_SDL)

#include <string.h>

// Options screen reached from the title menu. Drawn with the game's font and
// frame like the rest of the title screens: three pages of settings, changed
// with the arrow keys and saved to ultima5.ini on leaving.

void GRAP_SDL_ApplyDisplayOptions(void);
void AUDIO_SDL_ApplySoundOptions(void);

enum
{
    ITEM_TOGGLE,
    ITEM_VOLUME,
    ITEM_WINDOW_MODE,
};

enum
{
    PAGE_DISPLAY,
    PAGE_SOUND,
    PAGE_GAMEPLAY,
    PAGE_COUNT,
};

typedef struct OptionItem
{
    int page;
    int kind;
    char* label;
    bool* flag;  // ITEM_TOGGLE
    int* value;  // ITEM_VOLUME, ITEM_WINDOW_MODE
} OptionItem;

// Settings shown on the screen. Others (such as bard_mana_roundup) can only be
// changed in ultima5.ini.
static OptionItem s_items[] =
{
    {PAGE_DISPLAY, ITEM_WINDOW_MODE, "Window mode", NULL, &g_options.windowMode},
    {PAGE_DISPLAY, ITEM_TOGGLE, "Keep 4:3 shape", &g_options.keepAspect, NULL},
    {PAGE_DISPLAY, ITEM_TOGGLE, "Full-color tiles", &g_options.colorTiles, NULL},
    {PAGE_DISPLAY, ITEM_TOGGLE, "Softer palette", &g_options.enhancedPalette, NULL},
    {PAGE_DISPLAY, ITEM_TOGGLE, "Wide view (16:9)", &g_options.wideView, NULL},

    {PAGE_SOUND, ITEM_TOGGLE, "Music", &g_options.music, NULL},
    {PAGE_SOUND, ITEM_VOLUME, "Music volume", NULL, &g_options.musicVolume},
    {PAGE_SOUND, ITEM_TOGGLE, "Sound effects", &g_options.soundEffects, NULL},
    {PAGE_SOUND, ITEM_VOLUME, "Effects volume", NULL, &g_options.soundVolume},
    {PAGE_SOUND, ITEM_TOGGLE, "Terrain footsteps", &g_options.terrainFootsteps, NULL},

    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Mouse aiming", &g_options.mouseAim, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Dialog improvements", &g_options.dialogImprovements, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Smarter monsters", &g_options.smarterAi, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Party shares exp.", &g_options.partyExp, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Group exp. bonus", &g_options.groupExpBonus, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Brighter nights", &g_options.brighterNight, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Esc asks to quit", &g_options.quickExit, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Climbing costs time", &g_options.terrainElevation, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Class stat growth", &g_options.classStatGrowth, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Varied battlefields", &g_options.variedCombat, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Vas Mani in combat", &g_options.vasManiInCombat, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "No wasted casts", &g_options.noWastedCasts, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Inn poison not fatal", &g_options.noInnPoisonDeath, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Stat point per level", &g_options.statPerLevel, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Vanishing rings", &g_options.vanishingRings, NULL},
    {PAGE_GAMEPLAY, ITEM_TOGGLE, "Ultima IV class mana", &g_options.u4ClassMana, NULL},
};

static char* s_pageNames[PAGE_COUNT] = {"Display", "Sound", "Gameplay"};
static char* s_windowModeNames[] = {"Fullscreen", "Bordered", "Windowed"};

#define SCREEN_RIGHT 0x27
#define SCREEN_BOTTOM 0x18
#define FIRST_ITEM_ROW 5
#define LABEL_COLUMN 3
#define VALUE_COLUMN 26

static void PrintAt(int x, int y, char* text)
{
    ULTIMA_1bf2_SetTextPosition(x, y);
    ULTIMA_1850_PrintString(text);
}

static void PrintCentered(int y, char* text)
{
    PrintAt((SCREEN_RIGHT + 1 - (int)strlen(text)) / 2, y, text);
}

// The game's frame characters around the whole screen, as the title menu draws
// around its box (see INTRO_04e0_DrawMenuBorders).
static void DrawFrame(void)
{
    ULTIMA_1cca_SetTextForegroundColor(D_13b2_frame_color);

    ULTIMA_1bf2_SetTextPosition(0, 0);
    ULTIMA_16ba_PrintChar(0x7b);
    for (int i = 1; i < SCREEN_RIGHT; i++)
        ULTIMA_16ba_PrintChar(0x7f);
    ULTIMA_16ba_PrintChar(0x7c);

    for (int y = 1; y < SCREEN_BOTTOM; y++)
    {
        ULTIMA_1bf2_SetTextPosition(0, y);
        ULTIMA_16ba_PrintChar(0x7f);
        ULTIMA_1bf2_SetTextPosition(SCREEN_RIGHT, y);
        ULTIMA_16ba_PrintChar(0x7f);
    }

    ULTIMA_1bf2_SetTextPosition(0, SCREEN_BOTTOM);
    ULTIMA_16ba_PrintChar(0x7d);
    for (int i = 1; i < SCREEN_RIGHT; i++)
        ULTIMA_16ba_PrintChar(0x7f);
    D_538e = 0; // don't advance (and scroll) past the last cell
    ULTIMA_16ba_PrintChar(0x7e);
    D_538e = 1;

    ULTIMA_1cca_SetTextForegroundColor(D_13b0_white_color);
}

static void ValueText(OptionItem* item, char* out)
{
    switch (item->kind)
    {
    case ITEM_TOGGLE:
        strcpy(out, *item->flag ? "On" : "Off");
        break;

    case ITEM_VOLUME:
        // A bar of 10 cells, filled up to the volume.
        for (int i = 0; i < 10; i++)
            out[i] = i < *item->value ? '*' : '.';
        out[10] = '\0';
        break;

    case ITEM_WINDOW_MODE:
        strcpy(out, s_windowModeNames[*item->value]);
        break;
    }
}

static int PageItems(int page, OptionItem** items)
{
    int count = 0;
    for (int i = 0; i < ARRAYSIZE(s_items); i++)
    {
        if (s_items[i].page == page)
            items[count++] = &s_items[i];
    }
    return count;
}

// Most items on any page; rows below a short page's items are blanked.
#define MAX_PAGE_ITEMS 16
#define LABEL_WIDTH 20
#define VALUE_WIDTH 11

// Print text padded with spaces to width, so it overwrites what was there.
static void PrintPadded(char* text, int width)
{
    int n = (int)strlen(text);
    ULTIMA_1850_PrintString(text);
    for (; n < width; n++)
        ULTIMA_16ba_PrintChar(0x20);
}

// Frame, title and help text: drawn once when the screen opens.
static void DrawStatic(void)
{
    ULTIMA_16ba_PrintChar(CTRL_CHAR_CLEAR_WINDOW);
    DrawFrame();
    PrintCentered(1, "Options");
    PrintCentered(SCREEN_BOTTOM - 2, "Arrows: choose and change");
    PrintCentered(SCREEN_BOTTOM - 1, "Tab: next page   Esc: done");
}

static void DrawTabs(int page)
{
    int x = 6;
    for (int p = 0; p < PAGE_COUNT; p++)
    {
        ULTIMA_1bf2_SetTextPosition(x, 3);
        if (p == page)
            ULTIMA_16ba_PrintChar(CTRL_CHAR_TOGGLE_INVERSION);
        ULTIMA_16ba_PrintChar(0x20);
        ULTIMA_1850_PrintString(s_pageNames[p]);
        ULTIMA_16ba_PrintChar(0x20);
        if (p == page)
            ULTIMA_16ba_PrintChar(CTRL_CHAR_TOGGLE_INVERSION);
        x += (int)strlen(s_pageNames[p]) + 4;
    }
}

// One setting line: the label (inverted when selected) and its value.
static void DrawItemRow(OptionItem* item, int row, bool selected)
{
    char text[32];
    int y = FIRST_ITEM_ROW + row;

    ULTIMA_1bf2_SetTextPosition(LABEL_COLUMN - 1, y);
    if (selected)
        ULTIMA_16ba_PrintChar(CTRL_CHAR_TOGGLE_INVERSION);
    ULTIMA_16ba_PrintChar(0x20);
    PrintPadded(item->label, LABEL_WIDTH);
    ULTIMA_16ba_PrintChar(0x20);
    if (selected)
        ULTIMA_16ba_PrintChar(CTRL_CHAR_TOGGLE_INVERSION);

    ValueText(item, text);
    ULTIMA_1bf2_SetTextPosition(VALUE_COLUMN, y);
    PrintPadded(text, VALUE_WIDTH);
}

static void DrawItems(OptionItem** items, int count, int selected)
{
    for (int row = 0; row < MAX_PAGE_ITEMS; row++)
    {
        if (row < count)
        {
            DrawItemRow(items[row], row, row == selected);
        }
        else
        {
            ULTIMA_1bf2_SetTextPosition(LABEL_COLUMN - 1, FIRST_ITEM_ROW + row);
            PrintPadded("", VALUE_COLUMN + VALUE_WIDTH - (LABEL_COLUMN - 1));
        }
    }
}

static void ChangeItem(OptionItem* item, int direction)
{
    switch (item->kind)
    {
    case ITEM_TOGGLE:
        *item->flag = !*item->flag;
        break;

    case ITEM_VOLUME:
        *item->value += direction;
        if (*item->value < 0)
            *item->value = 0;
        if (*item->value > 10)
            *item->value = 10;
        break;

    case ITEM_WINDOW_MODE:
        *item->value = (*item->value + direction + ARRAYSIZE(s_windowModeNames)) % ARRAYSIZE(s_windowModeNames);
        break;
    }

    if (item->page == PAGE_DISPLAY)
        GRAP_SDL_ApplyDisplayOptions();
    else if (item->page == PAGE_SOUND)
        AUDIO_SDL_ApplySoundOptions();
}

static bool s_menuOpen;

extern bool g_presentFullScreenPage;

// Wait for a key without the game's own key wait (ULTIMA_266c_GetChar), which
// updates the game frame while idle and would redraw the map over the menu.
static int ReadKey(void)
{
    int key;
    while ((key = KEY_PollKey()) == 0)
    {
        TIME_SleepMs(10);
    }
    return key;
}

void OPTIONS_MENU_Run(void)
{
    OptionItem* items[ARRAYSIZE(s_items)];
    int page = PAGE_DISPLAY;
    int selected = 0;

    s_menuOpen = true;
    g_presentFullScreenPage = true; // full-screen page: centered in the wide view

    ULTIMA_1c22_SetTextWindowSize(0, 0, 0, SCREEN_RIGHT, SCREEN_BOTTOM);
    ULTIMA_1b94_SelectTextWindow(0);
    ULTIMA_0c22_GRAP_0f_SelectPage(0);

    DrawStatic();
    int count = PageItems(page, items);
    DrawTabs(page);
    DrawItems(items, count, selected);

    for (;;)
    {
        int previous = selected;
        int key = ReadKey();

        switch (key)
        {
        case U5_KEY_UP:
            selected = (selected + count - 1) % count;
            DrawItemRow(items[previous], previous, false);
            DrawItemRow(items[selected], selected, true);
            break;

        case U5_KEY_DOWN:
            selected = (selected + 1) % count;
            DrawItemRow(items[previous], previous, false);
            DrawItemRow(items[selected], selected, true);
            break;

        case U5_KEY_LEFT:
            ChangeItem(items[selected], -1);
            DrawItemRow(items[selected], selected, true);
            break;

        case U5_KEY_RIGHT:
        case U5_KEY_ENTER:
        case U5_KEY_SPACE:
            ChangeItem(items[selected], 1);
            DrawItemRow(items[selected], selected, true);
            break;

        case '\t':
        case U5_KEY_PGDN:
        case U5_KEY_PGUP:
            page = (page + (key == U5_KEY_PGUP ? PAGE_COUNT - 1 : 1)) % PAGE_COUNT;
            selected = 0;
            count = PageItems(page, items);
            DrawTabs(page);
            DrawItems(items, count, selected);
            break;

        case U5_KEY_ESC:
            OPTIONS_Save();
            ULTIMA_16ba_PrintChar(CTRL_CHAR_CLEAR_WINDOW);
            s_menuOpen = false;
            g_presentFullScreenPage = false;
            return;
        }
    }
}

// Text output state the Options screen changes; saved and restored around it.
typedef struct TextState
{
    TextWindow windows[4];
    u16 windowIdx;
    u16 charsetIdx;
    u16 advance;
    u8* charset;
    TextWindow* window;
    u16 underline;
    u16 flag53a6;
    u16 inverse;
    u8 fg;
    u8 bg;
    VideoDriverParams video;
} TextState;

// Opened with F1 while the game waits for a key: saves the screen and text
// state, runs the Options screen, then puts everything back.
void OPTIONS_MENU_RunInGame(void)
{
    TextState saved;

    if (s_menuOpen)
        return;

    memcpy(saved.windows, D_535e_textWindows, sizeof(saved.windows));
    saved.windowIdx = D_5386_current_text_window_idx;
    saved.charsetIdx = D_5388_current_charset_idx;
    saved.advance = D_538e;
    saved.charset = D_5398_currentCharset;
    saved.window = D_539a_currentTextWindow;
    saved.underline = D_53a4_underline;
    saved.flag53a6 = D_53a6;
    saved.inverse = D_53a8_inverse;
    saved.fg = D_53aa_text_fg_color;
    saved.bg = D_53ab_text_bg_color;
    saved.video = D_52ba_vdp;
    GRAP_BUF_SaveScreen();
    GRAP_BUF_ClearColorLayer();

    // Start from plain white text in window 0, whatever the game was doing.
    D_53a4_underline = 0;
    D_53a6 = 0;
    D_53a8_inverse = 0;
    ULTIMA_1c9e_SelectCharset(0);

    OPTIONS_MENU_Run();

    GRAP_BUF_RestoreScreen();
    memcpy(D_535e_textWindows, saved.windows, sizeof(saved.windows));
    D_5386_current_text_window_idx = saved.windowIdx;
    D_5388_current_charset_idx = saved.charsetIdx;
    D_538e = saved.advance;
    D_5398_currentCharset = saved.charset;
    D_539a_currentTextWindow = saved.window;
    D_53a4_underline = saved.underline;
    D_53a6 = saved.flag53a6;
    D_53a8_inverse = saved.inverse;
    D_53aa_text_fg_color = saved.fg;
    D_53ab_text_bg_color = saved.bg;
    D_52ba_vdp = saved.video;
}

#endif
