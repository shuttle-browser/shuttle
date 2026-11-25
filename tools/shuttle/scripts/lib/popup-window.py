#!/usr/bin/env python3
# Copyright 2026 Silkweave
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Read/capture one native X11 test window. Requires a disposable display."""
import ctypes as c
import json
import os
import struct
import sys
import time
import zlib
from collections import Counter

assert os.environ.get('DISPLAY') and os.environ.get('SHUTTLE_POPUP_TEST_DISPLAY') == os.environ['DISPLAY'], 'Use a dedicated test display'
x = c.CDLL('libX11.so.6')
x.XOpenDisplay.restype = c.c_void_p
x.XOpenDisplay.argtypes = [c.c_char_p]
d = x.XOpenDisplay(None)
assert d, 'Cannot open test display'
x.XDefaultRootWindow.argtypes = [c.c_void_p]
x.XDefaultRootWindow.restype = c.c_ulong
x.XQueryTree.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ulong)), c.POINTER(c.c_uint)]
x.XFetchName.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_char_p)]
x.XFree.argtypes = [c.c_void_p]
x.XCloseDisplay.argtypes = [c.c_void_p]

x.XInternAtom.argtypes = [c.c_void_p, c.c_char_p, c.c_int]
x.XInternAtom.restype = c.c_ulong
x.XGetWindowProperty.argtypes = [c.c_void_p, c.c_ulong, c.c_ulong, c.c_long, c.c_long, c.c_int, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_int), c.POINTER(c.c_ulong), c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ubyte))]
name_atom = x.XInternAtom(d, b'_NET_WM_NAME', 0)

def windows(w):
    actual, fmt, count, remaining, data = c.c_ulong(), c.c_int(), c.c_ulong(), c.c_ulong(), c.POINTER(c.c_ubyte)()
    if x.XGetWindowProperty(d, w, name_atom, 0, 4096, 0, 0, c.byref(actual), c.byref(fmt), c.byref(count), c.byref(remaining), c.byref(data)) == 0 and data:
        title = bytes(data[:count.value]).decode('utf-8', errors='replace')
        x.XFree(data)
        if sys.argv[1] in title:
            yield w, title
    root, parent, children, count = c.c_ulong(), c.c_ulong(), c.POINTER(c.c_ulong)(), c.c_uint()
    if x.XQueryTree(d, w, c.byref(root), c.byref(parent), c.byref(children), c.byref(count)):
        ids = list(children[:count.value])
        if children:
            x.XFree(children)
        for child in ids:
            yield from windows(child)

matches = list(windows(x.XDefaultRootWindow(d)))
assert len(matches) == 1, f'Expected one window matching {sys.argv[1]!r}, got {matches}'
w, title = matches[0]
result = {'title': title, 'window': w}
if len(sys.argv) > 2:
    x.XRaiseWindow.argtypes = [c.c_void_p, c.c_ulong]
    x.XSync.argtypes = [c.c_void_p, c.c_int]
    x.XRaiseWindow(d, w)
    x.XSync(d, 0)
    time.sleep(0.15)
    root, px, py, width, height, border, depth = c.c_ulong(), c.c_int(), c.c_int(), c.c_uint(), c.c_uint(), c.c_uint(), c.c_uint()
    x.XGetGeometry.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_uint), c.POINTER(c.c_uint), c.POINTER(c.c_uint), c.POINTER(c.c_uint)]
    assert x.XGetGeometry(d, w, c.byref(root), c.byref(px), c.byref(py), c.byref(width), c.byref(height), c.byref(border), c.byref(depth))
    class XImage(c.Structure):
        _fields_ = [('width',c.c_int),('height',c.c_int),('xoffset',c.c_int),('format',c.c_int),('data',c.c_void_p),('byte_order',c.c_int),('bitmap_unit',c.c_int),('bitmap_bit_order',c.c_int),('bitmap_pad',c.c_int),('depth',c.c_int),('bytes_per_line',c.c_int),('bits_per_pixel',c.c_int),('red_mask',c.c_ulong),('green_mask',c.c_ulong),('blue_mask',c.c_ulong)]
    x.XGetImage.argtypes = [c.c_void_p,c.c_ulong,c.c_int,c.c_int,c.c_uint,c.c_uint,c.c_ulong,c.c_int]
    x.XGetImage.restype = c.POINTER(XImage)
    im = x.XGetImage(d,w,0,0,width.value,height.value,0xffffffff,2)
    assert im and im.contents.bits_per_pixel == 32 and im.contents.byte_order == 0
    raw = c.string_at(im.contents.data, im.contents.bytes_per_line * height.value)
    stride = im.contents.bytes_per_line
    rows = [b''.join(raw[y*stride+i:y*stride+i+3][::-1] for i in range(0,width.value*4,4)) for y in range(height.value)]
    def chunk(kind, data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    png = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width.value,height.value,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+r for r in rows)))+chunk(b'IEND',b'')
    with open(sys.argv[2], 'wb') as out:
        out.write(png)
    colors = Counter(tuple(row[i:i+3]) for row in rows[:90] for i in range(0,len(row),3))
    result.update(width=width.value,height=height.value,chromeColors=[{'rgb':rgb,'pixels':n} for rgb,n in colors.most_common(16)])
    x.XDestroyImage.argtypes = [c.POINTER(XImage)]
    x.XDestroyImage(im)
x.XCloseDisplay(d)
print(json.dumps(result))
