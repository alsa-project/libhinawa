=====================
The libhinawa project
=====================

2023/12/14
Takashi Sakamoto

Instruction
===========

I design the library for userspace application to send asynchronous transaction to node in
IEEE 1394 bus and to handle asynchronous transaction initiated by the node. The library is
itself an application of Linux FireWire subsystem,
`GLib and GObject <https://gitlab.gnome.org/GNOME/glib>`_.

The library had originally included some helper object classes for model-specific functions
via ALSA HwDep character device added by drivers in ALSA firewire stack. The object classes have
been already obsoleted and deligated the functions to
`libhitaki <https://github.com/alsa-project/libhitaki>`_, while are still kept for backward
compatibility. They should not be used for applications written newly.

The latest release is `4.0.1 <https://git.kernel.org/pub/scm/libs/ieee1394/libhinawa.git/tag/?h=4.0.1>`_.
The package archive is available in `<https://kernel.org/pub/linux/libs/ieee1394/>`_ with detached
signature created by `my GnuPG key <https://git.kernel.org/pub/scm/docs/kernel/pgpkeys.git/tree/keys/B5A586C7D66FD341.asc>`_.
I note that version 3 release is skipped to use the same major version in library itself and
GObject Introspection (g-i) metadata.

License
=======

- GNU Lesser General Public License version 2.1 or later

Documentation
=============

- `<https://alsa-project.github.io/gobject-introspection-docs/hinawa/>`_

Repository location
===================

- Upstream is `<https://git.kernel.org/pub/scm/libs/ieee1394/libhinawa.git/>`_.
* Mirror at `<https://github.com/alsa-project/libhinawa>`_ for user support and continuous
  integration.

Dependencies
============

- Glib 2.44.0 or later
- GObject Introspection 1.32.1 or later
- Linux kernel 3.12 or later

Requirements to build
=====================

- Meson 0.60.0 or later
- Ninja
- PyGObject (optional to run unit tests)
- gi-docgen 2023.1 or later (optional to generate API documentation)

How to build
============

::

    $ meson setup (--prefix=directory-to-install) build
    $ meson compile -C build
    $ meson install -C build
    ($ meson test -C build)

When working with gobject-introspection, ``Hinawa-4.0.typelib`` should be
installed in your system girepository so that ``libgirepository`` can find
it. Of course, your system LD should find ELF shared object for libhinawa4.
Before installing, it's good to check path of the above and configure
'--prefix' meson option appropriately. The environment variables,
``GI_TYPELIB_PATH`` and ``LD_LIBRARY_PATH`` are available for ad-hoc settings
of the above as well.

How to generate document
========================

::

    $ meson configure (--prefix=directory-to-install) -Ddoc=true build
    $ meson compile -C build
    $ meson install -C build

You can see documentation files under ``(directory-to-install)/share/doc/hinawa/``.

Supplemental information for language bindings
==============================================

* `PyGObject <https://pygobject.readthedocs.io/>`_ is a dynamic loader in Python 3 language for
  libraries compatible with g-i.
* `hinawa-rs <https://git.kernel.org/pub/scm/libs/ieee1394/hinawa-rs.git>`_ includes crates to
  use the library in Rust language.

Sample scripts
==============

Some sample scripts are available under ``samples`` directory:

- gtk3 - PyGObject is required.
- gtk4 - PyGObject is required.
- qt5 - PyQt5 is required.

Example of Python3 with PyGobject
=================================

::

    #!/usr/bin/env python3

    import gi
    gi.require_version('GLib', '2.0')
    gi.require_version('Hinawa', '4.0')
    from gi.repository import GLib, Hinawa

    from threading import Thread
    from struct import unpack

    node = Hinawa.FwNode.new()
    node.open('/dev/fw1')

    ctx = GLib.MainContext.new()
    src = node.create_source()
    src.attach(ctx)

    dispatcher = GLib.MainLoop.new(ctx, False)
    th = Thread(target=lambda d: d.run(), args=(dispatcher, ))
    th.start()

    addr = 0xfffff0000404
    req = Hinawa.FwReq.new()
    frame = [0] * 4
    _, frame = req.transaction(
        node,
        Hinawa.FwTcode.READ_QUADLET_REQUEST,
        addr,
        len(frame),
        frame,
        50
    )
    quad = unpack('>I', frame)[0]
    print('0x{:012x}: 0x{:02x}'.format(addr, quad))

    dispatcher.quit()
    th.join()

How to make DEB package
=======================

- Please refer to `<https://salsa.debian.org/debian/libhinawa>`_.

How to make RPM package
=======================

- Please refer to `<https://build.opensuse.org/package/show/openSUSE:Factory/libhinawa>`_.

Meson subproject
================

This is a sample of wrap file to satisfy dependency on libhinawa by
`Meson subprojects <https://mesonbuild.com/Subprojects.html>`_.

::

    $ cat subprojects/hinawa.wrap
    [wrap-git]
    directory = hinawa
    url = https://git.kernel.org/pub/scm/libs/ieee1394/libhinawa.git
    revision = 4.0.1
    depth = 1
    
    [provide]
    dependency_names = hinawa

After installation of the wrap file, the dependency can be solved by ``hinawa`` name since it is
common in both pkg-config and the wrap file. The implicit or explicit fallback to subproject is
available.

::

    $ cat meson.build
    hinawa_dependency = dependency('hinawa',
      version: '>=4.0.1'
    )

Loss of backward compatibility with version 1 and version 2 releases
====================================================================

In the current version of the library, the focus is on supporting features to operate 1394 OHCI
hardware for asynchronous communication. However, it originally started by supporting features
provided by drivers in ALSA firewire stack.

The version 0 of library supported the GObject class ``Hinawa.FwUnit``, which was derived by
``Hinawa.SndUnit`` class. The ``Hinawa.SndUnit`` class was then inherited by other object classes
for each driver. However, there was an inconvenience where only some parts of asynchronous
transactions (read, write, and lock) were supported by ``Hinawa.SndUnit``.

To address the inconvenience, the version 1 of library integrated ``Hinawa.FwReq`` GObject class
with ``Hinawa.FwTcode`` and ``Hinawa.FwRcode`` GObject enumerations. Nonetheless, another
inconvenience persisted, as some threads were internally launched to dispatch events in Linux
FireWire subsystem and Linux Sound subsystem. These threads, running ``GLib.MainLoop``, were
hidden from the user application.

The version 2 of library aimed to alleviate this issue by providing ``GLib.Source`` to user
applications instead of processing it in the internal threads. The application became responsible
for processing it using ``GLib.MainContext``. Additionally, ``Hinawa.FwNode`` was introduced to
obsolete ``Hinawa.FwUnit`` in an aspect of topology in IEEE 1394 bus. Consequently,
``Hinawa.SndUnit`` directly derived from GObject.

Before releasing the version 4 of library, `libhitaki <https://github.com/alsa-project/libhitaki>`_
was released. The library provides ``Hitaki.SndUnit`` and its derived object classes to obsolete
equivalent features in the version 2 of library. Furthermore, with the release of Linux kernel
version 6.5, new events were introduced to deliver hardware time stamp for asynchronous
communication. To accommodate this, ``Hinawa.CycleTime`` was added, along with some methods of
``Hinawa.FwReq``, ``Hinawa.FwResp``, and ``Hinawa.FwFcp``, to facilitating user application
processing of the hardware time stamp.

The version 4 library is specifically tailored to features in Linux FireWire subsystem, with a
sole focus on asynchronous communication in IEEE 1394 bus. For isochronous communication,
`libhinoko <https://git.kernel.org/pub/scm/libs/ieee1394/libhinoko.git/>`_ provides the
necessary features.

About Hinawa
============

``Hinawa`` is a Japanese term for a match cord used to ignite fires for various purposes. For
example, twine fuses used for igniting gunpowder weapons are a type of ``Hinawa``. The name
``Hinawa`` consists of two kanji characters; ``Hi`` (U+2F55 |kanji-hi|) meaning ``fire``, and
``Nawa`` (U+7E04 |kanji-nawa|) meaning ``cord``.

.. |kanji-hi| unicode:: &#x2f55 .. Hi spelled in Kanji
.. |kanji-nawa| unicode:: &#x7e04 .. Nawa spelled in Kanji

In the IEEE 1394 bus, asynchronous communication is the foundational step for operating functions
within nodes. The ``Hinawa`` library is designed specifically to address this aspect.

end
