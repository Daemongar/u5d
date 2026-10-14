#ifndef _WIDE_VIEW_H
#define _WIDE_VIEW_H

#if defined(TARGET_SDL)
// Work out the tiles for the wide view's extra columns (after the map is drawn).
void WIDE_UpdateExtraTiles(void);
void WIDE_PutTerrainUnderActor(int x, int y, int actorTile);
void WIDE_PutGroundUnderTerrain(int x, int y, int mapTile);
#endif

#endif
