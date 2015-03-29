#!/usr/bin/env python3

import sys

# Gtk+3 gir
from gi.repository import Gtk

# Hinawa-1.0 gir
from gi.repository import Hinawa

# to handle UNIX signal
from gi.repository import GLib
import signal

from array import array

import glob

# helper function
def get_array():
    # The width with 'L' parameter is depending on environment.
    arr = array('L')
    if arr.itemsize is not 4:
        arr = array('I')
    return arr

# query sound devices and get FireWire sound unit
for fullpath in glob.glob('/dev/snd/hw*'):
    try:
        snd_unit = Hinawa.SndDice()
        snd_unit.open(fullpath)
    except:
        del snd_unit
        try:
            snd_unit = Hinawa.SndEfw()
            snd_unit.open(fullpath)
        except:
            del snd_unit
            try:
                snd_unit = Hinawa.SndDg00x()
                snd_unit.open(fullpath)
            except:
                del snd_unit
                try:
                    snd_unit = Hinawa.SndUnit()
                    snd_unit.open(fullpath)
                except:
                    del snd_unit
                    continue
    break

if 'snd_unit' not in locals():
    print('No sound FireWire devices found.')
    sys.exit()

# create sound unit
def handle_lock_status(snd_unit, status):
    if status:
        print("streaming is locked.");
    else:
        print("streaming is unlocked.");
def handle_disconnected(snd_unit):
    print('disconnected')
    Gtk.main_quit()
print('Sound device info:')
print(' type:\t{0}'.format(snd_unit.get_property("type")))
print(' card:\t{0}'.format(snd_unit.get_property("card")))
print(' device:\t{0}'.format(snd_unit.get_property("device")))
print(' GUID:\t{0:016x}'.format(snd_unit.get_property("guid")))
snd_unit.connect("lock-status", handle_lock_status)
snd_unit.connect("disconnected", handle_disconnected)

# create FireWire unit
def handle_bus_update(snd_unit):
	print(snd_unit.get_property('generation'))
snd_unit.connect("bus-update", handle_bus_update)

# start listening
try:
    snd_unit.listen()
except Exception as e:
    print(e)
    sys.exit()
print(" listening:\t{0}".format(snd_unit.get_property('listening')))

# create firewire responder
resp = Hinawa.FwResp()
def handle_request(resp, tcode, req_frame):
    print('Requested with tcode {0}:'.format(tcode))
    for i in range(len(req_frame)):
        print(' [{0:02d}]: 0x{1:08x}'.format(i, req_frame[i]))
    # Return no data for the response frame
    return None
try:
    resp.register(snd_unit, 0xfffff0000d00, 0x100)
    resp.connect('requested', handle_request)
except Exception as e:
    print(e)
    sys.exit()

# create firewire requester
req = Hinawa.FwReq()

# Fireworks/BeBoB/OXFW supports FCP and some AV/C commands
if snd_unit.get_property('type') is not 1 and not 5:
    request = bytes([0x01, 0xff, 0x19, 0x00, 0xff, 0xff, 0xff, 0xff])
    try:
        response = snd_unit.fcp_transact(request)
    except Exception as e:
        print(e)
        sys.exit()
    print('FCP Response:')
    for i in range(len(response)):
        print(' [{0:02d}]: 0x{1:02x}'.format(i, response[i]))

# Echo Fireworks Transaction
if snd_unit.get_property("type") is 2:
    args = get_array()
    args.append(5)
    try:
        params = snd_unit.transact(6, 1, args)
    except Exception as e:
        print(e)
        sys.exit()
    print('Echo Fireworks Transaction Response:')
    for i in range(len(params)):
        print(" [{0:02d}]: {1:08x}".format(i, params[i]))

# Dice notification
def handle_notification(self, message):
    print("Dice Notification: {0:08x}".format(message))
if snd_unit.get_property('type') is 1:
    snd_unit.connect('notified', handle_notification)
    args = get_array()
    args.append(0x0000030c)
    try:
        # The address of clock in Impact Twin
        snd_unit.transact(0xffffe0000074, args, 0x00000020)
    except Exception as e:
        print(e)
        sys.exit()

# Dg00x message
def handle_message(self, message):
    print("Dg00x Messaging {0:08x}".format(message));
if snd_unit.get_property('type') is 5:
    print('hear message')
    snd_unit.connect('message', handle_message)

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
        self.entry.set_text("0xfffff0000980")
        bottombox.pack_start(self.entry, True, True, 0)

        self.label = Gtk.Label("result")
        self.label.set_text("0x00000000")
        bottombox.pack_start(self.label, True, True, 0)

        # handle unix signal
        GLib.unix_signal_add(GLib.PRIORITY_HIGH, signal.SIGINT, \
                             self.handle_unix_signal, None)

    def handle_unix_signal(self, user_data):
        self.on_click_close( None)

    def on_click_transact(self, button):
        try:
            addr = int(self.entry.get_text(), 16)
            val = snd_unit.read_transact(addr, 1)
        except Exception as e:
            print(e)
            return

        self.label.set_text('0x{0:08x}'.format(val[0]))
        print(self.label.get_text())

    def on_click_close(self, button):
        Gtk.main_quit()

# Main logic
win = Sample()
win.connect("delete-event", Gtk.main_quit)
win.show_all()

Gtk.main()

del win
print('delete window object')

snd_unit.unlisten()
del snd_unit
print('delete snd_unit object')

resp.unregister()
del resp
print('delete fw_resp object')

del req
print('delete fw_req object')

sys.exit()
