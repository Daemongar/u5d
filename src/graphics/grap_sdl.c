#include "common/common.h"
#include "common/config.h"

#include "grap_buf.h"
#include "grap_ops.h"
#include "origin.h"
#include "wd.h"

#include <SDL3/SDL.h>

static SDL_Window* s_sdlWindow;
static SDL_Renderer* s_sdlRenderer;
static SDL_Surface* s_sdlSurface;
static SDL_Texture* s_sdlTexture;

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

void GRAP_SDL_Initialize(void)
{
    windowWidth = CONFIG_GetInt("window", "width", windowWidth);
    windowHeight = CONFIG_GetInt("window", "height", windowHeight);

    SDL_WindowFlags windowFlags = 0;

#if defined(ENABLE_BUG_FIX)
    bool fullscreen = CONFIG_GetInt("window", "fullscreen", 1) != 0;
    if (fullscreen)
    {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }
    else
    {
        windowFlags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;
    }
#endif

    SDL_CreateWindowAndRenderer("Ultima V: Warriors of Destiny", windowWidth, windowHeight, windowFlags, &s_sdlWindow, &s_sdlRenderer);

#if defined(ENABLE_BUG_FIX)
    if (fullscreen)
    {
        // BUGFIX: dstRect scaling uses windowWidth/windowHeight, not the live window size,
        // so they must be updated to the actual fullscreen resolution after creation.
        SDL_GetWindowSizeInPixels(s_sdlWindow, &windowWidth, &windowHeight);
    }
#endif

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
}

void GRAP_SDL_Cleanup(void)
{
    GRAP_BUF_Cleanup();

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

    for (int y = 0; y < loresHeight; y++)
    {
        for (int x = 0; x < loresWidth; x++)
        {
            Uint32 color = s_egaPalette[g_linearEgaBuffer0[y * loresWidth + x] & 0xf];
            pixels[(y * 2) * pitch + (x * 2)] = color;
            pixels[(y * 2) * pitch + (x * 2 + 1)] = color;
            pixels[(y * 2 + 1) * pitch + (x * 2)] = color;
            pixels[(y * 2 + 1) * pitch + (x * 2 + 1)] = color;
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
