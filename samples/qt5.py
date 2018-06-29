#!/usr/bin/env python3

import sys

# Qt5 python binding
from PyQt5.QtWidgets import QApplication, QWidget, QHBoxLayout, QVBoxLayout
from PyQt5.QtWidgets import QToolButton, QGroupBox, QLineEdit, QLabel

# Hinawa-1.0 gir
import gi
gi.require_version('Hinawa', '1.0')
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
snd_specific_types = {
    Hinawa.SndUnitType.DICE:        Hinawa.SndDice,
    Hinawa.SndUnitType.FIREWORKS:   Hinawa.SndEfw,
    Hinawa.SndUnitType.DIGI00X:     Hinawa.SndDg00x,
    Hinawa.SndUnitType.MOTU:        Hinawa.SndMotu,
}
for fullpath in glob.glob('/dev/snd/hw*'):
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
    sys.exit()

# create sound unit
def handle_lock_status(snd_unit, status):
    if status:
        print("streaming is locked.");
    else:
        print("streaming is unlocked.");
def handle_disconnected(snd_unit):
    print('disconnected')
    app.quit()
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
    sys.exit()
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
    sys.exit()

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
        sys.exit()
    request = bytes([0x01, 0xff, 0x19, 0x00, 0xff, 0xff, 0xff, 0xff])
    try:
        response = fcp.transact(request)
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
if snd_unit.get_property('type') is Hinawa.SndUnitType.DICE:
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
if snd_unit.get_property('type') is Hinawa.SndUnitType.DIGI00X:
    print('hear message')
    snd_unit.connect('message', handle_message)

# Motu notification and status
def handle_motu_notification(self, message):
    print("Motu Notification: {0:08x}".format(message))
if snd_unit.get_property('type') is Hinawa.SndUnitType.MOTU:
    snd_unit.connect('notified', handle_motu_notification)

# GUI
class Sample(QWidget):
    def __init__(self, parent=None):
        super(Sample, self).__init__(parent)

        self.setWindowTitle("Hinawa-1.0 gir sample with PyQt5")

        layout = QVBoxLayout()
        self.setLayout(layout)

        top_grp = QGroupBox(self)
        top_layout = QHBoxLayout()
        top_grp.setLayout(top_layout)
        layout.addWidget(top_grp)

        buttom_grp = QGroupBox(self)
        buttom_layout = QHBoxLayout()
        buttom_grp.setLayout(buttom_layout)
        layout.addWidget(buttom_grp)

        button = QToolButton(top_grp)
        button.setText('transact')
        top_layout.addWidget(button)
        button.clicked.connect(self.transact)

        close = QToolButton(top_grp)
        close.setText('close')
        top_layout.addWidget(close)
        close.clicked.connect(app.quit)

        self.addr = QLineEdit(buttom_grp)
        self.addr.setText('0xfffff0000980')
        buttom_layout.addWidget(self.addr)

        self.value = QLabel(buttom_grp)
        self.value.setText('00000000')
        buttom_layout.addWidget(self.value)

        # handle unix signal
        GLib.unix_signal_add(GLib.PRIORITY_HIGH, signal.SIGINT, \
                             self.handle_unix_signal, None)

    def handle_unix_signal(self, user_data):
        app.quit()

    def transact(self, val):
        try:
            addr = int(self.addr.text(), 16)
            vals = req.read(snd_unit, addr, 4)
        except Exception as e:
            print(e)
            return

        label = '0x{0:02x}{1:02x}{2:02x}{3:02x}'.format(vals[0], vals[1], vals[2], vals[3])
        self.value.setText(label)
        print(self.value.text())

app = QApplication(sys.argv)
sample = Sample()

sample.show()
app.exec()

del app
del sample
print('delete application object')

snd_unit.unlisten()
del snd_unit
print('delete snd_unit object')

resp.unregister()
del resp
print('delete fw_resp object')

del req
print('delete fw_req object')

sys.exit()
