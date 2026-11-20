#!/usr/bin/env python3
"""DXLauncher's binary patches: made here, applied by src/core/patch.c.

    python3 -I tools/dxpatch.py make  <source> <result> <patch> [--chain N] [--min K]
    python3 -I tools/dxpatch.py apply <source> <patch> <result>
    python3 -I tools/dxpatch.py info  <patch>

The format (docs/LAUNCHER.md, "DXController"), every number little-endian:

    8 bytes   "DXLPATCH"
    4 bytes   the format's version, 1
    8 bytes   the source's size        20 bytes  the source's SHA-1
    8 bytes   the result's size        20 bytes  the result's SHA-1

then operations to the end of the file, each begun by a LEB128 number v,
its low two bits the kind and the rest, n = v >> 2, a length of 1 or more:

    0   add: n bytes follow, put into the result as they are
    1   copy n bytes of the source from the diagonal: the end of the last
        copy (0 at the start) plus the bytes added since it
    2   the same from the diagonal plus a distance, a second LEB128 number,
        zigzag-signed (0 1 2 3 4 ... for 0 -1 1 -2 2 ...)

A patch applies only to a source of exactly that size and SHA-1, and gives
a result of exactly that size and SHA-1, or nothing. Standard library alone.
"""

import hashlib
import struct
import sys
from array import array

MAGIC = b"DXLPATCH"
VERSION = 1
HEADER = struct.Struct("<8sIQ20sQ20s")
ADD, COPY, COPY_AT = 0, 1, 2


def uleb(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def zigzag(d):
    return (d << 1) if d >= 0 else ((-d << 1) - 1)


def unzigzag(z):
    return (z >> 1) if not z & 1 else -((z + 1) >> 1)


def read_uleb(data, pos):
    n = shift = 0
    while True:
        if pos >= len(data) or shift > 56:
            raise ValueError("a number cut short or too long")
        b = data[pos]
        pos += 1
        n |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            return n, pos


def match_len(a, i, b, j, limit):
    """How many bytes a[i:] and b[j:] share, up to limit, by halving steps."""
    n = 0
    step = 4096
    while n < limit:
        s = min(step, limit - n)
        if a[i + n:i + n + s] == b[j + n:j + n + s]:
            n += s
            step *= 2
        elif s == 1:
            break
        else:
            step = max(1, s // 2)
    return n


def op_cost(length, distance):
    """The bytes a copy takes: its number, and its distance unless 0."""
    c = len(uleb(length << 2))
    return c if distance == 0 else c + len(uleb(zigzag(distance)))


class Index:
    """Every position of the source, by a hash of the K bytes there."""

    BITS = 22

    def __init__(self, src, k):
        self.k = k
        self.mask = (1 << self.BITS) - 1
        self.head = array("i", [-1]) * (1 << self.BITS)
        self.next = array("i", [-1]) * max(1, len(src))
        head, nxt = self.head, self.next
        # Positions added from the end, so a chain runs from the source's start.
        for p in range(len(src) - k, -1, -1):
            h = self.hash(src[p:p + k])
            nxt[p] = head[h]
            head[h] = p

    def hash(self, key):
        # Python's own hash() of bytes changes from run to run: this one
        # keeps a patch the same bytes each time it is made.
        return ((int.from_bytes(key, "little") * 0x9E3779B97F4A7C15)
                >> (64 - self.BITS)) & self.mask

    def candidates(self, key, chain):
        p = self.head[self.hash(key)]
        n = 0
        while p >= 0 and (chain <= 0 or n < chain):
            yield p
            p = self.next[p]
            n += 1


def make(src, dst, chain=64, k=6):
    """The operations that make dst from src: greedy, with one step of
    lookahead, each place's best of the diagonal (the same bytes replaced),
    the last copy's end (bytes put in before them) and the index's."""
    index = Index(src, k)
    ops = []
    pending = bytearray()
    st = {"cursor": 0, "last_t": 0}
    t = 0
    n, ns = len(dst), len(src)

    def best_at(t):
        cursor, last_t = st["cursor"], st["last_t"]
        diag = cursor + (t - last_t)
        best = (0, 0, 0)  # (gain, s, length)
        tried = set()

        def weigh(s):
            nonlocal best
            ln = match_len(src, s, dst, t, min(ns - s, n - t))
            if ln:
                g = ln - op_cost(ln, s - diag)
                if g > best[0]:
                    best = (g, s, ln)

        for s in (diag, cursor):
            if 0 <= s < ns and s not in tried:
                tried.add(s)
                weigh(s)
        if t + k <= n:
            key = dst[t:t + k]
            for s in index.candidates(key, chain):
                if s not in tried and src[s:s + k] == key:
                    tried.add(s)
                    weigh(s)
        return best

    while t < n:
        g, s, ln = best_at(t)
        # A copy pays when it saves more than it costs and the add header it
        # may break off; a better one starting a byte on is waited for.
        if g >= 2 and ln < 64 and t + 1 < n:
            g2, _, _ = best_at(t + 1)
            if g2 > g + 1:
                pending.append(dst[t])
                t += 1
                continue
        if g >= 2:
            if pending:
                ops.append((ADD, bytes(pending)))
                pending = bytearray()
            diag = st["cursor"] + (t - st["last_t"])
            ops.append((COPY, s - diag, ln))
            st["cursor"] = s + ln
            t += ln
            st["last_t"] = t
        else:
            pending.append(dst[t])
            t += 1
    if pending:
        ops.append((ADD, bytes(pending)))
    return ops


def encode(src, dst, ops):
    out = bytearray(HEADER.pack(MAGIC, VERSION, len(src), hashlib.sha1(src).digest(),
                                len(dst), hashlib.sha1(dst).digest()))
    for op in ops:
        if op[0] == ADD:
            out += uleb(len(op[1]) << 2 | ADD)
            out += op[1]
        elif op[1] == 0:
            out += uleb(op[2] << 2 | COPY)
        else:
            out += uleb(op[2] << 2 | COPY_AT)
            out += uleb(zigzag(op[1]))
    return bytes(out)


def apply(src, patch):
    """The result, as src/core/patch.c makes it, every check its own."""
    if len(patch) < HEADER.size:
        raise ValueError("no patch: shorter than its header")
    magic, ver, ssize, ssha, rsize, rsha = HEADER.unpack_from(patch)
    if magic != MAGIC or ver != VERSION:
        raise ValueError("not a patch of this format")
    if len(src) != ssize or hashlib.sha1(src).digest() != ssha:
        raise ValueError("the source is not the one the patch is for")
    out = bytearray()
    pos, cursor, added = HEADER.size, 0, 0
    while pos < len(patch):
        v, pos = read_uleb(patch, pos)
        kind, ln = v & 3, v >> 2
        if ln == 0 or kind == 3:
            raise ValueError("an operation of no kind or no length")
        if ln > rsize - len(out):
            raise ValueError("an operation reaches past the result")
        if kind == ADD:
            if ln > len(patch) - pos:
                raise ValueError("the patch is cut short")
            out += patch[pos:pos + ln]
            pos += ln
            added += ln
            continue
        d = 0
        if kind == COPY_AT:
            z, pos = read_uleb(patch, pos)
            d = unzigzag(z)
        s = cursor + added + d
        if s < 0 or s > len(src) or ln > len(src) - s:
            raise ValueError("a copy reaches past the source")
        out += src[s:s + ln]
        cursor, added = s + ln, 0
    if len(out) != rsize or hashlib.sha1(out).digest() != rsha:
        raise ValueError("the result is not the one the patch makes")
    return bytes(out)


def main(argv):
    if len(argv) >= 4 and argv[0] == "make":
        chain, k = 64, 6
        rest = argv[4:]
        while rest:
            if rest[0] == "--chain" and len(rest) > 1:
                chain = int(rest[1])
            elif rest[0] == "--min" and len(rest) > 1:
                k = int(rest[1])
            else:
                sys.exit("unknown option " + rest[0])
            rest = rest[2:]
        with open(argv[1], "rb") as f:
            src = f.read()
        with open(argv[2], "rb") as f:
            dst = f.read()
        ops = make(src, dst, chain, k)
        patch = encode(src, dst, ops)
        if apply(src, patch) != dst:
            sys.exit("the patch made does not give the result")
        with open(argv[3], "wb") as f:
            f.write(patch)
        adds = sum(len(o[1]) for o in ops if o[0] == ADD)
        copies = [o for o in ops if o[0] == COPY]
        print("%s: %d bytes, %d operations (%d copies, %d off the diagonal; %d added bytes)"
              % (argv[3], len(patch), len(ops), len(copies), sum(1 for o in copies if o[1]), adds))
        return 0
    if len(argv) == 4 and argv[0] == "apply":
        with open(argv[1], "rb") as f:
            src = f.read()
        with open(argv[2], "rb") as f:
            patch = f.read()
        out = apply(src, patch)
        with open(argv[3], "wb") as f:
            f.write(out)
        return 0
    if len(argv) == 2 and argv[0] == "info":
        with open(argv[1], "rb") as f:
            patch = f.read()
        magic, ver, ssize, ssha, rsize, rsha = HEADER.unpack_from(patch)
        print("version %d\nsource %d bytes, SHA-1 %s\nresult %d bytes, SHA-1 %s"
              % (ver, ssize, ssha.hex(), rsize, rsha.hex()))
        return 0
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
