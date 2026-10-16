/* The savegame migration: dx-reverse-info/launch-flow.md section 6.
 *
 * Below FirstRun 220 the original looks for Unreal-style saves, ..\Save\*.usa
 * beside System\, and gives each a slot name in User.ini --
 * [UnrealShare.UnrealSlotMenu] SlotNames[<n>]=Saved game -- where it has
 * none, <n> being the number after the name's first four characters
 * (Save12.usa is 12). Deus Ex saves are folders with no .usa in them, so on
 * a Deus Ex install it finds nothing; it is here because the original does it.
 */
#ifndef DXL_MIGRATE_H
#define DXL_MIGRATE_H

#include "core/ini.h"

/* Looks in <system_dir>/../Save. Returns how many slot names it set. */
int dxl_migrate_saves(const char *system_dir, dxl_ini *user_ini);

#endif
