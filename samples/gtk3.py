#!/usr/bin/env python3

import sys

# Gtk+3 gir
from gi.repository import Gtk

# Hinawa-1.0 gir
from gi.repository import Hinawa

def get_array():
    # The width with 'L' parameter is depending on environment.
    arr = array('L')
    if arr.itemsize is not 4:
        arr = array('I')
    return arr

# create sound unit
def handle_lock_status(snd_unit, status):
    if status:
        print("streaming is locked.");
    else:
        print("streaming is unlocked.");
try:
    snd_unit = Hinawa.SndUnit.new("hw:0")
except Exception as e:
    print(e)
    sys.exit()
print('Sound device info:')
print(' name:\t{0}'.format(snd_unit.get_property("name")))
print(' iface:\t{0}'.format(snd_unit.get_property("iface")))
print(' card:\t{0}'.format(snd_unit.get_property("card")))
print(' device:\t{0}'.format(snd_unit.get_property("device")))
print(' GUID:\t{0:016x}'.format(snd_unit.get_property("guid")))
snd_unit.connect("lock-status", handle_lock_status)

# create FireWire unit
def handle_bus_update(fw_unit):
	print(fw_unit.get_property('generation'))
path = "/dev/%s" % snd_unit.get_property("device")
try:
    fw_unit = Hinawa.FwUnit.new(path)
except Exception as e:
    print(e)
    sys.exit()
snd_unit.connect("lock-status", handle_lock_status)

# start listening
try:
    snd_unit.listen()
    fw_unit.listen()
except Exception as e:
    print(e)
    sys.exit()

# create firewire responder
def handle_request(resp, tcode, frame, private_data):
    print('Requested with tcode {0}:'.format(tcode))
    for i in range(len(frame)):
        print(' [{0:02d}]: 0x{1:08x}'.format(i, frame[i]))
    return True
try:
    resp = Hinawa.FwResp.new(fw_unit)
    resp.register(0xfffff0000d00, 0x100, 0)
    resp.connect('requested', handle_request)
except Exception as e:
    print(e)
    sys.exit()

# create firewire requester
try:
    req = Hinawa.FwReq.new()
except Exception as e:
    print(e)
    sys.exit()

# Fireworks/BeBoB/OXFW supports FCP and some AV/C commands
if snd_unit.get_property('iface') is not 1:
    request = bytes([0x01, 0xff, 0x19, 0x00, 0xff, 0xff, 0xff, 0xff])
    try:
        fcp = Hinawa.FwFcp.new()
        fcp.listen(fw_unit)
        response = fcp.transact(request)
    except Exception as e:
        print(e)
        sys.exit()
    print('FCP Response:')
    for i in range(len(response)):
        print(' [{0:02d}]: 0x{1:02x}'.format(i, response[i]))
    fcp.unlisten()

# Echo Fireworks Transaction
from array import array
if snd_unit.get_property("iface") is 2:
    args = get_array()
    args.append(5)
    try:
        eft = Hinawa.SndEft.new(snd_unit)
        params = eft.transact(6, 1, args)
    except Exception as e:
        print(e)
        sys.exit()
    print('Echo Fireworks Transaction Response:')
    for i in range(len(params)):
        print(" [{0:02d}]: {1:08x}".format(i, params[i]))

# Dice notification
def handle_notification(self, message):
    print("Dice Notification: {0:08x}".format(message))
if snd_unit.get_property('iface') is 1:
    try:
        dice_notify= Hinawa.SndDiceNotify.new(snd_unit)
    except Exception as e:
        print(e)
        sys.exit()
    dice_notify.connect('notified', handle_notification)

# GUI
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
            val = req.read(fw_unit, addr, 1)
        except Exception as e:
            print(e)
            return

        self.label.set_text('0x{0:08x}'.format(val[0]))
        print(self.label.get_text())

    def on_click_close(self, button):
        print("Closing application")
        Gtk.main_quit()

# Main logic
win = Sample()
win.connect("delete-event", Gtk.main_quit)
win.show_all()

Gtk.main()

sys.exit()
