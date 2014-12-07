#!/usr/bin/env python3

import sys

# Hinawa-1.0 gir
from gi.repository import Hinawa

# Gtk+3 gir
from gi.repository import Gtk

# Qt5 python binding
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QApplication, QWidget

try:
	fw_unit = Hinawa.FwUnit.new("/dev/fw1")
except Exception as e:
	print(e);
	sys.exit;

try:
	resp = Hinawa.FwResp.new(fw_unit)
except Exception as e:
	print(e);
	sys.exit;

try:
	req = Hinawa.FwReq.new()
except Exception as e:
	print(e);
	sys.exit;

try:
	snd_unit = Hinawa.SndUnit.new("hw:0")
except Exception as e:
	print(e)
	sys.exit()
print(snd_unit.get_property("name"))
print(snd_unit.get_property("iface"))
print(snd_unit.get_property("card"))
print(snd_unit.get_property("device"))
print("%016x" % snd_unit.get_property("guid"))
print(snd_unit.get_property("streaming"))

class Sample(Gtk.Window):
    def __init(self):
        Gtk.Window.__init__(self, title="Hinawa-1.0 gir sample")
def handle_request(self):
	print("handle request")

def handle_lock_status(self):
	print("change lock status")

win = Gtk.Window()
win.connect("delete-event", Gtk.main_quit)
win.show_all()

fw_unit.listen()
resp.connect("request", handle_request)
resp.register(0xfffff0000d00, 256)

snd_unit.connect("lock-status", handle_lock_status)
snd_unit.listen()

Gtk.main()

snd_unit.unlisten()

resp.unregister()
fw_unit.unlisten()

sys.exit()
