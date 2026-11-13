#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 LAYER8.
"""GapM5 R3 safe StopWatch APP-only flasher.

No auto backup, erase_flash, bootloader/partition table/NVS/storage/otadata
writes, or OTA-slot changes. Reads 0x8000 partition table + OTA selector.
Stops without writing if the future boot APP is not uniquely determined.
Adapted from Layer812/bikeM5's publicly available R26 APP-only logic.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zlib

SECTOR=4096

class Stop(Exception): pass

def need(test, message):
    if not test: raise Stop(message)

@dataclass(frozen=True)
class Part:
    name: str
    kind: int
    subtype: int
    offset: int
    size: int
    flags: int


def esptool(port, *args, baud=115200, after="no-reset"):
    cmd=[sys.executable,"-m","esptool","--chip","esp32s3","--port",port,
         "--baud",str(baud),"--before","default-reset","--after",after,*args]
    print("EXEC:"," ".join(cmd),flush=True)
    p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                     encoding="utf-8",errors="replace")
    print(p.stdout,flush=True)
    need(p.returncode==0, f"esptool exit={p.returncode}")
    return p.stdout


def read_sector(port, off, target):
    for attempt in range(3):
        try:
            target.unlink(missing_ok=True)
            esptool(port,"read-flash",hex(off),hex(SECTOR),str(target))
            raw=target.read_bytes()
            need(len(raw)==SECTOR,"truncated sector read")
            return raw
        except (Stop,OSError) as e:
            print(f"sector retry {attempt+1}/3: {e}",flush=True)
            if attempt==2:raise Stop(f"unable to read 0x{off:X}")


def parse_parts(raw,flash_size):
    need(len(raw)==SECTOR,"table sector size mismatch")
    parts=[]
    labels=set()
    end=False
    for off in range(0,0xc00,32):
        entry=raw[off:off+32]
        magic=struct.unpack_from("<H",entry)[0]
        if magic==0xEBEB:
            if entry[2:16]==b"\xff"*14:
                need(hashlib.md5(raw[:off]).digest()==entry[16:32],"partition table checksum mismatch")
            end=True
            break
        if magic==0xFFFF:
            end=True
            break
        need(magic==0x50AA,f"invalid partition entry {off:#x}")
        _,kind,sub,base,size,label,flags=struct.unpack("<HBBII16sI",entry)
        name=label.split(b"\x00",1)[0].decode("ascii")
        need(name and name not in labels,"duplicate/empty partition label")
        labels.add(name)
        need(size>0 and base>=0x9000 and base%SECTOR==0 and size%SECTOR==0,"invalid partition bounds")
        need(base+size<=flash_size,"partition crosses flash boundary")
        if kind==0: need(base%0x10000==0,"APP partition is not 64K-aligned")
        parts.append(Part(name,kind,sub,base,size,flags))
    need(end and bool(parts),"partition table invalid/incomplete")
    ordered=sorted(parts,key=lambda x:x.offset)
    for a,b in zip(ordered,ordered[1:]):need(a.offset+a.size<=b.offset,"overlapping partitions")
    return parts


def one(parts,kind,sub):
    found=[p for p in parts if p.kind==kind and p.subtype==sub]
    need(len(found)<=1,f"duplicate partition {kind:x}/{sub:x}")
    return found[0] if found else None


def boot_app(parts,ota):
    factory=one(parts,0,0)
    slots=[one(parts,0,0x10+i) for i in range(16)]
    n=0
    while n<16 and slots[n] is not None:n+=1
    need(not any(p is not None for p in slots[n:]),"non-contiguous OTA slots")
    if n==0:
        need(factory is not None,"no APP partitions")
        return factory,"factory only"
    otapart=one(parts,1,0)
    if otapart is None:
        if factory:return factory,"factory (no otadata)"
        return slots[0],"OTA0 (no otadata)"
    need(ota is not None and len(ota)==8192,"OTA selector read incomplete")
    valid=[]
    erased=True
    for idx in (0,1):
        v=ota[idx*SECTOR:idx*SECTOR+32]
        if v!=b"\xff"*32:erased=False
        seq=struct.unpack_from("<I",v,0)[0]
        state=struct.unpack_from("<I",v,24)[0]
        crc=struct.unpack_from("<I",v,28)[0]
        if seq in (0,0xffffffff):continue
        expect=zlib.crc32(struct.pack("<I",seq),0xffffffff)&0xffffffff
        if crc!=expect:continue
        if state in (0,2,0xffffffff):valid.append((seq,idx))
    if not valid:
        need(erased,"non-erased otadata has no valid boot slot")
        return (factory,"factory (blank otadata)") if factory else (slots[0],"OTA0 (blank otadata)")
    need(len(valid)!=2 or valid[0][0]!=valid[1][0],"ambiguous OTA sequence")
    if len(valid)==2:need(abs(valid[0][0]-valid[1][0])<0x80000000,"OTA seq wrap ambiguity")
    seq,_=max(valid)
    slot=(seq-1)%n
    return slots[slot],f"OTA{slot} (seq={seq})"


def check_image(path,selected):
    need(path.is_file(),"firmware.bin missing")
    size=path.stat().st_size
    need(64<=size<=selected.size,"APP image does not fit boot partition")
    with path.open("rb") as f:h=f.read(32)
    need(h[0]==0xE9 and 1<=h[1]<=16,"not an Espressif APP image")
    need(struct.unpack_from("<H",h,12)[0]==9,"BIN not ESP32-S3")
    need(((h[3]>>4)&0xF)==4,"BIN flash size is not 16MB")
    need((selected.flags&0x03)==0,"selected APP is encrypted/read-only")
    return size


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--port",required=True)
    parser.add_argument("--bin",required=True,type=Path)
    args=parser.parse_args()
    port=args.port.upper()
    need(re.fullmatch(r"COM\d{1,3}",port) is not None,"port must be COMxx")
    need(args.bin.is_file(),"no built firmware.bin")
    print("GapM5 R3: APP ONLY / no backup / no partition changes",flush=True)
    with tempfile.TemporaryDirectory(prefix="gapm5_part_") as td:
        folder=Path(td)
        flashinfo=esptool(port,"flash-id")
        need("ESP32-S3" in flashinfo,"connected chip is not ESP32-S3")
        match=re.search(r"(?:Detected flash size|Flash size)\s*:\s*(\d+)\s*MB",flashinfo,re.I)
        need(match is not None and int(match.group(1))==16,"flash size not 16MB")
        security=esptool(port,"get-security-info")
        need(re.search(r"Secure Boot:\s*Disabled",security,re.I),"Secure Boot status not disabled")
        need(re.search(r"Flash Encryption:\s*Disabled",security,re.I),"Flash Encryption status not disabled")
        table=read_sector(port,0x8000,folder/"partition.bin")
        parts=parse_parts(table,16*1024*1024)
        print("EXISTING PARTITIONS (READ ONLY):")
        for p in parts:print(f"  {p.name:16} {p.kind:02x}/{p.subtype:02x} {p.offset:#08x} + {p.size:#x}")
        otapart=one(parts,1,0)
        if otapart:
            need(otapart.size>=8192,"otadata too small")
            ota=read_sector(port,otapart.offset,folder/"ota0.bin")+read_sector(port,otapart.offset+SECTOR,folder/"ota1.bin")
        else:ota=None
        app,reason=boot_app(parts,ota)
        size=check_image(args.bin,app)
        print(f"BOOT SELECTION: {reason}")
        print(f"APP WRITE ONLY: {app.name} @ {app.offset:#08x} / {size} bytes")
        print("No change to bootloader/partition table/otadata/NVS/storage/other APPs.")
        esptool(port,"write-flash",hex(app.offset),str(args.bin),baud=460800,after="hard-reset")
        print("PASS: APP-only flash; if in ROM mode, close monitor then press power briefly.")

if __name__=="__main__":
    try:main()
    except (Stop,OSError,ValueError,UnicodeError,struct.error) as e:
        print("SAFE STOP:",e,file=sys.stderr)
        print("No full erase, bootloader, otadata, partition, NVS or storage modification attempted.",file=sys.stderr)
        sys.exit(2)
