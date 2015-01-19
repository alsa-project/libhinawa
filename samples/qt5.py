#!/usr/bin/env python3

import sys

# Qt5 python binding
from PyQt5.QtWidgets import QApplication, QWidget, QHBoxLayout, QVBoxLayout
from PyQt5.QtWidgets import QToolButton, QGroupBox, QLineEdit, QLabel

# Hinawa-1.0 gir
from gi.repository import Hinawa

from array import array

# helper function
def get_array():
    # The width with 'L' parameter is depending on environment.
    arr = array('L')
    if arr.itemsize is not 4:
        arr = array('I')
    return arr

# query sound devices
index = -1
while True:
    try:
        index = Hinawa.UnitQuery.get_sibling(index)
    except Exception as e:
        break
    break

# no fw sound devices are detected.
if index == -1:
    print('No sound FireWire devices found.')
    sys.exit()

# get unit type
try:
    unit_type = Hinawa.UnitQuery.get_unit_type(index)
except Exception as e:
    print(e)
    sys.exit()

# create sound unit
def handle_lock_status(snd_unit, status):
    if status:
        print("streaming is locked.");
    else:
        print("streaming is unlocked.");
try:
    path = "hw:{0}".format(index)
    if unit_type == 1:
        snd_unit = Hinawa.SndDice.new(path)
    elif unit_type == 2:
        snd_unit = Hinawa.SndEfw.new(path)
    else:
        snd_unit = Hinawa.SndUnit.new(path)
except Exception as e:
    print(e)
    sys.exit()
print('Sound device info:')
print(' name:\t{0}'.format(snd_unit.get_property("name")))
print(' type:\t{0}'.format(snd_unit.get_property("type")))
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
fw_unit.connect("bus-update", handle_bus_update)

# start listening
try:
    snd_unit.listen()
    fw_unit.listen()
except Exception as e:
    print(e)
    sys.exit()

# create firewire responder
def handle_request(resp, tcode, frame):
    print('Requested with tcode {0}:'.format(tcode))
    for i in range(len(frame)):
        print(' [{0:02d}]: 0x{1:08x}'.format(i, frame[i]))
    return True
try:
    resp = Hinawa.FwResp.new(fw_unit)
    resp.register(0xfffff0000d00, 0x100)
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
if snd_unit.get_property('type') is not 1:
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
    dice.connect('notified', handle_notification)

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

    def transact(self, val):
        try:
            addr = int(self.addr.text(), 16)
            val = req.read(fw_unit, addr, 1)
        except Exception as e:
            print(e)
            return

        self.value.setText('0x{0:08x}'.format(val[0]))
        print(self.value.text())

app = QApplication(sys.argv)
sample = Sample()

sample.show()
app.exec()

sys.exit()
