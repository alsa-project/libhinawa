=========
libhinawa
=========

2021/11/17
Takashi Sakamoto

Instruction
===========

I design this library to send asynchronous transaction to units in
IEEE 1394 bus from Linux userspace applications, written by any language
binding supporting GObject Introspection. According to this design, the
library is an application of Linux FireWire subsystem and GLib/GObject.

Additionally, my recent work since 2013 for Linux sound subsystem, a.k.a
ALSA, adds some loadable modules into Linux kernel as drivers for some
Audio and Music units in IEEE 1394 bus. They allow userspace applications
to transfer PCM frames and MIDI messages via ALSA PCM, RawMidi, and
Sequencer interfaces. The modules also supports ALSA HwDep interface for
model-specific functions such as notification. The library includes some
helper objects for the model-specific functions. According to this design,
a part of this library is an application of Linux sound subsystem.

Example of Python3 with PyGobject
=================================

::

    #!/usr/bin/env python3

    from threading import Thread

    import gi
    gi.require_version('GLib', '2.0')
    gi.require_version('Hinawa', '2.0')
    from gi.repository import GLib, Hinawa

    unit = Hinawa.SndUnit()
    unit.open('/dev/snd/hwC0D0')
    node = unit.get_node()

    ctx = GLib.MainContext.new()
    unit_src = unit.create_source()
    node_src = node.create_source()
    unit_src.attach(ctx)
    node_src.attach(ctx)
    dispatcher = GLib.MainLoop.new(ctx, False)
    dispatch_th = Thread(target=lambda d: d.run(), args=(dispatcher,))
    dispatch_th.start()

    addr = 0xfffff0000904
    req = Hinawa.FwReq()
    frame = bytearray(4)
    frame = req.transaction_sync(node, Hinawa.FwTcode.READ_QUADLET_REQUEST, addr, 4,
                                 frame, 50)
    for i, frame in enumerate(frame):
        print('0x{:016x}: 0x{:02x}'.format(addr + i, frame))

    dispatcher.quit()
    dispatcher_th.join()

License
=======

- GNU Lesser General Public License version 2.1 or later

Documentation
=============

- https://alsa-project.github.io/libhinawa-docs/

Dependencies
============

- Glib 2.34.0 or later
- GObject Introspection 1.32.1 or later
- Linux kernel 3.12 or later

Requirements to build
=====================

- Meson 0.46.0 or later
- Ninja
- PyGObject (optional to run unit tests)
- GTK-Doc 1.18-2 (optional to generate API documentation)

How to build
============

::

    $ meson . build
    $ cd build
    $ ninja
    $ ninja install
    ($ ninja test)

When working with gobject-introspection, ``Hinawa-3.0.typelib`` should be
installed in your system girepository so that ``libgirepository`` can find
it. Of course, your system LD should find ELF shared object for libhinawa2.
Before installing, it's good to check path of the above and configure
'--prefix' meson option appropriately. The environment variables,
``GI_TYPELIB_PATH`` and ``LD_LIBRARY_PATH`` are available for ad-hoc settings
of the above as well.

How to generate document
========================

::

    $ meson -Ddoc=true . build
    $ cd build
    $ ninja
    $ ninja install

Sample scripts
==============

::

    $ ./samples/run.sh [gtk|qt5]

- gtk - PyGObject is required.
- qt5 - PyQt5 is required.

How to make DEB package
=======================

- Please refer to https://salsa.debian.org/debian/libhinawa.

How to make RPM package
=======================

1. Satisfy build dependencies

::

    $ dns install meson glib2-devel gobject-introspection-devel gtk-doc

2. make archive

::

    $ meson . build
    $ cd build
    $ meson dist
    ...
    meson-dist/libhinawa-2.4.0.tar.xz 3bc5833e102f38d3b08de89e6355deb83dffb81fb6cc34fc7f2fc473be5b4c47
    $ cd ..

3. copy the archive

::

    $ cp build/meson-dist/libhinawa-2.4.0.tar.xz ~/rpmbuild/SOURCES/

4. build package

::

    $ rpmbuild -bb libhinawa.spec

Lose of backward compatibility from v1 release.
===============================================

- HinawaFwUnit

  - This gobject class is dropped. Instead, HinawaFwNode should be used
    to communicate to the node on IEEE 1394 bus.

- HinawaFwReq/HinawaFwResp/HinawaFwFcp

  - Any API with arguments for HinawaFwUnit is dropped. Instead, use APIs
    with arguments for HinawaFwNode.
  - Any API with arguments for GByteArray is dropped. Instead, use APIs with
    arguments for guint8(buffer) and gsize(buffer length).

- HinawaSndEfw/HinawaSndDice

  - Any API with arguments for GArray is dropped. Instead, use APIs with
    arguments for guint32(buffer) and gsize(buffer length).

- I/O thread

  - No thread is launched internally for event dispatcher. Instead, retrieve
    GSource from HinawaFwNode and HinawaSndUnit and use it with GMainContext
    for event dispatcher. When no dispatcher runs, timeout occurs for any
    transaction.

- Notifier thread

  - No thread is launched internally for GObject signal notifier. Instead,
    implement another thread for your notifier by your own and delegate any
    transaction into it. This is required to prevent I/O thread to be stalled
    because of waiting for an additional event of the transaction.

end
