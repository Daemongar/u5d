#include "common/common.h"
#include "common/config.h"
#include "common/file.h"

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

#if defined(ENABLE_ENHANCED_PALETTE)
// The same 16 EGA hues/roles (index-for-index), softened: less oversaturated,
// slightly warmer grays/white, so tile art (authored against these exact
// slots) reads the same way but without the harsh raw 0x00/0x55/0xaa/0xff
// stepping of the original EGA DAC.
static Uint32 s_egaPalette[16] =
{
    0xff0a0a0a, 0xff1e3a8a, 0xff15803d, 0xff0e7490, 0xffb91c1c, 0xff86198f, 0xff92400e, 0xffa8a29e,
    0xff57534e, 0xff3b82f6, 0xff4ade80, 0xff22d3ee, 0xfff87171, 0xffe879f9, 0xfffacc15, 0xfffafaf9,
};
#else
static Uint32 s_egaPalette[16] =
{
    0xff000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa, 0xffaa0000, 0xffaa00aa, 0xffaa5500, 0xffaaaaaa,
    0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff, 0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff,
};
#endif

void GRAP_SDL_FlushFrame(void);

void AUDIO_SDL_Init(void);
void AUDIO_SDL_Cleanup(void);

static void LoadColorTileAtlas(void)
{
    FILE* fp = FILE_Open("TILES.BMP", "rb");
    if (fp == NULL)
        return;
    fclose(fp);

    char path[512];
    FILE_ResolvePath("TILES.BMP", path, sizeof(path));

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
    GRAP_BUF_LoadColorTileAtlas(s_colorTileAtlas, s_colorTileColumns, s_colorTileCount);
    fprintf(stderr, "Loaded %d full-color tiles from %s\n", s_colorTileCount, path);
}

typedef enum WindowMode
{
    WINDOW_MODE_FULLSCREEN, // borderless, covering the whole screen
    WINDOW_MODE_BORDERED,   // normal window with title bar, maximized to fill the screen
    WINDOW_MODE_WINDOWED,   // normal resizable window at [window] width x height
} WindowMode;

// [window] mode = fullscreen | bordered | windowed. Without it, the older
// fullscreen=1/0 setting picks fullscreen or bordered.
static WindowMode ReadWindowMode(void)
{
    char mode[32];
    CONFIG_GetString("window", "mode", "", mode, sizeof(mode));

    if (SDL_strcasecmp(mode, "fullscreen") == 0)
        return WINDOW_MODE_FULLSCREEN;
    if (SDL_strcasecmp(mode, "bordered") == 0)
        return WINDOW_MODE_BORDERED;
    if (SDL_strcasecmp(mode, "windowed") == 0)
        return WINDOW_MODE_WINDOWED;

    return CONFIG_GetInt("window", "fullscreen", 1) != 0 ? WINDOW_MODE_FULLSCREEN : WINDOW_MODE_BORDERED;
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
    WindowMode windowMode = ReadWindowMode();

    switch (windowMode)
    {
    case WINDOW_MODE_FULLSCREEN:
        windowFlags |= SDL_WINDOW_FULLSCREEN;
        break;
    case WINDOW_MODE_BORDERED:
        windowFlags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;
        break;
    case WINDOW_MODE_WINDOWED:
        windowFlags |= SDL_WINDOW_RESIZABLE;
        FitWindowToDisplay(&windowWidth, &windowHeight);
        break;
    }

    SDL_CreateWindowAndRenderer("Ultima V: Warriors of Destiny", windowWidth, windowHeight, windowFlags, &s_sdlWindow, &s_sdlRenderer);

    if (windowMode == WINDOW_MODE_WINDOWED)
    {
        SDL_SetWindowPosition(s_sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    // The picture is scaled to the live window size, which in fullscreen and
    // bordered modes differs from the requested width/height.
    SDL_GetWindowSizeInPixels(s_sdlWindow, &windowWidth, &windowHeight);

    s_sdlSurface = SDL_CreateSurface(hiresWidth, hiresHeight, SDL_GetPixelFormatForMasks(32, 0xff0000, 0xff00, 0xff, 0xff000000));

    s_sdlTexture = SDL_CreateTextureFromSurface(s_sdlRenderer, s_sdlSurface);
    SDL_SetTextureScaleMode(s_sdlTexture, SDL_SCALEMODE_LINEAR);

    if (SDL_MUSTLOCK(s_sdlSurface))
    {
        exit(0);
    }

    if (s_sdlSurface->pitch != hiresWidth * 4)
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
    Uint32* pixels = s_sdlSurface->pixels;
    int pitch = s_sdlSurface->pitch / 4;

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

extern void DisplayDebugMessages(void);

void GRAP_SDL_FlushFrame(void)
{
    LinearToRGB();

    SDL_UpdateTexture(s_sdlTexture, NULL, s_sdlSurface->pixels, s_sdlSurface->pitch);

    // Window may be resized or (un)maximized at any time; re-read the live size
    // each frame so the picture always fills the window instead of clipping.
    SDL_GetWindowSizeInPixels(s_sdlWindow, &windowWidth, &windowHeight);

    SDL_FRect srcRect = {0, 0, hiresWidth, hiresHeight};
    SDL_FRect dstRect = {0, 0, windowWidth, windowHeight};
    SDL_RenderTexture(s_sdlRenderer, s_sdlTexture, &srcRect, &dstRect);
    SDL_RenderPresent(s_sdlRenderer);
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
