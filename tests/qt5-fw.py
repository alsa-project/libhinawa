#!/usr/bin/env python3

from gi.repository import Hinawa
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QApplication, QWidget

class Sample(QWidget):
    def __init__(self, parent=None):
        super(Sample, self).__init__(parent)

        self.setWindowTitle("Hinawa-1.0 gir sample")
        self.resize(200, 200)

import sys

app = QApplication(sys.argv)
sample = Sample()
sample.show()
app.exec_()
sys.exit()
