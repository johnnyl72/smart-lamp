"""
Where every generated file goes, in one place, so the scripts and the docs
agree. Folder numbers give the order you use them in.
"""
from pathlib import Path

HERE = Path(__file__).resolve().parent
PRINTS = HERE.parent / "3d-prints"

README = PRINTS / "READ ME FIRST.txt"
PRINT = PRINTS / "1 Print these"
MAKERWORLD = PRINTS / "2 MakerWorld upload kit"
LISTING = MAKERWORLD / "Listing text - copy into MakerWorld.md"
PICTURES = MAKERWORLD / "pictures"
PROFILES = MAKERWORLD / "print profiles - save Bambu Studio files here"
SCREEN = PRINTS / "3 Optional - base with screen window"

# Printed parts, in print order. The fit test can be any colour.
FIT_RIM = "1 Fit test - base rim"
DIFFUSER = "2 Diffuser - white PLA - print 2"
BASE = "3 Base - dark PLA - print 2"
SCREEN_BASE = "Base with screen window - dark PLA - print 2"
SCREEN_FIT = "Screen fit test"

# Linked Lamp's diffuser: its logo is engraved on top of ours when
# settings.TOP_TEXT is empty.
ORIG_DIFFUSER = HERE / "linked-lamp-original" / "Neopixel Lamp - 3in Diffuser.step"
