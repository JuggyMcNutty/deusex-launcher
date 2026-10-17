/* The engine's own sections of a profile's DeusEx.ini, seeded whole.
 *
 * The engine reads its [Engine.SurrealClient], [Engine.SurrealAudioDevice]
 * and [Engine.SurrealRenderDevice] once [Engine.SurrealAudioDevice] has a
 * key, and the retail sections they stand for before -- WinDrv.WindowsClient,
 * Galaxy.GalaxyAudioSubsystem and the one [Engine.Engine] GameRenderDevice
 * names -- a key missing taking its built-in default either way; its first
 * clean exit writes all 40 keys and [Engine.SurrealWindowSystem]
 * WindowSystem (VibeEngine's Engine.cpp, the U*Device classes). Seeded by
 * the launcher, every key is there before the first run, so the settings
 * read and write the generation the engine will, and it never fills in
 * its own defaults (a 1920x1080 window) over the game's:
 *
 *   before the engine's sections are read: each from its retail key, in
 *   the form the engine writes it, else the engine's default
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

#endif
