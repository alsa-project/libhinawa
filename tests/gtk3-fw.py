#!/usr/bin/env python3

import sys

# Hinawa-1.0 gir
from gi.repository import Hinawa

# Gtk+3 gir
from gi.repository import Gtk

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

try:
    path = "/dev/%s" % snd_unit.get_property("device")
    fw_unit = Hinawa.FwUnit.new(path)
except Exception as e:
    print(e)
    sys.exit()

try:
    req = Hinawa.FwReq.new()
except Exception as e:
    print(e)
    sys.exit()

# response
def handle_request(resp, tcode, frame, private_data):
    template = "{0}: 0x{1:02x}{2:02x}{3:02x}{4:02x}"
    print(tcode, frame)
    return True
try:
    resp = Hinawa.FwResp.new(fw_unit)
except Exception as e:
    print(e)
    sys.exit()
try:
    resp.register(0xfffff0000d00, 0x100, 0)
except Exception as e:
    print(e)
    sys.exit()
try:
    resp.connect('requested', handle_request)
except Exception as e:
    print(e)
    sys.exit()

class Sample(Gtk.Window):

    def __init__(self):
        Gtk.Window.__init__(self, title="Hinawa-1.0 gir sample")
        self.set_border_width(20)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.add(vbox)

        topbox = Gtk.Box(spacing=10)
        vbox.pack_start(topbox, True, True, 0)

        button = Gtk.Button("transact")
        button.connect("clicked", self.on_click_transact)
        topbox.pack_start(button, True, True, 0)

        button = Gtk.Button("_Close", use_underline=True)
        button.connect("clicked", self.on_click_close)
        topbox.pack_start(button, True, True, 0)

        bottombox = Gtk.Box(spacing=10)
        vbox.pack_start(bottombox, True, True, 0)

        self.entry = Gtk.Entry()
        self.entry.set_text("0x00000000")
        bottombox.pack_start(self.entry, True, True, 0)

        self.label = Gtk.Label("result")
        self.label.set_text("0x00000000")
        bottombox.pack_start(self.label, True, True, 0)

    def on_click_transact(self, button):
        try:
            addr = int(self.entry.get_text(), 16)
        except Exception as e:
            print(e)
            return

        try:
            val = req.read(fw_unit, addr, 4)
        except Exception as e:
            print(e)
            return

        template = "0x{0:02x}{1:02x}{2:02x}{3:02x}"
        self.label.set_text(template.format(val[0], val[1], val[2], val[3]))

    def on_click_close(self, button):
        print("Closing application")
        Gtk.main_quit()

def handle_request(req, frame, length):
    print("handle request")
    return

def handle_notification(snd_unit, notification):
    print("%08x".format(notification))

def handle_lock_status(snd_unit, status):
    if status:
        print("streaming is locked.");
    else:
        print("streaming is unlocked.");

def handle_bus_update(fw_unit):
	print(fw_unit.get_property('generation'))

win = Sample()
win.connect("delete-event", Gtk.main_quit)
win.show_all()

snd_unit.connect("lock-status", handle_lock_status)
try:
    snd_unit.listen()
except Exception as e:
    print(e)
    sys.exit()
    
try:
    fw_unit.listen()
except Exception as e:
    print(e)
    sys.exit()
try:
    fcp = Hinawa.FwFcp.new()
except Exception as e:
    print(e)
    sys.exit()

try:
    fcp.listen(fw_unit)
except Exception as e:
    print(e)
    sys.exit()

request = bytearray(8)
request[0] = 0x01;
request[1] = 0xff;
request[2] = 0x19;
request[3] = 0x00;
request[4] = 0xff;
request[5] = 0xff;
request[6] = 0xff;
request[7] = 0xff;
try:
    print(fcp.transact(request))
except Exception as e:
    print(e)
    sys.exit()

# Echo Fireworks Transaction
from array import array
if snd_unit.get_property("iface") is 2:
    try:
        eft = Hinawa.SndEft.new(snd_unit)
    except Exception as e:
        print(e)
        sys.exit()
    args = bytearray(0)
    try:
        vals = eft.transact(3, 1, args)
    except Exception as e:
        print(e)
        sys.exit()

    for i in range(len(vals)):
        print("{0:02x}".format(vals[i]))

    params = array('L')
    if params.itemsize is not 4:
        params = array('I')
    params.frombytes(vals)
    for i in range(len(params)):
        print("{0:08x}".format(params[i]))

def handle_notification(self, message):
    print("notified: {0:08x}".format(message))

if snd_unit.get_property('iface') is 1:
	snd_unit.connect('notified', handle_notification)

Gtk.main()

fw_unit.unlisten()

snd_unit.unlisten()

sys.exit()
