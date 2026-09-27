/* Command-line parsing, reproducing UE1's three helpers exactly.
 *
 * dx-reverse-info/cli-flags.md is emphatic that these are NOT equivalent, and that the
 * difference is observable:
 *
 *   dxl_cmd_param   ParseParam(s,"X")  -- "-X" or "/X", must end at whitespace
 *   dxl_cmd_value   Parse(s,"X=",v)    -- case-insensitive substring "X=",
 *                                         value runs to whitespace, quotable
 *   dxl_cmd_find    appStrfind(s,"X")  -- raw case-insensitive substring,
 *                                         no leading '-' required at all
 *
 * The last one is the surprising one. "readini" and the four single-instance
 * bypass tokens match anywhere in the command line, including inside a map
 * name or a URL. A port that reached for a normal argv parser here would
 * behave differently from the original in ways players would eventually hit,
 * so the launcher keeps the command line as one string and matches on it.
 */
#ifndef DXL_CMDLINE_H
#define DXL_CMDLINE_H

#include "core/common.h"

/* Joins argv[1..] with single spaces -- the engine sees one string, not a
 * vector, and the substring semantics above only make sense on the joined
 * form. Caller frees. */
char *dxl_cmdline_join(int argc, char *const *argv);

/* ParseParam: token preceded by '-' or '/' and followed by end-of-string or
 * whitespace. Case-insensitive. */
int dxl_cmd_param(const char *cmdline, const char *name);

/* Parse: finds "<name>=" anywhere, case-insensitively, and copies the value
 * up to the next whitespace into out. A double-quoted value may contain
 * spaces. Returns 1 if found. out is always NUL-terminated when size > 0. */
int dxl_cmd_value(const char *cmdline, const char *name, char *out, size_t size);

/* appStrfind: raw case-insensitive substring. */
int dxl_cmd_find(const char *cmdline, const char *token);

#endif
