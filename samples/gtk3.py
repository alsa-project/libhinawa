#!/usr/bin/env python3

from pathlib import Path
from sys import exit
from array import array

import gi
gi.require_version('GLib', '2.0')
from gi.repository import GLib

gi.require_version('Hinawa', '2.0')
from gi.repository import Hinawa

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk

# to handle UNIX signal
import signal

# query sound devices and get FireWire sound unit
snd_specific_types = {
    Hinawa.SndUnitType.DICE:        Hinawa.SndDice,
    Hinawa.SndUnitType.FIREWORKS:   Hinawa.SndEfw,
    Hinawa.SndUnitType.DIGI00X:     Hinawa.SndDg00x,
    Hinawa.SndUnitType.MOTU:        Hinawa.SndMotu,
}
for p in Path('/dev/snd/').glob('hw*'):
    fullpath = str(p)
    try:
        snd_unit = Hinawa.SndUnit()
        snd_unit.open(fullpath)
    except:
        del snd_unit
        continue

    snd_type = snd_unit.get_property('type')
    if snd_type in snd_specific_types:
        del snd_unit
        snd_unit = snd_specific_types[snd_type]()
        try:
            snd_unit.open(fullpath)
        except:
            del snd_unit
            continue
    break

if 'snd_unit' not in locals():
    print('No sound FireWire devices found.')
    exit()

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
print(' type:\t\t{0}'.format(snd_unit.get_property("type").value_nick))
print(' card:\t\t{0}'.format(snd_unit.get_property("card")))
print(' device:\t{0}'.format(snd_unit.get_property("device")))
print(' GUID:\t\t{0:016x}'.format(snd_unit.get_property("guid")))
snd_unit.connect("lock-status", handle_lock_status)
snd_unit.connect("disconnected", handle_disconnected)
print('\nIEEE1394 Unit info:')
print(' Node IDs:')
print('  self:\t\t{0:04x}'.format(snd_unit.get_property('node-id')))
print('  local:\t{0:04x}'.format(snd_unit.get_property('local-node-id')))
print('  root:\t\t{0:04x}'.format(snd_unit.get_property('root-node-id')))
print('  bus-manager:\t{0:04x}'.format(snd_unit.get_property('bus-manager-node-id')))
print('  ir-manager:\t{0:04x}'.format(snd_unit.get_property('ir-manager-node-id')))
print('  generation:\t{0}'.format(snd_unit.get_property('generation')))
print(' Config ROM:')
config_rom = snd_unit.get_config_rom()
for i in range(len(config_rom) // 4):
    print('  [{0:016x}]: {1:02x}{2:02x}{3:02x}{4:02x}'.format(
        0xfffff0000000 + i * 4, config_rom[i * 4], config_rom[i * 4 + 1],
        config_rom[i * 4 + 2], config_rom[i * 4 + 3]))

# create FireWire unit
def handle_bus_update(snd_unit):
    print('bus-reset: generation {0}'.format(snd_unit.get_property('generation')))
snd_unit.connect("bus-update", handle_bus_update)

# start listening
try:
    snd_unit.listen()
except Exception as e:
    print(e)
    exit()
print(" listening:\t{0}".format(snd_unit.get_property('listening')))

# create firewire responder
resp = Hinawa.FwResp()
def handle_request(resp, tcode):
    print('Requested with tcode: {0}'.format(tcode.value_nick))
    req_frame = resp.get_req_frame()
    for i in range(len(req_frame)):
        print(' [{0:02d}]: 0x{1:02x}'.format(i, req_frame[i]))
    return Hinawa.FwRcode.COMPLETE
try:
    resp.register(snd_unit, 0xfffff0000d00, 0x100)
    resp.connect('requested', handle_request)
except Exception as e:
    print(e)
    exit()

# create firewire requester
req = Hinawa.FwReq()

# Fireworks/BeBoB/OXFW supports FCP and some AV/C commands
fcp_types = (
    Hinawa.SndUnitType.FIREWORKS,
    Hinawa.SndUnitType.BEBOB,
    Hinawa.SndUnitType.OXFW,
)
if snd_unit.get_property('type') in fcp_types:
    fcp = Hinawa.FwFcp()
    try:
        fcp.listen(snd_unit)
    except Exception as e:
        print(e)
        exit()
    request = bytes([0x01, 0xff, 0x19, 0x00, 0xff, 0xff, 0xff, 0xff])
    try:
        response = fcp.transact(request)
    except Exception as e:
        print(e)
        exit()
    print('FCP Response:')
    for i in range(len(response)):
        print(' [{0:02d}]: 0x{1:02x}'.format(i, response[i]))
    fcp.unlisten()
    del fcp

# Echo Fireworks Transaction
if snd_unit.get_property("type") is 2:
    args = array('I')
    args.append(5)
    try:
        params = snd_unit.transact(6, 1, args)
    except Exception as e:
        print(e)
        exit()
    print('Echo Fireworks Transaction Response:')
    for i in range(len(params)):
        print(" [{0:02d}]: {1:08x}".format(i, params[i]))

# Dice notification
def handle_notification(self, message):
    print("Dice Notification: {0:08x}".format(message))
if snd_unit.get_property('type') is Hinawa.SndUnitType.DICE:
    snd_unit.connect('notified', handle_notification)
    args = array('I')
    args.append(0x0000030c)
    try:
        # The address of clock in Impact Twin
        snd_unit.transact(0xffffe0000074, args, 0x00000020)
    except Exception as e:
        print(e)
        exit()

# Dg00x message
def handle_message(self, message):
    print("Dg00x Messaging {0:08x}".format(message));
if snd_unit.get_property('type') is Hinawa.SndUnitType.DIGI00X:
    print('hear message')
    snd_unit.connect('message', handle_message)

# Motu notification and status
def handle_motu_notification(self, message):
    print("Motu Notification: {0:08x}".format(message))
if snd_unit.get_property('type') is Hinawa.SndUnitType.MOTU:
    snd_unit.connect('notified', handle_motu_notification)

# GUI
class Sample(Gtk.Window):

    def __init__(self):
        Gtk.Window.__init__(self, title="Hinawa-2.0 gir sample with Gtk+3")
        self.set_border_width(20)

        grid = Gtk.Grid(row_spacing=10, row_homogeneous=True,
                        column_spacing=10, column_homogeneous=True)
        self.add(grid)

        button = Gtk.Button(label="transact")
        button.connect("clicked", self.on_click_transact)
        grid.attach(button, 0, 0, 1, 1)

        button = Gtk.Button(label="_Close", use_underline=True)
        button.connect("clicked", self.on_click_close)
        grid.attach(button, 1, 0, 1, 1)

        self.entry = Gtk.Entry()
        self.entry.set_text("0xfffff0000980")
        grid.attach(self.entry, 0, 1, 1, 1)

        self.label = Gtk.Label(label="result")
        self.label.set_text("0x00000000")
        grid.attach(self.label, 1, 1, 1, 1)

        # handle unix signal
        GLib.unix_signal_add(GLib.PRIORITY_HIGH, signal.SIGINT, \
                             self.handle_unix_signal, None)

    def handle_unix_signal(self, user_data):
        self.on_click_close( None)

    def on_click_transact(self, button):
        try:
            addr = int(self.entry.get_text(), 16)
            vals = req.read(snd_unit, addr, 4);
        except Exception as e:
            print(e)
            return

        label = '0x{0:02x}{1:02x}{2:02x}{3:02x}'.format(vals[0], vals[1], vals[2], vals[3])
        self.label.set_text(label)
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

exit()
