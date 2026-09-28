#!/usr/bin/env python3
"""The recreation run live, start to end, where nobody sees it.

    tools/livecheck.py <GameDir> <out dir> [scenario ...]

DeusEx must be installed in <GameDir>/System with its run-game.sh and the
engine beside it (Port Ex Machina's scripts/recreation.sh install). The
checks run on a copy -- <out dir>/Game, the files a run writes copied, the
rest linked -- so the install itself is not written, and on a private X
display (Xvfb) where xdotool clicks, so nothing shows on the desktop and no
real input is used. The engine's audio goes to OpenAL Soft's null driver.
Each scenario's screens are saved in <out dir> as the display shows them,
with the launcher's log, and its results printed; the exit code is the
number of checks that failed.

Needs Xvfb, xdotool and ImageMagick's import. The scenarios, in order:

  quick        -consolecommand= and -testrendev=, which end with no game
  make         -make's critical box, OK, exit 1
  changevideo  the Renderer page, captioned, with the game's icon, and its
               Cancel
  firstrun     -firstrun: detection, the pages, Run!; the game up, EXEC=
               run, a second launch forwarded to it and travelled, its clean
               end; Running.ini gone, FirstRun 1100
  safe         -safe: SafeMode, its options, Run! -- the launcher again
               with the flags alone; the engine silent, in a 640x480
               window, with no pad
  crash        a killed game: Running.ini stays, the next launch is
               RecoveryMode, whose Cancel deletes it
  cd           CdPath pointed nowhere: the prompt, over the splash; its
               Cancel ends the run at once, Running.ini left
  server       -server: no wizard, a dedicated engine, stopped by a signal
"""
import os, re, shutil, signal, subprocess, sys, time

if len(sys.argv) < 3:
    sys.exit(__doc__)
SOURCE, OUT = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
ONLY = sys.argv[3:]
GAME = OUT + "/Game"
SYS = GAME + "/System"
results = []

# ---- the copy and the display -----------------------------------------------

WRITTEN = (".ini", ".int", ".log", ".txt")      # copied: what a run writes, and the strings
# Copied too: the launcher finds its folder through its own resolved path,
# and run-game.sh through its own.
PROGRAMS = ("DeusEx", "run-game.sh")


def make_copy():
    if not os.path.exists(SOURCE + "/System/DeusEx"):
        sys.exit("no DeusEx in %s/System -- scripts/recreation.sh install" % SOURCE)
    shutil.rmtree(GAME, ignore_errors=True)
    os.makedirs(SYS)
    for d in ("Save", "Cache"):                   # empty: the player's own are not touched
        os.makedirs(GAME + "/" + d)
    shutil.copytree(SOURCE + "/Help", GAME + "/Help")
    for d in ("Maps", "Music", "Sounds", "Textures"):
        os.symlink(SOURCE + "/" + d, GAME + "/" + d)
    for f in os.listdir(SOURCE + "/System"):
        src = SOURCE + "/System/" + f
        if f.lower().endswith(WRITTEN) or f in PROGRAMS:
            shutil.copy2(src, SYS)
        elif not f.startswith("Shot"):
            os.symlink(src, SYS + "/" + f)
    for f in ("Running.ini", "Detected.ini", "Detected.log"):
        if os.path.exists(SYS + "/" + f):
            os.remove(SYS + "/" + f)


def free_display():
    for n in range(90, 120):
        if not os.path.exists("/tmp/.X11-unix/X%d" % n):
            return ":%d" % n
    sys.exit("no free X display number")


DISPLAY = free_display()


def env(extra=None):
    e = dict(os.environ)
    e.pop("WAYLAND_DISPLAY", None)
    e.update(DISPLAY=DISPLAY, SDL_VIDEODRIVER="x11", ALSOFT_DRIVERS="null")
    e.update(extra or {})
    return e


# ---- driving it ---------------------------------------------------------------

def xdo(*args):
    return subprocess.run(["xdotool", *args], env=env(), capture_output=True, text=True).stdout.strip()


def shot(name):
    subprocess.run(["import", "-display", DISPLAY, "-window", "root", "%s/%s.png" % (OUT, name)], env=env())


def find_window(title, timeout=20):
    t0 = time.time()
    while time.time() - t0 < timeout:
        ids = xdo("search", "--onlyvisible", "--name", "^" + re.escape(title) + "$").split()
        if ids:
            return ids[0]
        time.sleep(0.25)
    return None


def window_icon(win):
    """The window's _NET_WM_ICON, read through Xlib: the first image's width,
    height and top-left pixel (0xAARRGGBB), or None."""
    import ctypes, ctypes.util
    x = ctypes.cdll.LoadLibrary(ctypes.util.find_library("X11") or "libX11.so.6")
    x.XOpenDisplay.restype = ctypes.c_void_p
    x.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x.XInternAtom.restype = ctypes.c_ulong
    x.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    ul, pul = ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong)
    x.XGetWindowProperty.argtypes = [ctypes.c_void_p, ul, ul, ctypes.c_long, ctypes.c_long, ctypes.c_int, ul,
                                     pul, ctypes.POINTER(ctypes.c_int), pul, pul, ctypes.POINTER(pul)]
    x.XFree.argtypes = [ctypes.c_void_p]
    x.XCloseDisplay.argtypes = [ctypes.c_void_p]
    dpy = x.XOpenDisplay(DISPLAY.encode())
    if not dpy:
        return None
    try:
        actual, fmt, n, after, data = ul(), ctypes.c_int(), ul(), ul(), pul()
        atom = x.XInternAtom(dpy, b"_NET_WM_ICON", 0)
        if x.XGetWindowProperty(dpy, int(win), atom, 0, 1 << 20, 0, 0, ctypes.byref(actual), ctypes.byref(fmt),
                                ctypes.byref(n), ctypes.byref(after), ctypes.byref(data)) != 0 or not data:
            return None
        # Format 32 comes as C longs: width, height, then the pixels.
        values = [data[i] & 0xffffffff for i in range(min(n.value, 3))]
        x.XFree(data)
        return tuple(values) if len(values) == 3 else None
    finally:
        x.XCloseDisplay(dpy)


def click(win, x, y):
    xdo("mousemove", "--window", win, str(x), str(y))
    time.sleep(0.15)
    xdo("click", "1")
    time.sleep(0.6)


def log():
    try:
        return open(SYS + "/DeusEx.log", encoding="latin1").read()
    except OSError:
        return ""


def wait_log(pattern, timeout=60):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if re.search(pattern, log()):
            return True
        time.sleep(0.25)
    return False


def check(name, ok, detail=""):
    results.append((name, ok))
    print(("PASS " if ok else "FAIL ") + name + ((": " + detail) if detail and not ok else ""), flush=True)


def launch(args, extra=None, fresh_log=True):
    # The launcher starts its log afresh; so does this, so that no line of
    # the last run's is taken for this one's.
    if fresh_log and os.path.exists(SYS + "/DeusEx.log"):
        os.remove(SYS + "/DeusEx.log")
    return subprocess.Popen([SYS + "/DeusEx", *args], env=env(extra), cwd=GAME,
                            stdout=open(OUT + "/launcher-output.txt", "a"), stderr=subprocess.STDOUT)


def ini(key):
    text = open(SYS + "/DeusEx.ini", encoding="latin1").read()
    m = re.search(r"^%s=([^\r\n]*)" % re.escape(key), text, re.M)
    return m.group(1) if m else None


def set_ini(key, value):
    p = SYS + "/DeusEx.ini"
    s = open(p, encoding="latin1", newline="").read()
    s, n = re.subn(r"^%s=[^\r\n]*" % re.escape(key), lambda m: "%s=%s" % (key, value), s, count=1, flags=re.M)
    assert n == 1, key
    open(p, "w", encoding="latin1", newline="").write(s)


def timeline(name, text):
    """The engine's own scripted run (VibeEngine's --timeline), through
    run-game.sh's DXL_ENGINE_ARGS: here, to end the game cleanly."""
    p = "%s/%s.timeline" % (OUT, name)
    open(p, "w").write(text)
    return {"DXL_ENGINE_ARGS": "--timeline=" + p}


def keep_log(name):
    if os.path.exists(SYS + "/DeusEx.log"):
        shutil.copy(SYS + "/DeusEx.log", "%s/%s.DeusEx.log" % (OUT, name))


def remove(name):
    if os.path.exists(SYS + "/" + name):
        os.remove(SYS + "/" + name)


# The controls' centres in the window, from the templates (src/wizard/wizard.c).
NEXT, CANCEL = (220, 395), (399, 395)
SAFE_SAFEMODE = (263, 279)
CD_CANCEL = (156 + 32, 42 + 13)     # the CD prompt's Cancel
MAKE_OK = (123 + 26, 55 + 13)       # the critical box's OK


# ---- the scenarios -----------------------------------------------------------

def scenario_quick():
    p = launch(["-consolecommand=flush"])
    rc = p.wait(20)
    L = log()
    check("consolecommand: ends with no game",
          rc == 0 and "Executing console command flush" in L and "starting the game" not in L)
    remove("Detected.ini")
    p = launch(["-testrendev=D3DDrv.D3DRenderDevice"])
    rc = p.wait(20)
    check("testrendev: Detected.ini written", rc == 0 and os.path.exists(SYS + "/Detected.ini"))


def scenario_make():
    p = launch(["-make"])
    w = find_window("Critical Error")
    check("make: the critical box", w is not None)
    if not w:
        p.kill()
        return
    shot("make")
    click(w, *MAKE_OK)
    rc = p.wait(20)
    check("make: exit 1", rc == 1, "exit %s" % rc)


def scenario_changevideo():
    p = launch(["-changevideo"])
    w = find_window("Deus Ex Video Configuration")
    check("changevideo: the Renderer page, captioned", w is not None)
    if not w:
        p.kill()
        return
    # DeusEx.exe's icon group 128, its 32x32 of 256 colours (tests/test_gamefiles.c)
    icon = window_icon(w)
    check("changevideo: the game's icon", icon == (32, 32, 0xff040404), repr(icon))
    time.sleep(3)
    shot("changevideo")
    click(w, *CANCEL)
    rc = p.wait(20)
    check("changevideo: Cancel ends it", rc == 0 and "wizard cancelled" in log(), "exit %s" % rc)


def scenario_firstrun():
    remove("Running.ini")
    execfile = OUT + "/exec.txt"
    open(execfile, "w").write("livecheckline\n")
    p = launch(["-firstrun", "EXEC=" + execfile], timeline("firstrun", "start 30 exit\n"))
    w = find_window("Deus Ex First-Time Configuration")
    check("firstrun: the wizard, captioned", w is not None)
    if not w:
        p.kill()
        return
    time.sleep(3)                                  # detection
    shot("firstrun-1-renderer")
    check("firstrun: detection ran", os.path.exists(SYS + "/Detected.ini"))
    for i in (2, 3):
        click(w, *NEXT)
        shot("firstrun-%d" % i)
    click(w, *NEXT)                                # Run!
    check("firstrun: the engine says it is up", wait_log(r"the engine is up", 60))
    check("firstrun: Running.ini while the game runs", os.path.exists(SYS + "/Running.ini"))
    q = launch(["01_NYC_UNATCOHQ.dx"], fresh_log=False)
    rc = q.wait(30)
    check("firstrun: a second launch is forwarded", rc == 0, "exit %s" % rc)
    check("firstrun: the game opens the forwarded map",
          wait_log(r"Launcher: open 01_NYC_UNATCOHQ\.dx", 20) and "WM_COPYDATA: 01_NYC_UNATCOHQ.dx" in log())
    rc = p.wait(90)
    keep_log("firstrun")
    L = log()
    check("firstrun: the game's clean end", rc == 0 and "the game ended: 0" in L, "exit %s" % rc)
    check("firstrun: Running.ini gone after", not os.path.exists(SYS + "/Running.ini"))
    check("firstrun: FirstRun 1100", ini("FirstRun") == "1100", str(ini("FirstRun")))
    check("firstrun: EXEC= ran in the game", "Unknown command: livecheckline" in L)


def scenario_safe():
    p = launch(["-safe"], timeline("safe", "start 15 exit\n"))
    w = find_window("Deus Ex Safe Mode")
    check("safe: SafeMode", w is not None)
    if not w:
        p.kill()
        return
    shot("safe-1")
    click(w, *SAFE_SAFEMODE)
    shot("safe-2-options")
    click(w, *NEXT)                                # Run!
    check("safe: the relaunched run starts the game", wait_log(r"the engine is up", 60))
    rc = p.wait(90)
    keep_log("safe")
    L = log()
    check("safe: relaunched with the flags alone",
          "Init: command line: -nosound -no3dsound -nohard -nohard -noddraw -defaultres "
          "-nommx -nokni -nok6 -nojoy\n" in L)
    check("safe: no sound", "-nosound: no sound" in L)
    check("safe: a 640x480 window", "Window: 640x480, in a window" in L)
    check("safe: no pad", "-nojoy: no pad" in L)
    check("safe: clean end", rc == 0, "exit %s" % rc)


def scenario_crash():
    p = launch([])
    check("crash: the game, with no wizard", wait_log(r"the engine is up", 60) and "wizard:" not in log())
    for k in subprocess.run(["pgrep", "-P", str(p.pid)], capture_output=True, text=True).stdout.split():
        os.kill(int(k), signal.SIGKILL)
    rc = p.wait(30)
    keep_log("crash-1")
    check("crash: the launcher sees an unclean end", rc == 137 and "not a clean end" in log(), "exit %s" % rc)
    check("crash: Running.ini stays", os.path.exists(SYS + "/Running.ini"))
    p = launch([])
    w = find_window("Deus Ex Recovery Mode")
    check("crash: the next launch is RecoveryMode", w is not None)
    if not w:
        p.kill()
        return
    shot("crash-2-recovery")
    click(w, *CANCEL)
    rc = p.wait(30)
    check("crash: its Cancel deletes Running.ini",
          rc == 0 and not os.path.exists(SYS + "/Running.ini"), "exit %s" % rc)


def scenario_cd():
    old = ini("CdPath")
    set_ini("CdPath", "..\\nowhere\\")
    try:
        p = launch([])
        w = find_window("Cd Required At Startup")
        check("cd: the prompt", w is not None)
        if not w:
            p.kill()
            return
        time.sleep(0.5)
        shot("cd-prompt")
        click(w, *CD_CANCEL)
        rc = p.wait(20)
        keep_log("cd")
        check("cd: its Cancel ends the run", rc == 0 and "CD check cancelled" in log(), "exit %s" % rc)
        check("cd: Running.ini left", os.path.exists(SYS + "/Running.ini"))
    finally:
        set_ini("CdPath", old)
        remove("Running.ini")


def scenario_server():
    p = launch(["-server"])
    time.sleep(12)
    p.send_signal(signal.SIGINT)
    rc = p.wait(30)
    keep_log("server")
    L = log()
    check("server: no wizard", "wizard:" not in L)
    check("server: the engine dedicated", "UdpServerQuery" in L or "DoUplink" in L)
    check("server: stopped by the signal", "stopping the game" in L and rc in (130, 137, 143), "exit %s" % rc)
    remove("Running.ini")


ALL = ["quick", "make", "changevideo", "firstrun", "safe", "crash", "cd", "server"]

os.makedirs(OUT, exist_ok=True)
make_copy()
xvfb = subprocess.Popen(["Xvfb", DISPLAY, "-screen", "0", "1280x800x24", "-ac", "-nolisten", "tcp"],
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2)
try:
    for name in ONLY or ALL:
        print("==", name, flush=True)
        try:
            globals()["scenario_" + name]()
        except Exception as e:
            check(name + ": ran", False, repr(e))
finally:
    # Whatever a failed scenario left running: programs of the copy's.
    for pid in os.listdir("/proc"):
        try:
            argv0 = open("/proc/%s/cmdline" % pid, "rb").read().split(b"\0")[0].decode()
            if argv0.startswith(SYS + "/"):
                os.kill(int(pid), signal.SIGKILL)
        except (OSError, ValueError):
            pass
    xvfb.terminate()
failed = sum(1 for _, ok in results if not ok)
print("\n%d of %d passed" % (len(results) - failed, len(results)))
sys.exit(failed)
