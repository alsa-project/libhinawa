from sys import stderr
from pathlib import Path
from struct import unpack
from contextlib import contextmanager
from threading import Thread

import gi
gi.require_versions({'GLib': '2.0', 'Hinawa': '3.0'})
from gi.repository import GLib, Hinawa


def print_help_with_msg(cmd: str, msg: str):
    print('Error:', file=stderr)
    print('  {}'.format(msg), file=stderr)
    print('', file=stderr)
    print('Usage:', file=stderr)
    print('  {} PATH'.format(cmd), file=stderr)
    print('', file=stderr)
    print('  where', file=stderr)
    print('    PATH: path to special file for Linux FireWire character device (/dev/fw[0-9]+)',
          file=stderr)


def detect_fw_cdev(literal: str) -> Path:
    path = Path(literal)

    if not path.exists():
        msg = '"{}" not exists'.format(path)
        raise ValueError(msg)

    if not path.is_char_device():
        msg = '"{}" is not for special file of any character device'.format(path)
        raise ValueError(msg)

    if path.name.find('fw') != 0:
        msg = '"{}" is not for special file of Linux Firewire character device'.format(path)
        raise ValueError(msg)

    return path


def print_generation_information(node: Hinawa.FwNode):
    print('  Topology:')
    print('    self:        {:04x}'.format(node.get_property('node-id')))
    print('    local:       {:04x}'.format(node.get_property('local-node-id')))
    print('    root:        {:04x}'.format(node.get_property('root-node-id')))
    print('    bus-manager: {:04x}'.format(node.get_property('bus-manager-node-id')))
    print('    ir-manager:  {:04x}'.format(node.get_property('ir-manager-node-id')))
    print('    generation:  {}'.format(node.get_property('generation')))


def print_fw_node_information(node: Hinawa.FwNode):
    print('IEEE1394 node info:')

    print_generation_information(node)

    print('  Config ROM:')
    image = node.get_config_rom()
    quads = unpack('>{}I'.format(len(image) // 4), image)
    for i, q in enumerate(quads):
        print('    0xfffff00004{:02x}: 0x{:08x}'.format(i * 4, q))


@contextmanager
def run_dispatcher(node: Hinawa.FwNode):
    ctx = GLib.MainContext.new()
    src = node.create_source()
    src.attach(ctx)

    dispatcher = GLib.MainLoop.new(ctx, False)
    th = Thread(target=lambda d: d.run(), args=(dispatcher,))
    th.start()

    yield

    dispatcher.quit()
    th.join()


def handle_bus_update(node: Hinawa.FwNode):
    print('Event: bus-update:')
    print_generation_information(node)


@contextmanager
def listen_bus_update(node: Hinawa.FwNode):
    handler = node.connect('bus-update', handle_bus_update)
    yield
    node.disconnect(handler)


def print_frame(frame: list):
    for i in range(len(frame)):
        print('  [{:02d}]: 0x{:02x}'.format(i, frame[i]))


def handle_requested2(resp: Hinawa.FwResp, tcode: Hinawa.FwRcode, offset: int,
                      src: int, dst: int, card: int, generation: int, frame: list,
                      length: int):
    print('Event requested2: {0}'.format(tcode.value_nick))
    print_frame(frame)
    return Hinawa.FwRcode.COMPLETE


@contextmanager
def listen_region(node: Hinawa.FwNode):
    resp = Hinawa.FwResp()
    handler = resp.connect('requested2', handle_requested2)
    try:
        resp.reserve(node, 0xfffff0000d00, 0x100)
        yield
    except Exception as e:
        print(e)

    resp.disconnect(handler)
    resp.release()


def handle_responded(fcp: Hinawa.FwFcp, frame: list, length: int):
    print('Event responded: length {}'.format(length))
    print_frame(frame)


@contextmanager
def listen_fcp(node: Hinawa.FwNode):
    fcp = Hinawa.FwFcp()
    handler = fcp.connect('responded', handle_responded)
    try:
        fcp.bind(node)

        request = bytes([0x01, 0xff, 0x19, 0x00, 0xff, 0xff, 0xff, 0xff])
        print('FCP request:')
        print_frame(request)

        response = fcp.avc_transaction(request, [0] * len(request), 100)
        print('FCP response:')
        print_frame(response)

        yield
    except Exception as e:
        print(e)

    fcp.disconnect(handler)
    fcp.unbind()


@contextmanager
def listen_node_event(node: Hinawa.FwNode):
    with run_dispatcher(node), listen_bus_update(node), listen_region(node), listen_fcp(node):
        yield


__all__ = ['print_help_with_msg', 'detect_fw_cdev', 'dump_fw_node_information', 'listen_node_event']
