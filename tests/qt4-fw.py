#!/usr/bin/env python2

import sys

# Hinawa-1.0 gir
from gi.repository import Hinawa

# Qt4 python binding
from PyQt4.QtCore import Qt
from PyQt4.QtGui import QApplication, QWidget

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

class Sample(QWidget):
    def __init__(self, parent=None):
        super(Sample, self).__init__(parent)

        self.setWindowTitle("Hinawa-1.0 gir sample")
        self.resize(200, 200)

def handle_request(self):
	print("handle request")

def handle_lock_status(self):
	print("change lock status")

app = QApplication(sys.argv)
sample = Sample()

snd_unit.connect("lock-status", handle_lock_status)
snd_unit.listen()

fw_unit.listen()
resp.connect("request", handle_request)
resp.register(0xfffff0000d00, 256)

sample.show()
app.exec_()

resp.unregister()
fw_unit.unlisten()

snd_unit.unlisten()

sys.exit()
