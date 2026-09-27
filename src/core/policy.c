#include "core/policy.h"
#include "core/cmdline.h"
#include "core/config.h"

#include <string.h>

/* The four tokens that skip the single-instance handoff. All matched with
 * appStrfind semantics: anywhere in the line, no leading '-' needed.
 * dx-reverse-info/cli-flags.md "Substring-matched tokens". */
static const char *const bypass_tokens[] = {
    "Server", "NewWindow", "changevideo", "TestRenDev"
};

void dxl_policy_decide(const dxl_policy_input *in, dxl_decision *out) {
    const char *cmd = in->cmdline ? in->cmdline : "";

    memset(out, 0, sizeof *out);
    out->action      = DXL_ACTION_LAUNCH;
    out->screen      = DXL_SCREEN_NONE;
    out->caption_key = NULL;

    /* -firstrun forces the gate to 0, which is what makes the first-time flow
     * reproducible on demand (0x1090A26D). */
    out->effective_first_run = dxl_cmd_param(cmd, "firstrun") ? 0 : in->first_run;
    out->migrate_saves = out->effective_first_run < DXL_FIRSTRUN_MIGRATE_BELOW;

    for (size_t i = 0; i < sizeof bypass_tokens / sizeof *bypass_tokens; i++)
        if (dxl_cmd_find(cmd, bypass_tokens[i])) { out->skip_handoff = 1; break; }

    /* dx-reverse-info/launch-flow.md section 3. Note the asymmetry: -log and -server
     * are ParseParam, TestRenDev is a raw substring. Both halves of this
     * condition were confirmed live. */
    out->show_splash = !dxl_cmd_param(cmd, "log") &&
                       !dxl_cmd_param(cmd, "server") &&
                       !dxl_cmd_find(cmd, "TestRenDev");

    dxl_cmd_value(cmd, "exec", out->exec_file, sizeof out->exec_file);

    /* Forwarding runs before any engine init and short-circuits everything
     * else: this process becomes a messenger and exits (section 1). */
    if (!out->skip_handoff && in->other_instance) {
        out->action = DXL_ACTION_FORWARD;
        return;
    }

    /* Both of these exit without ever launching (section 5, steps 6 and 7). */
    if (dxl_cmd_value(cmd, "consolecommand", out->console_command,
                      sizeof out->console_command)) {
        out->action = DXL_ACTION_CONSOLE_CMD;
        return;
    }
    if (dxl_cmd_value(cmd, "testrendev", out->test_rendev, sizeof out->test_rendev)) {
        out->action = DXL_ACTION_TEST_RENDEV;
        return;
    }

    /* The wizard is only ever considered for a client. Under -server there is
     * nobody to show it to. */
    if (!in->is_client) return;

    /* --- the entry tree, first match wins (dx-reverse-info/wizard.md) --- */

    /* 1. -safe, or "readini" anywhere in the line. */
    if (dxl_cmd_param(cmd, "safe") || dxl_cmd_find(cmd, "readini")) {
        out->action = DXL_ACTION_SCREEN;
        out->screen = DXL_SCREEN_MAIN_SAFE;
        out->caption_key = "SafeMode";
        return;
    }

    /* 2. First run. Beats -changevideo, which is why a pristine install with
     *    -changevideo still gets the full first-time flow. */
    if (out->effective_first_run < DXL_FIRSTRUN_WIZARD_BELOW) {
        out->action = DXL_ACTION_SCREEN;
        out->screen = DXL_SCREEN_RENDERER_FIRST;
        out->caption_key = "FirstTime";
        return;
    }

    /* 3. Explicit video reconfiguration. */
    if (dxl_cmd_param(cmd, "changevideo")) {
        out->action = DXL_ACTION_SCREEN;
        out->screen = DXL_SCREEN_RENDERER_VIDEO;
        out->caption_key = "Video";
        return;
    }

    /* 4. The crash sentinel. Only meaningful when nothing else is running --
     *    otherwise Running.ini belongs to that live instance, not to a crash.
     *    This is the whole of crash detection. */
    if (!in->other_instance && in->running_ini_exists) {
        out->action = DXL_ACTION_SCREEN;
        out->screen = DXL_SCREEN_MAIN_RECOVERY;
        out->caption_key = "RecoveryMode";
        return;
    }
}

const char *dxl_action_name(dxl_action a) {
    switch (a) {
    case DXL_ACTION_LAUNCH:      return "launch";
    case DXL_ACTION_SCREEN:      return "screen";
    case DXL_ACTION_FORWARD:     return "forward";
    case DXL_ACTION_CONSOLE_CMD: return "console-command";
    case DXL_ACTION_TEST_RENDEV: return "test-rendev";
    }
    return "?";
}

const char *dxl_screen_name(dxl_screen s) {
    switch (s) {
    case DXL_SCREEN_NONE:            return "none";
    case DXL_SCREEN_MAIN_SAFE:       return "main/safe";
    case DXL_SCREEN_MAIN_RECOVERY:   return "main/recovery";
    case DXL_SCREEN_RENDERER_FIRST:  return "renderer/firsttime";
    case DXL_SCREEN_RENDERER_VIDEO:  return "renderer/video";
    }
    return "?";
}
