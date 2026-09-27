# Port: Xbox 360 (planned)

## Status

Planned. Nothing is built yet, and what the port needs has not been worked
out. `port.cmake` stops the configure with a pointer here, and `port.sh`
refuses to fetch anything.

Unlike the other ports, the console is not POSIX, so the launcher's few OS
dependencies ([`docs/PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#other-operating-systems))
would each need a version of their own.
