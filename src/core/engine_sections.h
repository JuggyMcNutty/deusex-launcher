/* The engine's own sections of a profile's DeusEx.ini, seeded whole.
 *
 * The engine reads its [Engine.DXEngineClient], [Engine.DXEngineAudioDevice]
 * and [Engine.DXEngineRenderDevice] once [Engine.DXEngineAudioDevice] has a
 * key, and the retail sections they stand for before -- WinDrv.WindowsClient,
 * Galaxy.GalaxyAudioSubsystem and the one [Engine.Engine] GameRenderDevice
 * names -- a key missing taking its built-in default either way; its first
 * clean exit writes all its keys, the original's config it keeps and has
 * no use for among them: 66 with D3DDrv (DXEngine's Engine.cpp, the
 * U*Device classes). Seeded by the launcher, every key is there before the
 * first run, so the settings read and write the generation the engine will,
 * and it never fills in its own defaults (a 1920x1080 window) over the
 * game's:
 *
 *   before the engine's sections are read: each from its retail key, read
 *   as the engine reads it -- a bool's 1, True, 0 or False alone, an empty
 *   float 0 -- and in the form the engine writes it, else the engine's
 *   default
 *   after: only the keys missing, with the engine's defaults, which it
 *   uses for them already
 *
 * Nothing the engine would do changes.
 */
#ifndef DXL_ENGINE_SECTIONS_H
#define DXL_ENGINE_SECTIONS_H

#include "core/ini.h"

/* Seeds system; returns how many keys it added. */
int dxl_engine_sections_seed(dxl_ini *system);

/* An OutputRate as the engine reads one (bugs.md D.55): the text's first
 * token one of Galaxy's seven names, 8000Hz to 48000Hz, in any case; its
 * rate, 0 for any other text, a number without its Hz among them. */
int dxl_engine_rate(const char *v);

#endif
