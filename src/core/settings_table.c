/* The settings' rows (core/settings.h): what each is, where it is kept,
 * and how its place's reader takes it. A row's default is what is taken
 * with no value kept: for the engine's sections its built-in one, for
 * User.ini's keys the class's (DefUser.ini's are what a profile starts
 * with: dxl_profile_defaults), for Settings.json the launcher's defaults
 * file's. */
#include "core/cpumode.h"
#include "core/settings.h"

const char *const dxl_settings_pages[] = {
    "Display", "Graphics", "Audio", "Keyboard & mouse", "Controller", "Gameplay & HUD", "Multiplayer", "Launcher",
};
const size_t dxl_settings_pages_count = sizeof dxl_settings_pages / sizeof *dxl_settings_pages;

/* Where each is kept. The system ini's engine generation first, then the
 * retail one it stands for (core/settings.h). */
#define ENGINE_SYS(sec, k, c) { DXL_STORE_SYSTEM, sec, k, c, 0, 0, DXL_GEN_ENGINE }
#define RETAIL_SYS(sec, k, c) { DXL_STORE_SYSTEM, sec, k, c, 0, 0, DXL_GEN_RETAIL }
#define CLIENT(k, c) ENGINE_SYS("Engine.DXEngineClient", k, c), RETAIL_SYS("WinDrv.WindowsClient", k, c)
#define AUDIO(k, c) ENGINE_SYS("Engine.DXEngineAudioDevice", k, c), RETAIL_SYS("Galaxy.GalaxyAudioSubsystem", k, c)
#define RENDER(k, c) ENGINE_SYS("Engine.DXEngineRenderDevice", k, c), RETAIL_SYS("D3DDrv.D3DRenderDevice", k, c)
#define VOLUME(k) { DXL_STORE_SYSTEM, "Engine.DXEngineAudioDevice", k, DXL_CODEC_BYTE, 255, 0, DXL_GEN_ENGINE }, \
                  { DXL_STORE_SYSTEM, "Galaxy.GalaxyAudioSubsystem", k, DXL_CODEC_BYTE, 255, 0, DXL_GEN_RETAIL }
#define JSON(f) { DXL_STORE_ENGINE, NULL, NULL, DXL_CODEC_JSON, 0, f, 0 }
#define JSON_SCALED(f, k) { DXL_STORE_ENGINE, NULL, NULL, DXL_CODEC_JSON, k, f, 0 }
#define PAWN(k, c) { DXL_STORE_USER, "Engine.PlayerPawn", k, c, 0, 0, 0 }
#define PLAYER(k, c) { DXL_STORE_USER, "DeusEx.DeusExPlayer", k, c, 0, 0, 0 }
#define NEXT(w) { DXL_STORE_NEXT_RUN, NULL, w, DXL_CODEC_BOOL, 0, 0, 0 }
#define LAUNCHER(k, c) { DXL_STORE_LAUNCHER, "Launcher", k, c, 0, 0, 0 }
#define PROFILE(k, c) { DXL_STORE_PROFILE, "Profile", k, c, 0, 0, 0 }

static const dxl_set_choice gammas[] = {
    { "D3D9", "Direct3D 9", "Brightness is a gamma power on red, green and blue apart, as the game's Direct3D "
                           "renderer had it." },
    { "XOpenGL", "XOpenGL", "Brightness scales each colour by one factor, so hues are kept: the XOpenGL "
                            "community renderer's curve." },
    { NULL, NULL, NULL },
};

static const dxl_set_choice antialiasing[] = {
    { "Off", "Off", NULL },
    { "MSAA2x", "2x multisampling", NULL },
    { "MSAA4x", "4x multisampling", NULL },
    { NULL, NULL, NULL },
};

static const dxl_set_choice lighting[] = {
    { "Normal", "Normal", "Light maps blended at twice their value, as the game's Direct3D renderer blended them." },
    { "OneX", "1x", "Light maps blended at their value: darker all over, close to the software renderer's look." },
    { "BrighterActors", "Brighter actors", "As Normal, with people, weapons and things one and a half times as "
                                           "bright, so they stand out in dark rooms." },
    { NULL, NULL, NULL },
};

static const dxl_set_choice channels[] = {
    { "8", "8", NULL }, { "16", "16", NULL }, { "24", "24", NULL }, { "32", "32", NULL }, { NULL, NULL, NULL },
};

static const dxl_set_choice rates[] = {
    { "11025", "11025 Hz", NULL }, { "22050", "22050 Hz", NULL }, { "44100", "44100 Hz", NULL },
    { NULL, NULL, NULL },
};

/* The preset row's choices: dxl_presets' ids, and Custom (the page's). */
static const dxl_set_choice presets[] = {
    { "as-in-2000", "As in 2000", NULL }, { "enhanced", "Enhanced", NULL }, { "performance", "Performance", NULL },
    { "custom", "Custom", "The settings below are no preset's: changed one by one." },
    { NULL, NULL, NULL },
};

/* What is under the menus and the screens of the game. */
static const dxl_set_choice backgrounds[] = {
    { "0", "The game", "The game goes on drawing behind them." },
    { "1", "A still of it", "The game as it was when the menu came up." },
    { "2", "Black", NULL },
    { NULL, NULL, NULL },
};

/* The game's own themes, by the names it keeps: a name it has no theme of
 * selects nothing. The menus' and the HUD's lists differ. */
#define THEME(n) { n, n, NULL }
static const dxl_set_choice menu_themes[] = {
    THEME("Default"), THEME("Blue & Gold"), THEME("Cool Green"), THEME("Cops"), THEME("Cyan"),
    THEME("Desert Storm"), THEME("Dried Blood"), THEME("Dusk"), THEME("Earth"), THEME("Green"), THEME("Grey"),
    THEME("Ion Storm"), THEME("Lava"), THEME("Night Vision"), THEME("Ninja"), THEME("Olive"), THEME("Pale Green"),
    THEME("Pastel"), THEME("Plasma"), THEME("Primaries"), THEME("Purple"), THEME("Red"), THEME("Seawater"),
    THEME("SoylentGreen"), THEME("Starlight"), THEME("Steel"), THEME("Steel Green"), THEME("Superhero"),
    THEME("Terminator"), THEME("Violet"), { NULL, NULL, NULL },
};
static const dxl_set_choice hud_themes[] = {
    THEME("Default"), THEME("Amber"), THEME("Cops"), THEME("Cyan"), THEME("Dark Blue"), THEME("Desert Storm"),
    THEME("Dried Blood"), THEME("Dusk"), THEME("Grey"), THEME("Ion Storm"), THEME("Night Vision"), THEME("Ninja"),
    THEME("Pale Green"), THEME("Pastel"), THEME("Plasma"), THEME("Primaries"), THEME("Purple"), THEME("Red"),
    THEME("SoylentGreen"), THEME("Starlight"), THEME("Steel Green"), THEME("Superhero"), THEME("Terminator"),
    THEME("Violet"), { NULL, NULL, NULL },
};

static const dxl_set_choice render_drivers[] = {
    { "", "Chosen by SDL", "The renderer SDL starts first here: OpenGL, Vulkan, or the software one." },
    { "opengl", "OpenGL", NULL },
    { "vulkan", "Vulkan", NULL },
    { "opengles2", "OpenGL ES", NULL },
    { "software", "Software", "Drawn on the CPU: slow, but it needs no graphics driver." },
    { NULL, NULL, NULL },
};

const dxl_setting dxl_settings_table[] = {
    /* ---- Display ---------------------------------------------------------- */
    { .id = "display.brightness", .page = "Display", .label = "Brightness",
      .help = "The setting the game's own Adjust Brightness changes: 0.40 shows the game as it was drawn, the "
              "game's default is 0.60.",
      .kind = DXL_SET_NUMBER, .min = 0.1, .max = 1, .step = 0.02, .format = "%.2f", .shown_scale = 1,
      .def = "0.5", .at = { CLIENT("Brightness", DXL_CODEC_FLOAT) } },
    { .id = "display.gamma", .page = "Display", .label = "Brightness curve",
      .help = "What Brightness does to the colours.", .kind = DXL_SET_CHOICE, .choices = gammas, .def = "D3D9",
      .at = { JSON(DXL_ES_GAMMA) } },
    { .id = "display.vsync", .page = "Display", .label = "Vertical sync",
      .help = "Each frame shown as the display refreshes: no tearing, but below the display's rate the game "
              "then holds frames to a half or a third of it.",
      .kind = DXL_SET_BOOL, .def = "1", .at = { JSON(DXL_ES_VSYNC) } },
    { .id = "display.render_scale", .page = "Display", .label = "Render scale",
      .help = "The scene drawn at this share of the window's size and scaled up to it: fewer pixels, faster and "
              "softer. Vulkan and OpenGL alike.",
      .kind = DXL_SET_NUMBER, .min = 0.25, .max = 1, .step = 0.05, .format = "%.0f%%", .shown_scale = 100,
      .def = "1", .at = { JSON(DXL_ES_RENDER_SCALE) } },
    { .id = "display.fov", .page = "Display", .label = "Field of view",
      .help = "Degrees across a 4:3 picture; wider screens see more at the sides. The game's own is 75.",
      .kind = DXL_SET_NUMBER, .min = 60, .max = 120, .step = 1, .format = "%.0f\xc2\xb0", .shown_scale = 1,
      .def = "90", .at = { PAWN("DefaultFOV", DXL_CODEC_FLOAT), PAWN("DesiredFOV", DXL_CODEC_FLOAT) } },

    /* ---- Graphics ---------------------------------------------------------- */
    { .id = "graphics.preset", .page = "Graphics", .label = "Preset",
      .help = "The settings below, chosen together.", .kind = DXL_SET_CHOICE, .choices = presets, .def = "custom",
      .flags = DXL_SET_PRESET },
    { .id = "graphics.renderer", .page = "Graphics", .label = "Renderer",
      .help = "What draws the game: the renderers this engine has, those this machine runs offered.",
      .kind = DXL_SET_CHOICE, .def = "Vulkan", .at = { JSON(DXL_ES_RENDER_TYPE) }, .flags = DXL_SET_RENDERER },
    { .id = "graphics.cpu_mode", .page = "Graphics", .label = "CPU mode",
      .help = "How fast the handheld's CPU runs while the game does; the menu's own speed comes back as it ends.",
      .kind = DXL_SET_CHOICE, .choices = dxl_cpu_modes, .def = DXL_CPU_MODE_DEFAULT,
      .at = { LAUNCHER("CpuMode", DXL_CODEC_TEXT) } },
    { .id = "graphics.antialias", .page = "Graphics", .label = "Anti-aliasing",
      .help = "Multisampling smooths the edges of polygons; 2x and 4x cost the GPU time and memory.",
      .kind = DXL_SET_CHOICE, .choices = antialiasing, .def = "MSAA4x", .at = { JSON(DXL_ES_ANTIALIAS) } },
    { .id = "graphics.light", .page = "Graphics", .label = "Lighting", .help = "How the light maps are blended.",
      .kind = DXL_SET_CHOICE, .choices = lighting, .def = "Normal", .at = { JSON(DXL_ES_LIGHT) } },
    { .id = "graphics.bloom", .page = "Graphics", .label = "Bloom",
      .help = "A soft glow round bright lights, DXEngine's own.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { JSON(DXL_ES_BLOOM) } },
    { .id = "graphics.bloom_amount", .page = "Graphics", .label = "Bloom strength",
      .help = "How far the glow spreads, and how bright it is.", .kind = DXL_SET_NUMBER, .min = 0.0625, .max = 1,
      .step = 0.0625, .format = "%.0f%%", .shown_scale = 100, .def = "0.501961",
      .at = { JSON_SCALED(DXL_ES_BLOOM_AMOUNT, 255) } },
    { .id = "graphics.detail_textures", .page = "Graphics", .label = "Detail textures",
      .help = "The fine grain the game lays over near walls and floors, as its Direct3D renderer drew it.",
      .kind = DXL_SET_BOOL,
      .def = "1", .at = { RENDER("DetailTextures", DXL_CODEC_BOOL) } },
    { .id = "graphics.volumetric", .page = "Graphics", .label = "Volumetric lighting",
      .help = "The fog that fills fog zones.", .kind = DXL_SET_BOOL, .def = "1",
      .at = { RENDER("VolumetricLighting", DXL_CODEC_BOOL) } },
    { .id = "graphics.decals", .page = "Graphics", .label = "Decals",
      .help = "Scorch marks, bullet holes and blood on walls and floors.", .kind = DXL_SET_BOOL, .def = "1",
      .at = { CLIENT("Decals", DXL_CODEC_BOOL) } },
    { .id = "graphics.dynamic_lights", .page = "Graphics", .label = "Moving lights",
      .help = "Lights that flicker, pulse and turn. Off, they are still.", .kind = DXL_SET_BOOL, .def = "1",
      .at = { CLIENT("NoDynamicLights", DXL_CODEC_NOT) } },
    { .id = "graphics.flashes", .page = "Graphics", .label = "Screen flashes",
      .help = "The screen's flash when you are hit or blinded; always on in a net game.", .kind = DXL_SET_BOOL,
      .def = "1", .at = { CLIENT("ScreenFlashes", DXL_CODEC_BOOL) } },
    { .id = "graphics.hdr", .page = "Graphics", .label = "HDR",
      .help = "A scene of 16-bit colours, shown through DXEngine's HDR shader; on Vulkan on a display "
              "that takes it.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { JSON(DXL_ES_HDR) }, .flags = DXL_SET_ADVANCED },
    { .id = "graphics.hdr_scale", .page = "Graphics", .label = "HDR brightness", .help = "HDR's brightness.",
      .kind = DXL_SET_NUMBER, .min = 0.0625, .max = 1, .step = 0.0625, .format = "%.0f%%", .shown_scale = 100,
      .def = "0.501961", .at = { JSON_SCALED(DXL_ES_HDR_SCALE, 255) }, .flags = DXL_SET_ADVANCED },
    { .id = "graphics.gamma_shots", .page = "Graphics", .label = "Screenshots with the brightness",
      .help = "A screenshot as the screen shows it, brightness and all, not the scene as drawn.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { JSON(DXL_ES_GAMMA_SCREENSHOTS) }, .flags = DXL_SET_ADVANCED },

    /* ---- Audio -------------------------------------------------------------- */
    { .id = "audio.music_volume", .page = "Audio", .label = "Music", .help = "The music's volume.",
      .kind = DXL_SET_NUMBER, .min = 0, .max = 1, .step = 0.02, .format = "%.0f%%", .shown_scale = 100,
      .def = "0.627451", .at = { VOLUME("MusicVolume") } },
    { .id = "audio.sound_volume", .page = "Audio", .label = "Sounds", .help = "The sound effects' volume.",
      .kind = DXL_SET_NUMBER, .min = 0, .max = 1, .step = 0.02, .format = "%.0f%%", .shown_scale = 100,
      .def = "0.784314", .at = { VOLUME("SoundVolume") } },
    { .id = "audio.speech_volume", .page = "Audio", .label = "Speech", .help = "The voices' volume.",
      .kind = DXL_SET_NUMBER, .min = 0, .max = 1, .step = 0.02, .format = "%.0f%%", .shown_scale = 100,
      .def = "1", .at = { VOLUME("SpeechVolume") } },
    { .id = "audio.ambient", .page = "Audio", .label = "Ambient sounds",
      .help = "The sounds of places, the hum and the rain, against the rest.", .kind = DXL_SET_NUMBER,
      .min = 0, .max = 1, .step = 0.05, .format = "%.0f%%", .shown_scale = 100, .def = "0.7",
      .at = { AUDIO("AmbientFactor", DXL_CODEC_FLOAT) } },
    { .id = "audio.music", .page = "Audio", .label = "Music on",
      .help = "Off, no music; from the next song.", .kind = DXL_SET_BOOL, .def = "1",
      .at = { AUDIO("UseDigitalMusic", DXL_CODEC_BOOL) } },
    { .id = "audio.reverb", .page = "Audio", .label = "Reverb", .help = "Each zone's echo.", .kind = DXL_SET_BOOL,
      .def = "1", .at = { AUDIO("UseReverb", DXL_CODEC_BOOL) } },
    { .id = "audio.surround", .page = "Audio", .label = "Surround",
      .help = "A sound behind you, panned as the game's own mixer did for a surround decoder.",
      .kind = DXL_SET_BOOL, .def = "1", .at = { AUDIO("UseSurround", DXL_CODEC_BOOL) } },
    { .id = "audio.reverse", .page = "Audio", .label = "Swap left and right", .help = "", .kind = DXL_SET_BOOL,
      .def = "0", .at = { AUDIO("ReverseStereo", DXL_CODEC_BOOL) } },
    { .id = "audio.channels", .page = "Audio", .label = "Sound channels",
      .help = "How many sounds play at once; past them, the furthest stop.", .kind = DXL_SET_CHOICE,
      .choices = channels, .def = "16", .at = { AUDIO("EffectsChannels", DXL_CODEC_INT) } },
    /* The launcher's own, in its window alone. */
    { .id = "launcher.music_volume", .page = "Audio", .label = "The launcher's music",
      .help = "The game's title music in the launcher's window, this loud; none at 0. The game's own music "
              "is Music, above.",
      .kind = DXL_SET_NUMBER, .min = 0, .max = 1, .step = 0.05, .format = "%.0f%%", .shown_scale = 100,
      .def = "0.5", .at = { LAUNCHER("MusicVolume", DXL_CODEC_FLOAT) } },
    { .id = "launcher.sound_volume", .page = "Audio", .label = "The launcher's sounds",
      .help = "The game's menu sounds in the launcher's window, this loud; none at 0.",
      .kind = DXL_SET_NUMBER, .min = 0, .max = 1, .step = 0.05, .format = "%.0f%%", .shown_scale = 100,
      .def = "0.7", .at = { LAUNCHER("SoundVolume", DXL_CODEC_FLOAT) } },
    { .id = "audio.rate", .page = "Audio", .label = "Mixing rate",
      .help = "The game's mixer's rate, from its next start; the system's sound resamples it.",
      .kind = DXL_SET_CHOICE, .choices = rates, .def = "44100", .at = { AUDIO("OutputRate", DXL_CODEC_HZ) },
      .flags = DXL_SET_ADVANCED },

    /* ---- Keyboard & mouse (the keys' rows follow, the page's own) ------------ */
    { .id = "mouse.sensitivity", .page = "Keyboard & mouse", .label = "Mouse sensitivity",
      .help = "How far a move of the mouse turns you. The game's own is 3.", .kind = DXL_SET_NUMBER, .min = 0.5,
      .max = 12, .step = 0.5, .format = "%.1f", .shown_scale = 1, .def = "5",
      .at = { PAWN("MouseSensitivity", DXL_CODEC_FLOAT) } },
    { .id = "mouse.invert", .page = "Keyboard & mouse", .label = "Invert the mouse",
      .help = "The mouse forward looks down.", .kind = DXL_SET_BOOL, .def = "1",
      .at = { PAWN("bInvertMouse", DXL_CODEC_BOOL) } },
    { .id = "mouse.look", .page = "Keyboard & mouse", .label = "Mouse look",
      .help = "The mouse looks up and down as well as round.", .kind = DXL_SET_BOOL, .def = "1",
      .at = { PAWN("bAlwaysMouseLook", DXL_CODEC_BOOL) } },
    { .id = "mouse.smoothing", .page = "Keyboard & mouse", .label = "Mouse smoothing",
      .help = "The mouse's moves smoothed over frames.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { PAWN("bMaxMouseSmoothing", DXL_CODEC_BOOL) } },

    /* ---- Controller (its buttons' rows the page's own) ----------------------- */
    { .id = "pad.dead_zone", .page = "Controller", .label = "Dead zone",
      .help = "How far a stick moves before it counts, round its middle, so a diagonal stays one.",
      .kind = DXL_SET_NUMBER, .min = 0.05, .max = 0.5, .step = 0.05, .format = "%.0f%%", .shown_scale = 100,
      .def = "0.2", .at = { JSON(DXL_ES_PAD_DEADZONE) } },
    { .id = "pad.look_x", .page = "Controller", .label = "Looking across",
      .help = "How fast the stick that looks turns you.", .kind = DXL_SET_NUMBER, .min = 0.25, .max = 3,
      .step = 0.25, .format = "%.2fx", .shown_scale = 1, .def = "1", .at = { JSON(DXL_ES_PAD_LOOK_X) } },
    { .id = "pad.look_y", .page = "Controller", .label = "Looking up and down",
      .help = "How fast the stick that looks tips you up and down.", .kind = DXL_SET_NUMBER, .min = 0.25, .max = 3,
      .step = 0.25, .format = "%.2fx", .shown_scale = 1, .def = "1", .at = { JSON(DXL_ES_PAD_LOOK_Y) } },
    { .id = "pad.invert_y", .page = "Controller", .label = "Invert looking", .help = "Forward on the stick looks down.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { JSON(DXL_ES_PAD_INVERT_Y) } },
    { .id = "pad.cursor_speed", .page = "Controller", .label = "Pointer speed in menus",
      .help = "How fast a stick moves the pointer in the game's menus and screens.", .kind = DXL_SET_NUMBER,
      .min = 0.25, .max = 3, .step = 0.25, .format = "%.2fx", .shown_scale = 1, .def = "1",
      .at = { JSON(DXL_ES_PAD_CURSOR_SPEED) } },
    { .id = "pad.enabled", .page = "Controller", .label = "Controller in the game",
      .help = "Off, the engine reads no pad, and SDL gives its buttons as keys instead: the d-pad the arrows, "
              "the south button Enter. The sticks then do nothing.",
      .kind = DXL_SET_BOOL, .def = "1", .at = { JSON(DXL_ES_PAD_ENABLED) }, .flags = DXL_SET_ADVANCED },

    /* ---- Gameplay & HUD ------------------------------------------------------ */
    { .id = "game.subtitles", .page = "Gameplay & HUD", .label = "Subtitles",
      .help = "Conversations' words on screen, and what people say in passing; a line with no recording is "
              "shown either way.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { PLAYER("bSubtitles", DXL_CODEC_BOOL) } },
    { .id = "game.always_run", .page = "Gameplay & HUD", .label = "Always run",
      .help = "Running unless the walk key is held. Off, walking unless it is.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { PLAYER("bAlwaysRun", DXL_CODEC_BOOL) } },
    { .id = "game.toggle_crouch", .page = "Gameplay & HUD", .label = "Crouch key toggles",
      .help = "One press crouches, the next stands up. Off, you crouch while it is held.", .kind = DXL_SET_BOOL,
      .def = "0", .at = { PLAYER("bToggleCrouch", DXL_CODEC_BOOL) } },
    { .id = "game.auto_reload", .page = "Gameplay & HUD", .label = "Reload by itself",
      .help = "A weapon reloads as its clip runs out.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { PLAYER("bAutoReload", DXL_CODEC_BOOL) } },
    { .id = "game.object_names", .page = "Gameplay & HUD", .label = "Names of things",
      .help = "What is under the crosshair, named beside its brackets.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { PLAYER("bObjectNames", DXL_CODEC_BOOL) } },
    { .id = "game.npc_highlight", .page = "Gameplay & HUD", .label = "Brackets round people",
      .help = "A person under the crosshair bracketed as things are. Off, only things are.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { PLAYER("bNPCHighlighting", DXL_CODEC_BOOL) } },
    { .id = "hud.crosshair", .page = "Gameplay & HUD", .label = "Crosshair", .help = "", .kind = DXL_SET_BOOL,
      .def = "0", .at = { PLAYER("bCrosshairVisible", DXL_CODEC_BOOL) } },
    { .id = "hud.compass", .page = "Gameplay & HUD", .label = "Compass", .help = "", .kind = DXL_SET_BOOL,
      .def = "0", .at = { PLAYER("bCompassVisible", DXL_CODEC_BOOL) } },
    { .id = "hud.hits", .page = "Gameplay & HUD", .label = "Body damage",
      .help = "The figure that shows each part of the body's damage.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { PLAYER("bHitDisplayVisible", DXL_CODEC_BOOL) } },
    { .id = "hud.ammo", .page = "Gameplay & HUD", .label = "Ammunition",
      .help = "The weapon in hand's ammunition and clip.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { PLAYER("bAmmoDisplayVisible", DXL_CODEC_BOOL) } },
    { .id = "hud.augs", .page = "Gameplay & HUD", .label = "Augmentations",
      .help = "The augmentations and the charged items in use, down the right.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { PLAYER("bAugDisplayVisible", DXL_CODEC_BOOL) } },
    { .id = "hud.all_augs", .page = "Gameplay & HUD", .label = "Every augmentation",
      .help = "Every augmentation installed in that list. Off, the active ones alone.", .kind = DXL_SET_BOOL,
      .def = "0", .at = { PLAYER("bHUDShowAllAugs", DXL_CODEC_BOOL) } },
    { .id = "hud.belt", .page = "Gameplay & HUD", .label = "Object belt", .help = "The belt's ten slots.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { PLAYER("bObjectBeltVisible", DXL_CODEC_BOOL) } },
    { .id = "hud.log_timeout", .page = "Gameplay & HUD", .label = "Messages stay",
      .help = "How long a message stays on the screen. In a net game, five seconds at the least.",
      /* The class's is 0, below the game's menu's 1 to 10: DefUser.ini's. */
      .kind = DXL_SET_NUMBER, .min = 1, .max = 10, .step = 0.5, .format = "%.1f s", .shown_scale = 1, .def = "3",
      .at = { PLAYER("logTimeout", DXL_CODEC_FLOAT) } },
    { .id = "hud.completed_goals", .page = "Gameplay & HUD", .label = "Goals done, listed",
      .help = "The Goals screen lists the goals already met as well.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { PLAYER("bDisplayCompletedGoals", DXL_CODEC_BOOL) } },
    { .id = "hud.background", .page = "Gameplay & HUD", .label = "Behind the menus",
      .help = "What the menus and the game's screens are drawn over.", .kind = DXL_SET_CHOICE,
      .choices = backgrounds, .def = "0", .at = { PLAYER("UIBackground", DXL_CODEC_INT) } },
    { .id = "hud.menu_theme", .page = "Gameplay & HUD", .label = "The menus' colours",
      .help = "The game's colour theme for its menus.", .kind = DXL_SET_CHOICE, .choices = menu_themes,
      .def = "Grey", .at = { PLAYER("MenuThemeName", DXL_CODEC_TEXT) } },
    { .id = "hud.hud_theme", .page = "Gameplay & HUD", .label = "The HUD's colours",
      .help = "The game's colour theme for the HUD and its screens.", .kind = DXL_SET_CHOICE,
      .choices = hud_themes, .def = "Grey", .at = { PLAYER("HUDThemeName", DXL_CODEC_TEXT) } },
    { .id = "hud.menus_translucent", .page = "Gameplay & HUD", .label = "See-through menus", .help = "",
      .kind = DXL_SET_BOOL, .def = "1", .at = { PLAYER("bMenusTranslucent", DXL_CODEC_BOOL) } },
    { .id = "hud.background_translucent", .page = "Gameplay & HUD", .label = "See-through HUD",
      .help = "The backgrounds of the HUD and of the game's screens.", .kind = DXL_SET_BOOL, .def = "1",
      .at = { PLAYER("bHUDBackgroundTranslucent", DXL_CODEC_BOOL) } },
    { .id = "hud.borders", .page = "Gameplay & HUD", .label = "HUD borders",
      .help = "The frames round the HUD's parts.", .kind = DXL_SET_BOOL, .def = "1",
      .at = { PLAYER("bHUDBordersVisible", DXL_CODEC_BOOL) } },
    { .id = "hud.borders_translucent", .page = "Gameplay & HUD", .label = "See-through borders", .help = "",
      .kind = DXL_SET_BOOL, .def = "1", .at = { PLAYER("bHUDBordersTranslucent", DXL_CODEC_BOOL) } },

    /* ---- Multiplayer ---------------------------------------------------------- */
    { .id = "mp.name", .page = "Multiplayer", .label = "Your name",
      .help = "What the others see you as in a net game: letters, digits and brackets, as the game takes "
              "them.",
      .kind = DXL_SET_TEXT, .def = "Player", .chars = "[]()",
      .at = { { DXL_STORE_USER, "DefaultPlayer", "Name", DXL_CODEC_TEXT, 0, 0, 0 } } },
    { .id = "mp.master", .page = "Multiplayer", .label = "Master server",
      .help = "Where the game's server browser gets its list of servers from.", .kind = DXL_SET_TEXT,
      .def = "master.333networks.com",
      .at = { { DXL_STORE_SYSTEM, "DeusEx.MenuScreenJoinGame", "MasterServerAddress", DXL_CODEC_TEXT, 0, 0, 0 } },
      .flags = DXL_SET_ADVANCED },

    /* ---- Launcher ------------------------------------------------------------- */
    /* The engine and the game's folder, chosen by the page (screens/settings.c). */
    { .id = "launcher.engine", .page = "Launcher", .label = "The engine",
      .help = "The DXEngine the game runs on: the launcher's own, beside it, or another -- a build of your own, "
              "one made for this system.",
      .kind = DXL_SET_ACTION },
    { .id = "profile.game", .page = "Launcher", .label = "The game's folder",
      .help = "The copy of Deus Ex this profile plays: another the launcher finds, or one shown to it.",
      .kind = DXL_SET_ACTION },
    { .id = "launcher.reduced_motion", .page = "Launcher", .label = "Reduced motion",
      .help = "Pages come up at once, and the focus brackets jump instead of gliding.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { LAUNCHER("ReducedMotion", DXL_CODEC_BOOL) } },
    { .id = "launcher.return", .page = "Launcher", .label = "Back here after playing",
      .help = "After a game that ended cleanly, the launcher's Home again, not the desktop.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { LAUNCHER("ReturnAfterPlaying", DXL_CODEC_BOOL) } },
    { .id = "next.nosound", .page = "Launcher", .label = "Next run: no sound",
      .help = "The next run alone with no sound device (-nosound); the sound settings stay as they are.",
      .kind = DXL_SET_BOOL, .def = "0", .at = { NEXT("-nosound") }, .flags = DXL_SET_ADVANCED },
    { .id = "next.software", .page = "Launcher", .label = "Next run: on the software renderer",
      .help = "The next run alone on the software renderer, full screen (-nohard): the original's safe mode "
              "runs it in a window, which this device does not show.", .kind = DXL_SET_BOOL,
      .def = "0", .at = { NEXT("-nohard") }, .flags = DXL_SET_ADVANCED },
    { .id = "next.defaultres", .page = "Launcher", .label = "Next run: at 640 x 480",
      .help = "The next run alone at the game's first size (-defaultres), which full screen on this device "
              "does not take.", .kind = DXL_SET_BOOL, .def = "0",
      .at = { NEXT("-defaultres") }, .flags = DXL_SET_ADVANCED },
    { .id = "next.nojoy", .page = "Launcher", .label = "Next run: no pad",
      .help = "The next run alone with the pad ignored (-nojoy).", .kind = DXL_SET_BOOL, .def = "0",
      .at = { NEXT("-nojoy") }, .flags = DXL_SET_ADVANCED },
    { .id = "launcher.render_driver", .page = "Launcher", .label = "The launcher's renderer",
      .help = "What draws the launcher's own window, from the next time it opens. The game's renderer is "
              "on Graphics.",
      .kind = DXL_SET_CHOICE, .choices = render_drivers, .def = "",
      .at = { LAUNCHER("RenderDriver", DXL_CODEC_TEXT) }, .flags = DXL_SET_ADVANCED },
    { .id = "profile.engine_args", .page = "Launcher", .label = "The game's extra words",
      .help = "Words for the engine's command line besides the launcher's, as a shell would split them: "
              "for testing.",
      .kind = DXL_SET_TEXT, .def = "", .at = { PROFILE("EngineArgs", DXL_CODEC_TEXT) },
      .flags = DXL_SET_ADVANCED },
};
const size_t dxl_settings_table_count = sizeof dxl_settings_table / sizeof *dxl_settings_table;

/* The graphics presets. As in 2000: the game as its Direct3D renderer drew
 * it on the hardware of its day -- no multisampling, no bloom, the detail
 * textures, which the engine draws as D3DDrv's passes do, Direct3D's light
 * and brightness curve. Enhanced: DXEngine's own on top.
 * Performance: less to draw. Each value one its place keeps as it is (a
 * bloom of 128/255, Settings.json's step), or the preset never matches. */
static const dxl_preset_value as_in_2000[] = {
    { "graphics.antialias", "Off" }, { "graphics.light", "Normal" }, { "display.gamma", "D3D9" },
    { "graphics.bloom", "0" }, { "graphics.detail_textures", "1" }, { "graphics.volumetric", "1" },
    { "graphics.dynamic_lights", "1" }, { "graphics.hdr", "0" }, { "display.render_scale", "1" },
    { NULL, NULL },
};
static const dxl_preset_value enhanced[] = {
    { "graphics.antialias", "MSAA4x" }, { "graphics.light", "Normal" }, { "graphics.bloom", "1" },
    { "graphics.bloom_amount", "0.501961" }, { "graphics.detail_textures", "1" }, { "graphics.volumetric", "1" },
    { "graphics.dynamic_lights", "1" }, { "display.render_scale", "1" },
    { NULL, NULL },
};
static const dxl_preset_value performance[] = {
    { "graphics.antialias", "Off" }, { "graphics.bloom", "0" }, { "graphics.detail_textures", "0" },
    { "graphics.volumetric", "0" }, { "display.render_scale", "0.75" }, { "display.vsync", "0" },
    { NULL, NULL },
};

const dxl_preset dxl_presets[] = {
    { "as-in-2000", "As in 2000", "The game as its Direct3D renderer drew it.", as_in_2000 },
    { "enhanced", "Enhanced", "Multisampling, bloom and detail textures.", enhanced },
    { "performance", "Performance", "Less to draw, for a slower GPU.", performance },
};
const size_t dxl_presets_count = sizeof dxl_presets / sizeof *dxl_presets;
