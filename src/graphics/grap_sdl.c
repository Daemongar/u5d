#include "common/common.h"
#include "common/config.h"
#include "common/file.h"
#include "common/options.h"

#include "app_icon.h"
#include "grap_buf.h"
#include "grap_ops.h"
#include "origin.h"
#include "wd.h"

#include <SDL3/SDL.h>

static SDL_Window* s_sdlWindow;
static SDL_Renderer* s_sdlRenderer;
static SDL_Surface* s_sdlSurface;
static SDL_Texture* s_sdlTexture;
static Uint32* s_colorTileAtlas;
static int s_colorTileColumns;
static int s_colorTileCount;

int windowWidth = 1280;
int windowHeight = 960;

// Where the picture is drawn in the window, and how it is laid out (for
// mouse mapping in key_sdl.c).
SDL_FRect g_presentRect = {0, 0, 1280, 960};
int g_presentWidth = hiresWidth; // width of the composed picture
int g_presentLayout = PRESENT_LAYOUT_NORMAL;

// The game's own 640x400 picture, before it is laid out for the window.
static Uint32* s_frame;

extern u8 D_5893_map_id;

static Uint32 s_egaPaletteOriginal[16] =
{
    0xff000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa, 0xffaa0000, 0xffaa00aa, 0xffaa5500, 0xffaaaaaa,
    0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff, 0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff,
};

#if defined(ENABLE_ENHANCED_PALETTE)
// The same 16 EGA hues/roles (index-for-index), softened: less oversaturated,
// slightly warmer grays/white, so tile art (authored against these exact
// slots) reads the same way but without the harsh raw 0x00/0x55/0xaa/0xff
// stepping of the original EGA DAC. Used when the enhanced_palette setting is on.
static Uint32 s_egaPaletteEnhanced[16] =
{
    0xff0a0a0a, 0xff1e3a8a, 0xff15803d, 0xff0e7490, 0xffb91c1c, 0xff86198f, 0xff92400e, 0xffa8a29e,
    0xff57534e, 0xff3b82f6, 0xff4ade80, 0xff22d3ee, 0xfff87171, 0xffe879f9, 0xfffacc15, 0xfffafaf9,
};
#endif

static const Uint32* CurrentPalette(void)
{
#if defined(ENABLE_ENHANCED_PALETTE)
    if (g_options.enhancedPalette)
        return s_egaPaletteEnhanced;
#endif
    return s_egaPaletteOriginal;
}

void GRAP_SDL_FlushFrame(void);

void AUDIO_SDL_Init(void);
void AUDIO_SDL_Cleanup(void);

// TILES.BMP is looked for in the game data folder, then next to the program,
// then three folders up from it: the repository root when running a fresh build
// from build/linux/<config>/ or u5win/x64/<config>/.
static bool FindColorTileAtlas(char* path, int size)
{
    FILE* fp = FILE_Open("TILES.BMP", "rb");
    if (fp != NULL)
    {
        fclose(fp);
        FILE_ResolvePath("TILES.BMP", path, size);
        return true;
    }

    const char* base = SDL_GetBasePath();
    if (base == NULL)
        return false;

    static const char* s_candidates[] = {"TILES.BMP", "../../../TILES.BMP"};
    for (int i = 0; i < ARRAYSIZE(s_candidates); i++)
    {
        SDL_snprintf(path, size, "%s%s", base, s_candidates[i]);
        SDL_IOStream* io = SDL_IOFromFile(path, "rb");
        if (io != NULL)
        {
            SDL_CloseIO(io);
            return true;
        }
    }
    return false;
}

static void LoadColorTileAtlas(void)
{
    char path[1024];
    if (!FindColorTileAtlas(path, sizeof(path)))
        return;

    SDL_Surface* source = SDL_LoadBMP(path);
    if (source == NULL)
    {
        fprintf(stderr, "Couldn't load optional %s: %s\n", path, SDL_GetError());
        return;
    }

    if (source->w <= 0 || source->h <= 0 ||
        source->w % tilePixelSize != 0 || source->h % tilePixelSize != 0)
    {
        fprintf(stderr, "%s must be a grid of %dx%d pixel tiles\n", path, tilePixelSize, tilePixelSize);
        SDL_DestroySurface(source);
        return;
    }

    SDL_Surface* converted = SDL_ConvertSurface(source, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(source);
    if (converted == NULL)
    {
        fprintf(stderr, "Couldn't convert %s to 32-bit color: %s\n", path, SDL_GetError());
        return;
    }

    s_colorTileColumns = converted->w / tilePixelSize;
    int rows = converted->h / tilePixelSize;
    s_colorTileCount = s_colorTileColumns * rows;
    size_t pixelCount = (size_t)converted->w * (size_t)converted->h;
    s_colorTileAtlas = malloc(pixelCount * sizeof(*s_colorTileAtlas));
    if (s_colorTileAtlas == NULL)
    {
        fprintf(stderr, "Couldn't allocate color tiles for %s\n", path);
        SDL_DestroySurface(converted);
        s_colorTileCount = 0;
        s_colorTileColumns = 0;
        return;
    }

    for (int y = 0; y < converted->h; y++)
    {
        const Uint32* src = (const Uint32*)((const Uint8*)converted->pixels + y * converted->pitch);
        for (int x = 0; x < converted->w; x++)
        {
            s_colorTileAtlas[y * converted->w + x] = src[x] & 0x00ffffff;
        }
    }

    SDL_DestroySurface(converted);
    if (g_options.colorTiles)
    {
        GRAP_BUF_LoadColorTileAtlas(s_colorTileAtlas, s_colorTileColumns, s_colorTileCount);
    }
    fprintf(stderr, "Loaded %d full-color tiles from %s\n", s_colorTileCount, path);
}

// Shrink a windowed size (keeping its shape) so it fits on the primary display.
static void FitWindowToDisplay(int* width, int* height)
{
    SDL_Rect usable;
    if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
        return;

    int maxWidth = usable.w * 9 / 10;
    int maxHeight = usable.h * 9 / 10;
    if (*width > maxWidth || *height > maxHeight)
    {
        float scale = SDL_min((float)maxWidth / *width, (float)maxHeight / *height);
        *width = (int)(*width * scale);
        *height = (int)(*height * scale);
    }
}

void GRAP_SDL_Initialize(void)
{
    windowWidth = CONFIG_GetInt("window", "width", windowWidth);
    windowHeight = CONFIG_GetInt("window", "height", windowHeight);

    SDL_WindowFlags windowFlags = 0;
    int windowMode = g_options.windowMode;

    switch (windowMode)
    {
    case OPTION_WINDOW_FULLSCREEN:
        windowFlags |= SDL_WINDOW_FULLSCREEN;
        break;
    case OPTION_WINDOW_BORDERED:
        windowFlags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;
        break;
    default:
        windowFlags |= SDL_WINDOW_RESIZABLE;
        FitWindowToDisplay(&windowWidth, &windowHeight);
        break;
    }

    SDL_CreateWindowAndRenderer("Ultima V: Warriors of Destiny", windowWidth, windowHeight, windowFlags, &s_sdlWindow, &s_sdlRenderer);

    // Window and taskbar icon: the ankh from tools/make_icon.py.
    SDL_Surface* icon = SDL_CreateSurfaceFrom(APP_ICON_SIZE, APP_ICON_SIZE, SDL_PIXELFORMAT_RGBA32,
                                              (void*)g_appIcon, APP_ICON_SIZE * 4);
    if (icon != NULL)
    {
        SDL_SetWindowIcon(s_sdlWindow, icon);
        SDL_DestroySurface(icon);
    }

    if (windowMode == OPTION_WINDOW_WINDOWED)
    {
        SDL_SetWindowPosition(s_sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    // The picture is scaled to the live window size, which in fullscreen and
    // bordered modes differs from the requested width/height.
    SDL_GetWindowSizeInPixels(s_sdlWindow, &windowWidth, &windowHeight);

    // Wide enough for the wide view; normal frames use the left 640 columns.
    s_sdlSurface = SDL_CreateSurface(WIDE_RENDER_WIDTH, hiresHeight, SDL_GetPixelFormatForMasks(32, 0xff0000, 0xff00, 0xff, 0xff000000));
    s_frame = malloc(sizeof(Uint32) * hiresWidth * hiresHeight);

    s_sdlTexture = SDL_CreateTextureFromSurface(s_sdlRenderer, s_sdlSurface);
    SDL_SetTextureScaleMode(s_sdlTexture, SDL_SCALEMODE_LINEAR);

    if (SDL_MUSTLOCK(s_sdlSurface))
    {
        exit(0);
    }

    if (s_sdlSurface->pitch != WIDE_RENDER_WIDTH * 4 || s_frame == NULL)
    {
        exit(0);
    }

    GRAP_BUF_Initialize(GRAP_SDL_FlushFrame);
    LoadColorTileAtlas();
}

void GRAP_SDL_Cleanup(void)
{
    GRAP_BUF_Cleanup();
    GRAP_BUF_LoadColorTileAtlas(NULL, 0, 0);
    free(s_colorTileAtlas);
    s_colorTileAtlas = NULL;
    s_colorTileColumns = 0;
    s_colorTileCount = 0;

    free(s_frame);
    s_frame = NULL;
    SDL_DestroyTexture(s_sdlTexture);
    SDL_DestroySurface(s_sdlSurface);
    SDL_DestroyRenderer(s_sdlRenderer);
    SDL_DestroyWindow(s_sdlWindow);

    s_sdlTexture = NULL;
    s_sdlSurface = NULL;
    s_sdlRenderer = NULL;
    s_sdlWindow = NULL;
}

static void LinearToRGB(void)
{
    Uint32* pixels = s_frame;
    int pitch = hiresWidth;
    const Uint32* s_egaPalette = CurrentPalette();

    for (int y = 0; y < renderHeight; y++)
    {
        for (int x = 0; x < renderWidth; x++)
        {
            Uint32 color = s_egaPalette[g_linearEgaBuffer0[y * renderWidth + x] & 0xf];
            int offset = y * renderWidth + x;
            if (g_linearColorMask0 != NULL && g_linearColorMask0[offset])
            {
                color = 0xff000000 | g_linearColorPixels0[offset];
            }
            pixels[y * pitch + x] = color;
        }
    }

#if defined(ENABLE_GRAP_OVERLAY)
    for (int y = 0; y < hiresHeight; y++)
    {
        for (int x = 0; x < hiresWidth; x++)
        {
            byte pixel = g_linearOverlayBuffer[y * hiresWidth + x] & 0xf;
            if (pixel != 0)
            {
                pixels[y * pitch + x] = s_egaPalette[pixel];
            }
            // pixels[y * pitch + x] |= egaPalette[pLinearOverlayBuffer[y * hiresWidth + x] & 0xf];
        }
    }
#endif
}

// Wide view: the game still draws its 640x400 screen. Map screens are laid out
// wider, with the map moved right by WIDE_EXTRA_WIDTH and the stats panel moved
// to the right edge; the strips beside the map are WIDE_EXTRA_WIDTH each
// (black for now, with the frame's top and bottom borders carried across).
// Other screens (title, intro) are centered.
static bool IsMapScreen(void)
{
    return D_5893_map_id != 0x40 && D_5893_map_id != 0x41;
}

static void ComposeFrame(void)
{
    Uint32* out = s_sdlSurface->pixels;
    int pitch = s_sdlSurface->pitch / 4;
    Uint32 black = CurrentPalette()[0];

    if (!g_options.wideView)
    {
        g_presentWidth = hiresWidth;
        g_presentLayout = PRESENT_LAYOUT_NORMAL;
        for (int y = 0; y < hiresHeight; y++)
            memcpy(&out[y * pitch], &s_frame[y * hiresWidth], sizeof(Uint32) * hiresWidth);
        return;
    }

    g_presentWidth = WIDE_RENDER_WIDTH;

    if (!IsMapScreen())
    {
        g_presentLayout = PRESENT_LAYOUT_CENTERED;
        int margin = (WIDE_RENDER_WIDTH - hiresWidth) / 2;
        for (int y = 0; y < hiresHeight; y++)
        {
            Uint32* row = &out[y * pitch];
            for (int x = 0; x < margin; x++)
                row[x] = row[WIDE_RENDER_WIDTH - 1 - x] = black;
            memcpy(&row[margin], &s_frame[y * hiresWidth], sizeof(Uint32) * hiresWidth);
        }
        return;
    }

    g_presentLayout = PRESENT_LAYOUT_WIDE_MAP;
    for (int y = 0; y < hiresHeight; y++)
    {
        Uint32* row = &out[y * pitch];
        const Uint32* src = &s_frame[y * hiresWidth];
        bool borderRow = y < MAP_RENDER_TOP || y >= MAP_RENDER_BOTTOM;

        for (int x = 0; x < WIDE_RENDER_WIDTH; x++)
        {
            int srcX = WideToGameX(x);
            if (srcX >= 0)
                row[x] = src[srcX];
            else if (borderRow)
                row[x] = src[MAP_RENDER_LEFT + (x % 16)]; // carry the frame border across
            else
                row[x] = black;
        }
    }
}

extern void DisplayDebugMessages(void);

void GRAP_SDL_FlushFrame(void)
{
    LinearToRGB();
    ComposeFrame();

    SDL_UpdateTexture(s_sdlTexture, NULL, s_sdlSurface->pixels, s_sdlSurface->pitch);

    // Window may be resized or (un)maximized at any time; re-read the live size
    // each frame so the picture always fills the window instead of clipping.
    SDL_GetWindowSizeInPixels(s_sdlWindow, &windowWidth, &windowHeight);

    SDL_FRect srcRect = {0, 0, (float)g_presentWidth, hiresHeight};
    SDL_FRect dstRect = {0, 0, windowWidth, windowHeight};

    if (g_options.keepAspect)
    {
        // Keep the picture's shape, centered with black bars: 4:3 for the
        // original screen (shown on 4:3 monitors, so 200 lines display as
        // 240), and the same pixel shape for the wide view (about 16:9).
        float aspect = (g_presentWidth / 2.0f) / 240.0f;
        float w = (float)windowWidth;
        float h = (float)windowHeight;
        if (w > h * aspect)
            w = h * aspect;
        else
            h = w / aspect;
        dstRect = (SDL_FRect){(windowWidth - w) / 2.0f, (windowHeight - h) / 2.0f, w, h};
    }

    g_presentRect = dstRect;

    SDL_SetRenderDrawColor(s_sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(s_sdlRenderer);
    SDL_RenderTexture(s_sdlRenderer, s_sdlTexture, &srcRect, &dstRect);
    SDL_RenderPresent(s_sdlRenderer);
}

// Apply display settings changed in the Options screen while the game runs.
void GRAP_SDL_ApplyDisplayOptions(void)
{
    switch (g_options.windowMode)
    {
    case OPTION_WINDOW_FULLSCREEN:
        SDL_SetWindowFullscreen(s_sdlWindow, true);
        break;

    case OPTION_WINDOW_BORDERED:
        SDL_SetWindowFullscreen(s_sdlWindow, false);
        SDL_SetWindowBordered(s_sdlWindow, true);
        SDL_SetWindowResizable(s_sdlWindow, true);
        SDL_MaximizeWindow(s_sdlWindow);
        break;

    default:
    {
        int width = CONFIG_GetInt("window", "width", 1280);
        int height = CONFIG_GetInt("window", "height", 960);
        FitWindowToDisplay(&width, &height);
        SDL_SetWindowFullscreen(s_sdlWindow, false);
        SDL_SetWindowBordered(s_sdlWindow, true);
        SDL_SetWindowResizable(s_sdlWindow, true);
        SDL_RestoreWindow(s_sdlWindow);
        SDL_SetWindowSize(s_sdlWindow, width, height);
        SDL_SetWindowPosition(s_sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        break;
    }
    }
    SDL_SyncWindow(s_sdlWindow);

    GRAP_BUF_LoadColorTileAtlas(g_options.colorTiles ? s_colorTileAtlas : NULL, s_colorTileColumns, s_colorTileCount);
}

static GraphicsDriverOps s_winOps =
{
    .Initialize = GRAP_SDL_Initialize,
    .Cleanup = GRAP_SDL_Cleanup,
    .Present = GRAP_BUF_Present,
    .FlushPendingPresent = GRAP_BUF_FlushPendingPresent,
    .SetPenColor = GRAP_BUF_SetPenColor,
    .SetPage = GRAP_BUF_SetPage,
    .PrintChar = GRAP_BUF_PrintChar,
    .ScrollWindow = GRAP_BUF_ScrollWindow,
    .Line = GRAP_BUF_Line,
    .Pset = GRAP_BUF_Pset,
    .FillWindow = GRAP_BUF_FillWindow,
    .LoadTileset = GRAP_BUF_LoadTileset,
    .UnloadTileset = GRAP_BUF_UnloadTileset,
    .AnimateTileset = GRAP_BUF_AnimateTileset,
    .UpdateTimeTileset = GRAP_BUF_UpdateTimeTileset,
    .ShowNextWDFrame = GRAP_BUF_ShowNextWDFrame,
    .AnimateWD = GRAP_BUF_AnimateWD,
    .AnimateOriginLogo = GRAP_BUF_AnimateOriginLogo,
    .PutAnimatedMoongateTile = GRAP_BUF_PutAnimatedMoongateTile,
    .PutTileRevealStep = GRAP_BUF_PutTileRevealStep,
    .PutTile = GRAP_BUF_PutTile,
    .PutImage = GRAP_BUF_PutImage,
    .PutBitImage = GRAP_BUF_PutBitImage,
    .TransferPage = GRAP_BUF_TransferPage,
    .TransferPage_Reveal = GRAP_BUF_TransferPage_Reveal
};

GraphicsDriverOps* GRAP_SDL_GetOps(void)
{
    return &s_winOps;
}
