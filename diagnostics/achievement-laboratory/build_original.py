#!/usr/bin/env python3
"""Assemble the original Mapper 0 diagnostics ROM with no dependencies."""
from pathlib import Path

code = bytearray()
labels = {}
fixups = []


def emit(*values):
    code.extend(values)


def label(name):
    labels[name] = 0xC000 + len(code)


def absolute(op, value):
    emit(op, value & 255, value >> 8)


def target(op, name, relative=False):
    emit(op, 0)
    fixups.append((len(code) - 1, name, relative))
    if not relative:
        emit(0)


def byte_store(address, value):
    emit(0xA9, value, 0x85, address)


def wait_for_blank(name):
    label(name)
    absolute(0x2C, 0x2002)
    target(0x10, name, True)


def ppu(address):
    for part in (address >> 8, address & 255):
        emit(0xA9, part)
        absolute(0x8D, 0x2006)


def ppu_byte(value):
    emit(0xA9, value)
    absolute(0x8D, 0x2007)


def screen(row, text):
    ppu(0x2000 + row * 32 + 3)
    for char in text:
        ppu_byte(ord(char))


label("reset")
emit(0x78, 0xD8, 0xA2, 0xFF, 0x9A, 0xA9, 0)
for address in (0x2000, 0x2001, 0x4010):
    absolute(0x8D, address)
absolute(0x2C, 0x2002)
wait_for_blank("boot1")
wait_for_blank("boot2")
ppu(0x2000)
emit(0xA9, 32, 0xA2, 4, 0xA0, 0)
label("clear")
absolute(0x8D, 0x2007)
emit(0xC8)
target(0xD0, "clear", True)
emit(0xCA)
target(0xD0, "clear", True)
ppu(0x3F00)
for color in (0x01, 0x30, 0x21, 0x12):
    ppu_byte(color)
for row, line in {
    4: "R36SX ACHIEVEMENT LAB",
    7: "COINS    $0010  ",
    9: "LEVEL    $0011  ",
    11: "HEALTH   $0012  ",
    13: "ENEMIES  $0013  ",
    16: "A COIN     B DAMAGE",
    18: "START LEVEL   SELECT RESET",
    20: "UP ENEMY      DOWN HEAL",
    23: "VALUES SHOWN IN HEX",
}.items():
    screen(row, line)
for address, value in enumerate((0, 1, 100, 0, 0, 0, 0, 0, 0), 0x10):
    byte_store(address, value)
label("main")
wait_for_blank("frame")
for row, address in ((7, 0x10), (9, 0x11), (11, 0x12), (13, 0x13)):
    ppu(0x2000 + row * 32 + 19)
    emit(0xA5, address, 0x4A, 0x4A, 0x4A, 0x4A)
    target(0x20, "hex")
    emit(0xA5, address, 0x29, 15)
    target(0x20, "hex")
emit(0xA9, 0)
for address in (0x2005, 0x2005, 0x2000):
    absolute(0x8D, address)
emit(0xA9, 0x0A)
absolute(0x8D, 0x2001)
byte_store(0x14, 0)
byte_store(0x18, 0)
emit(0xA5, 0x16, 0x85, 0x15, 0xA9, 1)
absolute(0x8D, 0x4016)
emit(0xA9, 0)
absolute(0x8D, 0x4016)
emit(0xA2, 8)
label("input")
absolute(0xAD, 0x4016)
emit(0x4A, 0x66, 0x16, 0xCA)
target(0xD0, "input", True)
emit(0xA5, 0x15, 0x49, 0xFF, 0x25, 0x16, 0x85, 0x17)
for bit, name in ((1, "coin"), (2, "damage"), (4, "resetdata"), (8, "level"),
                  (16, "enemy"), (32, "heal")):
    emit(0xA5, 0x17, 0x29, bit)
    target(0xF0, name, True)
    if bit == 1:
        emit(0xE6, 0x10)
    if bit == 2:
        emit(0xA5, 0x12)
        target(0xF0, name, True)
        emit(0xC6, 0x12)
        byte_store(0x18, 1)
    if bit == 4:
        for address, value in enumerate((0, 1, 100, 0, 0), 0x10):
            byte_store(address, value)
        byte_store(0x18, 1)
        target(0x4C, "main")
    if bit == 8:
        emit(0xE6, 0x11)
    if bit == 16:
        emit(0xE6, 0x13)
        byte_store(0x14, 1)
    if bit == 32:
        byte_store(0x12, 100)
    label(name)
target(0x4C, "main")
label("hex")
emit(0xC9, 10)
target(0x90, "digit", True)
emit(0x69, 6)  # carry is set, add seven to move 9 -> A
label("digit")
emit(0x18, 0x69, ord("0"))
absolute(0x8D, 0x2007)
emit(0x60)
label("interrupt")
emit(0x40)
for pos, name, relative in fixups:
    address = labels[name]
    if relative:
        delta = address - (0xC000 + pos + 1)
        assert -128 <= delta < 128
        code[pos] = delta & 255
    else:
        code[pos:pos + 2] = address.to_bytes(2, "little")
assert len(code) < 0x3FFA
code.extend(b"\xEA" * (0x3FFA - len(code)))
for name in ("interrupt", "reset", "interrupt"):
    code.extend(labels[name].to_bytes(2, "little"))

# Original 3×5 glyphs, stored in ordinary NES pattern tiles.
glyphs = {
    "A":"25755","B":"65656","C":"34443","D":"65556","E":"74647","F":"74644",
    "G":"34753","H":"55755","I":"72227","J":"11152","K":"55655","L":"44447",
    "M":"57755","N":"57775","O":"25552","P":"65644","Q":"25573","R":"65655",
    "S":"34616","T":"72222","U":"55557","V":"55552","W":"55775","X":"55255",
    "Y":"55222","Z":"71247","0":"75557","1":"26227","2":"71647","3":"71717",
    "4":"55711","5":"74617","6":"74657","7":"71222","8":"75757","9":"75717",
    "$":"27362",
}
tiles = bytearray(8192)
for char, rows in glyphs.items():
    for row, value in enumerate(rows):
        tiles[ord(char)*16 + row + 1] = int(value) << 4
rom = b"NES\x1A" + bytes([1, 1]) + bytes(10) + code + tiles
Path(__file__).with_name("r36sx_achievement_lab.nes").write_bytes(rom)
