#ifndef _COMMON_PLATFORM_H
#define _COMMON_PLATFORM_H

//
// Compiler
//
#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#elif defined(__GNUC__)
#define COMPILER_GCC 1
#else
#define COMPILER_UNKNOWN 1
#endif

//
// OS / Environment
//
#if defined(_WIN32) || defined(_WIN64)
// Windows
#define OS_WINDOWS 1
#elif defined(__DJGPP__)
// Protected mode DOS
#define OS_DOS32 1
#elif defined(MSDOS) && defined(M_I86) // MSC 5.x
// Real mode DOS
#define OS_DOS16 1
#elif defined(__linux__)
// Linux
#define OS_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
// Mac
#define OS_MACOS 1
#else
#define OS_UNKNOWN 1
#endif

//
// Target profiles
//
#if defined(OS_WINDOWS) || defined(OS_MACOS) || defined(OS_LINUX)
#define TARGET_SDL 1
#elif defined(OS_DOS32)
#define TARGET_DOS32 1
#elif defined(OS_DOS16)
#define TARGET_DOS16 1
#endif

// TODO: move out of this file
//
// Build Configurations
//
// Matching build configuration (IBM 1.16 720 KB release)
//#define MATCHING_BUILD 1

// Approximates the behavior of the binary-patched GOG release
//#define GOG_BUILD 1

// Enable BGM
//#define ENABLE_BGM 1

// Enable speed fixes for modern build
//#define ENABLE_SPEED_FIX 1

// Enable bug fixes
//#define ENABLE_BUG_FIX 1

// Enable mouse-aimed combat targeting
//#define ENABLE_MOUSE_AIM 1

// Enable dialog improvements (case-insensitive keyword matching, auto-paging,
// larger input buffer, repeat detection, keyword hints)
//#define ENABLE_DIALOG_IMPROVEMENTS 1

// Enable HP-aware monster target selection (still nearest-enemy first, but
// prefers a wounded target over a healthier one at similar range)
//#define ENABLE_SMARTER_AI 1

// Award kill experience to the whole living party instead of only whoever
// landed the killing blow
//#define ENABLE_PARTY_EXP 1

// Award bonus kill experience for fighting multiple enemies at once (see
// COMBAT_AwardExp in combat.c)
//#define ENABLE_GROUP_EXP_BONUS 1

// Wider ambient light radius outdoors at night without a torch (~3 tiles
// instead of ~1). Dungeons and other always-dark maps are unaffected -
// torches remain necessary underground.
//#define ENABLE_BRIGHTER_NIGHT 1

// Let Esc also trigger the "Exit to DOS?" prompt (previously only Ctrl+E)
// at the idle overworld/town/dungeon command prompt, since Ctrl+E is an
// undiscoverable legacy DOS convention.
//#define ENABLE_QUICK_EXIT 1

// Charge extra travel time for climbing straight from low ground into much
// higher terrain (see MAINOUT_03e0 in mainout.c), on top of the destination
// tile's own difficulty cost. Descending is free.
//#define ENABLE_TERRAIN_ELEVATION 1

// Replace the raw 16-color EGA palette with a softer, less oversaturated
// set of the same 16 hues (see s_egaPalette in graphics/grap_sdl.c). Purely
// cosmetic - tile art is unchanged, just recolored at display time.
//#define ENABLE_ENHANCED_PALETTE 1

// Weight the random STR/DEX/INTEL level-up bonus (OUTSUBS_0658) by class
// instead of a flat uniform 1-in-3 roll - see s_classStatWeights.
//#define ENABLE_CLASS_STAT_GROWTH 1

// Round a Bard's mana up instead of down when derived from an odd INTEL
// (intel/2 -> (intel+1)/2), everywhere mag is set for CLASS_BARD.
//#define ENABLE_BARD_MANA_ROUNDUP 1

#if defined(TARGET_DOS16)
#define MATCHING_BUILD 1
#else
#define GOG_BUILD 1
#endif

#if !defined(TARGET_DOS16)
#define ENABLE_BGM 1
#define ENABLE_SPEED_FIX 1
#define ENABLE_BUG_FIX 1
#define ENABLE_MOUSE_AIM 1
//#define ENABLE_VARIED_COMBAT 1
#define ENABLE_DIALOG_IMPROVEMENTS 1
#define ENABLE_SMARTER_AI 1
#define ENABLE_PARTY_EXP 1
#define ENABLE_GROUP_EXP_BONUS 1
#define ENABLE_BRIGHTER_NIGHT 1
#define ENABLE_QUICK_EXIT 1
//#define ENABLE_TERRAIN_ELEVATION 1
#define ENABLE_ENHANCED_PALETTE 1
#define ENABLE_CLASS_STAT_GROWTH 1
#define ENABLE_BARD_MANA_ROUNDUP 1
//#define ENABLE_TRANSLATION 1
#endif

#if defined(TARGET_DOS16)
#define CDECL cdecl
#define FAR far
#else
#define CDECL
#define FAR
#endif

#if defined(COMPILER_MSVC)
#define _CRT_SECURE_NO_WARNINGS
#endif

#endif
