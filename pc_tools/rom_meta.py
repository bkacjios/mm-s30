#!/usr/bin/env python3
"""
rom_meta.py -- MIPS-free metadata for the S30 Majora's Mask asset build
(adapted from ports/oot's).

Everything is produced on the host, without a MIPS toolchain, from:

  * the in-repo `spec/` (preprocessed with `gcc -E`),
  * the decomp's host tools tools/buildtools/mkldscript and mkdmadata,
  * the audio tables read directly out of the decompressed baserom,
  * dmadata (the baserom's own file table) for the raw audio blob locations.

Layout policy: the port's ROM (VROM) layout is recomputed cumulatively from
the *port-compiled* segment sizes (compute_vrom_layout), not inherited from
the N64 build. gDmaDataTable (dmadata_table_spec.h) and gVromTable both come
from that one layout, so the game's DMA requests and reimpl.c's segment
lookup stay consistent.

make_assets.py imports this module, takes the static metadata up front, and
calls compute_vrom_layout() once it knows the real port segment sizes.

Standalone use:
  rom_meta.py --version n64-us            # print metadata summary
  rom_meta.py --version n64-us --cflags   # the version -D flags only
"""

import argparse
import json
import os
import re
import struct
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Host (target-agnostic) toolchain used only to build the layout tools and
# preprocess the spec. `gcc` is clang on macOS; both are fine.
CPP = os.environ.get("CPP", "gcc -E")


# ---------------------------------------------------------------------------
# Version configuration -- parsed from the decomp Makefile so any supported
# ROM works without a hand-maintained table here.
# ---------------------------------------------------------------------------
def load_version_config(version):
    """MM's per-version settings: only the version macro (MM_VERSION=N64_US)."""
    if not os.path.isdir(os.path.join(REPO, "baseroms", version)):
        raise SystemExit(f"rom_meta: unknown version '{version}' (no baseroms/{version})")
    return {
        "version": version,
        "version_macro": version.upper().replace("-", "_").replace(".", "_"),
    }


def cpp_defines(cfg):
    """The -D flags used to preprocess the spec (as the decomp's GCC build).
    Returned as a list (never a string)."""
    return ["-DCOMPILER_GCC", "-DNON_MATCHING", "-DAVOID_UB",
            f"-DMM_VERSION={cfg['version_macro']}"]


# ---------------------------------------------------------------------------
# Host tools.
# ---------------------------------------------------------------------------
def ensure_host_tools():
    """The decomp's mkldscript/mkdmadata (tools/buildtools, built by its
    `make setup`). Returns (mkldscript, mkdmadata)."""
    d = os.path.join(REPO, "tools", "buildtools")
    tools = (os.path.join(d, "mkldscript"), os.path.join(d, "mkdmadata"))
    if not all(os.access(t, os.X_OK) for t in tools):
        subprocess.run(["make", "-C", d], check=True)
    return tools


def preprocess_spec(cfg):
    """gcc -E the master spec exactly like the Makefile's build/<v>/spec rule."""
    outdir = os.path.join(REPO, "build_assets", "meta")
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, "spec")
    cmd = CPP.split() + ["-P", "-xc", "-fno-dollars-in-identifiers"] + cpp_defines(cfg)
    cmd += ["-I.", os.path.join("spec", "spec")]
    with open(out, "w") as f:
        r = subprocess.run(cmd, cwd=REPO, stdout=f, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        raise SystemExit("rom_meta: spec preprocessing failed:\n" + r.stderr)
    # The Makefile applies BUILD_DIR_REPLACE; mirror it so include paths resolve.
    txt = open(out).read().replace("$(BUILD_DIR)", f"build/{cfg['version']}")
    open(out, "w").write(txt)
    return out


# ---------------------------------------------------------------------------
# Segment metadata from the spec + mkldscript.
# ---------------------------------------------------------------------------
def parse_spec_fields(spec_path):
    """Per-segment fields we need: romalign (for the VROM layout) and a flag for
    whether the segment carries ROM data (has includes)."""
    segs = {}
    name = None
    order = []
    for line in open(spec_path):
        s = line.strip()
        if s.startswith("beginseg"):
            name = None
        elif s.startswith("name "):
            name = s.split('"')[1]
            segs[name] = {"romalign": None, "has_data": False}
            order.append(name)
        elif s.startswith("romalign") and name:
            segs[name]["romalign"] = int(s.split()[1], 0)
        elif s.startswith("include") and name:
            segs[name]["has_data"] = True
        elif s.startswith("endseg"):
            name = None
    return segs, order


def segment_bases(cfg, mkldscript, spec_path):
    """VRAM base per segment, via mkldscript (proven byte-identical to the .ld)."""
    ld = os.path.join(REPO, "build_assets", "meta", "mm.ld")
    subprocess.run([mkldscript, spec_path, ld], check=True)
    bases = {}
    for m in re.finditer(r'\.\.(\w+)\s+(0x[0-9A-Fa-f]+)\s*:', open(ld).read()):
        bases[m.group(1)] = int(m.group(2), 16)
    return bases


def dma_order(cfg, mkdmadata, spec_path):
    """Ordered list of DMA'd segment names (mkdmadata's dmadata_table_spec.h).
    This is the authoritative order the ROM/VROM layout follows."""
    hdr = os.path.join(REPO, "build_assets", "meta", "dmadata_table_spec.h")
    cr = os.path.join(REPO, "build_assets", "meta", "compress_ranges.txt")
    subprocess.run([mkdmadata, spec_path, hdr, cr], check=True)
    names = []
    for m in re.finditer(r'DEFINE_DMA_ENTRY(?:_SYMS)?\((\w+),', open(hdr).read()):
        names.append(m.group(1))
    return names


# ---------------------------------------------------------------------------
# Audio symbols (Sequence_N / Soundfont_N / SampleBank_N Start/Size).
# ---------------------------------------------------------------------------
def _read_baserom(cfg):
    p = os.path.join(REPO, "baseroms", cfg["version"], "baserom-decompressed.z64")
    if not os.path.exists(p):
        raise SystemExit(f"rom_meta: {p} not found")
    return open(p, "rb").read()


def _audio_tables(rom, blob_sizes):
    """Find the soundfont, sequence and samplebank AudioTableHeaders in the
    (uncompressed) baserom by shape: a u16 numEntries, then 16-byte entries
    {romAddr, size} whose real (size != 0) entries tile one audio blob from
    offset 0; size == 0 marks an alias entry (romAddr = the aliased index).
    Each table is identified by the blob it tiles (Audiobank -> soundfonts,
    Audioseq -> sequences, Audiotable -> samplebanks), allowing for the blob's
    alignment padding after the last entry.
    Version-agnostic; no symbols required."""
    found = {}
    for base in range(16, len(rom) - 32, 16):
        ra0, sz0 = struct.unpack_from(">II", rom, base)
        if ra0 != 0 or not (0 < sz0 < 0x800000):
            continue
        num = struct.unpack_from(">H", rom, base - 16)[0]
        if not (2 <= num <= 4096) or base + num * 16 > len(rom):
            continue
        entries = [struct.unpack_from(">II", rom, base + i * 16) for i in range(num)]
        off = 0
        ok = True
        for ra, sz in entries:
            if sz == 0:
                if ra >= num:
                    ok = False
                    break
                continue
            if ra != off:
                ok = False
                break
            off += sz
        if not ok:
            continue
        for name, size in blob_sizes.items():
            if name not in found and 0 <= size - off < 0x40:
                found[name] = entries
        if len(found) == 3:
            return found
    raise SystemExit(f"rom_meta: audio tables not found in baserom (found {sorted(found)})")


def audio_symbols(cfg, blobs):
    """Return {sym_name: value} for every Sequence_/Soundfont_/SampleBank_ Start/Size."""
    syms = {}
    tables = _audio_tables(_read_baserom(cfg), {n: ve - vs for n, (vs, ve) in blobs.items()})
    for prefix, blob in (("Soundfont", "Audiobank"), ("Sequence", "Audioseq"),
                         ("SampleBank", "Audiotable")):
        for n, (ra, sz) in enumerate(tables[blob]):
            # Alias entries (size 0, romAddr = aliased index): samplebank
            # aliases get no symbols; sequence aliases keep theirs
            if sz == 0 and prefix != "Sequence":
                continue
            syms[f"{prefix}_{n}_Start"] = ra
            syms[f"{prefix}_{n}_Size"] = sz
    return syms


# ---------------------------------------------------------------------------
# Raw audio blob locations in the baserom (for byte extraction) + D_ constants.
# ---------------------------------------------------------------------------
def _dmadata_entries(cfg):
    dmadata = open(os.path.join(REPO, "extracted", cfg["version"],
                                "baserom", "dmadata"), "rb").read()
    entries = []
    for off in range(0, len(dmadata), 16):
        vs, ve, rs, re_ = struct.unpack_from(">IIII", dmadata, off)
        if (vs, ve, rs, re_) == (0, 0, 0, 0):
            break
        entries.append((vs, ve))
    return entries


def audio_blobs(cfg, dma_names):
    """{Audiobank/Audioseq/Audiotable: (vrom_start, vrom_end)} from dmadata,
    name-anchored via the dma order (1:1 with dmadata entries)."""
    blobs = {}
    for name, (vs, ve) in zip(dma_names, _dmadata_entries(cfg)):
        if name in ("Audiobank", "Audioseq", "Audiotable"):
            blobs[name] = (vs, ve)
    return blobs


def baserom_sizes(cfg, dma_names):
    """{name: uncompressed byte size} for every DMA segment, from the baserom's
    own dmadata. Used to size segments make_assets.py does not compile (makerom,
    boot, code, ...) so their option-A VROM slots don't collapse."""
    return {name: ve - vs
            for name, (vs, ve) in zip(dma_names, _dmadata_entries(cfg))}


def linker_symbols():
    """The handful of D_XXXXXXXX segment-slot constants the runtime references.
    Each equals the hex embedded in its name (D_06000000 == 0x06000000)."""
    # Only the segment-slot constants (D_NN000000, value == name); other D_
    # symbols the sources reference (e.g. RAM addresses) are real symbols
    # resolved elsewhere, not linker-defined here. Scan the whole src/ tree,
    # since these are referenced from actor overlays too (e.g. ovl_Bg_Mjin).
    r = subprocess.run(
        ["grep", "-rhoE", r"\bD_[0-9A-Fa-f]{2}000000\b", os.path.join(REPO, "src")],
        capture_output=True, text=True)
    syms = {}
    for name in set(r.stdout.split()):
        syms[name] = int(name[2:], 16)
    return syms


# ---------------------------------------------------------------------------
# OPTION A: recompute the port VROM layout cumulatively from port segment sizes.
# ---------------------------------------------------------------------------
def compute_vrom_layout(order, sizes, romaligns):
    """Replicates mkldscript's _RomSize accumulation (align start & end by
    `romalign` where present). `sizes` maps segment name -> port ROM-resident byte
    size. Returns {name: (vrom_start, vrom_end)} in `order`."""
    layout = {}
    romsize = 0
    for name in order:
        ra = romaligns.get(name)
        if ra:
            romsize = (romsize + ra - 1) & ~(ra - 1)
        start = romsize
        romsize += sizes.get(name, 0)
        end = romsize
        if ra:
            romsize = (romsize + ra - 1) & ~(ra - 1)
        layout[name] = (start, end)
    return layout


# ---------------------------------------------------------------------------
# Top-level: gather everything static (no port sizes needed yet).
# ---------------------------------------------------------------------------
def gather(version):
    cfg = load_version_config(version)
    mkldscript, mkdmadata = ensure_host_tools()
    spec_path = preprocess_spec(cfg)
    spec_fields, spec_order = parse_spec_fields(spec_path)
    bases = segment_bases(cfg, mkldscript, spec_path)
    dnames = dma_order(cfg, mkdmadata, spec_path)
    return {
        "cfg": cfg,
        "spec_path": spec_path,
        "segment_bases": bases,
        "dma_order": dnames,
        "romaligns": {n: spec_fields.get(n, {}).get("romalign") for n in dnames},
        "audio_blobs": audio_blobs(cfg, dnames),
        "audio_symbols": audio_symbols(cfg, audio_blobs(cfg, dnames)),
        "baserom_sizes": baserom_sizes(cfg, dnames),
        "linker_symbols": linker_symbols(),
    }


def main():
    ap = argparse.ArgumentParser(description="MIPS-free asset metadata generator")
    ap.add_argument("--version", default="n64-us")
    ap.add_argument("--cflags", action="store_true",
                    help="print only the version -D flags (for the game build)")
    args = ap.parse_args()
    if args.cflags:
        print(" ".join(cpp_defines(load_version_config(args.version))))
        return
    meta = gather(args.version)
    print(f"version         : {meta['cfg']['version']} ({meta['cfg']['version_macro']})")
    print(f"cpp defines     : {' '.join(cpp_defines(meta['cfg']))}")
    print(f"segment bases   : {len(meta['segment_bases'])}")
    print(f"dma segments    : {len(meta['dma_order'])}")
    print(f"audio symbols   : {len(meta['audio_symbols'])}")
    print(f"audio blobs     : {meta['audio_blobs']}")
    print(f"linker D_ syms  : {meta['linker_symbols']}")


if __name__ == "__main__":
    main()
