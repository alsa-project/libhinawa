from sys import stderr
from pathlib import Path
from struct import unpack
from contextlib import contextmanager
from threading import Thread

import gi
gi.require_versions({'GLib': '2.0', 'Hinawa': '3.0'})
from gi.repository import GLib, Hinawa

CLOCK_MONOTONIC_RAW = 4


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


def read_quadlet(node: Hinawa.FwNode, req: Hinawa.FwReq, addr: int) -> int:
    cycle_time = Hinawa.CycleTime.new()

    try:
        _, cycle_time = node.read_cycle_time(CLOCK_MONOTONIC_RAW, cycle_time)
    except Exception as e:
        print(e)
        return 0
    initiate_cycle = cycle_time.get_fields()[:2]

    frame = [0] * 4
    try:
        _, frame, tstamp = req.transaction_with_tstamp(
            node,
            Hinawa.FwTcode.READ_QUADLET_REQUEST,
            addr,
            len(frame),
            frame,
            100
        )
    except Exception as e:
        print(e)

    sent_cycle = cycle_time.compute_tstamp(tstamp[0])
    recv_cycle = cycle_time.compute_tstamp(tstamp[1])

    try:
        _, cycle_time = node.read_cycle_time(CLOCK_MONOTONIC_RAW, cycle_time)
    except Exception as e:
        print(e)
        return 0
    finish_cycle = cycle_time.get_fields()[:2]

    quadlet = unpack('>I', frame)[0]

    print('Read quadlet transaction:')
    print('  addr 0x{:012x}, quadlet: 0x{:08x}'.format(addr, quadlet))
    print('  initiate at:  sec {} cycle {}'.format(initiate_cycle[0], initiate_cycle[1]))
    print('  sent at:      sec {} cycle {}'.format(sent_cycle[0], sent_cycle[1]))
    print('  received at:  sec {} cycle {}'.format(recv_cycle[0], recv_cycle[1]))
    print('  finish at:    sec {} cycle {}'.format(finish_cycle[0], finish_cycle[1]))

    return quadlet


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


def handle_requested3(resp: Hinawa.FwResp, tcode: Hinawa.FwRcode, offset: int,
                      src: int, dst: int, card: int, generation: int, tstamp: int,
                      frame: list, length: int, args: tuple[Hinawa.FwNode, Hinawa.CycleTime]):
    node, cycle_time = args
    print('Event requested3: {0}'.format(tcode.value_nick))
    try:
        _, cycle_time = node.read_cycle_time(CLOCK_MONOTONIC_RAW, cycle_time)
        isoc_cycle = cycle_time.compute_tstamp(tstamp)
    except Exception as e:
        print(e)
        isoc_cycle = [0] * 2
        pass
    print_frame(frame)
    print('  received at: sec {0[0]} cycle {0[1]}'.format(isoc_cycle))
    return Hinawa.FwRcode.COMPLETE


@contextmanager
def listen_region(node: Hinawa.FwNode):
    resp = Hinawa.FwResp()
    cycle_time = Hinawa.CycleTime.new()
    handler = resp.connect('requested3', handle_requested3, (node, cycle_time))
    try:
        resp.reserve(node, 0xfffff0000d00, 0x100)
        yield
    except Exception as e:
        print(e)

    resp.disconnect(handler)
    resp.release()


def handle_responded2(fcp: Hinawa.FwFcp, tstamp: int, frame: list, length: int,
                      args: tuple[Hinawa.FwNode, Hinawa.CycleTime]):
    node, cycle_time = args
    print('Event responded2: length {}'.format(length))
    try:
        _, cycle_time = node.read_cycle_time(CLOCK_MONOTONIC_RAW, cycle_time)
        isoc_cycle = cycle_time.compute_tstamp(tstamp)
    except Exception as e:
        print(e)
        isoc_cycle = [0] * 2
        pass
    print_frame(frame)
    print('  received at: sec {0[0]} cycle {0[1]}'.format(isoc_cycle))


@contextmanager
def listen_fcp(node: Hinawa.FwNode):
    cycle_time = Hinawa.CycleTime.new()

    fcp = Hinawa.FwFcp()
    handler = fcp.connect('responded2', handle_responded2, (node, cycle_time))
    try:
        fcp.bind(node)

        _, cycle_time = node.read_cycle_time(CLOCK_MONOTONIC_RAW, cycle_time)
        initiate_cycle = cycle_time.get_fields()[:2]

        request = bytes([0x01, 0xff, 0x19, 0x00, 0xff, 0xff, 0xff, 0xff])
        _, response, tstamp = fcp.avc_transaction_with_tstamp(request, [0] * len(request), 100)

        req_sent_cycle = cycle_time.compute_tstamp(tstamp[0])
        req_responded_cycle = cycle_time.compute_tstamp(tstamp[1])
        resp_sent_cycle = cycle_time.compute_tstamp(tstamp[2])

        _, cycle_time = node.read_cycle_time(CLOCK_MONOTONIC_RAW, cycle_time)
        finish_cycle = cycle_time.get_fields()[:2]

        print('FCP request:')
        print_frame(request)
        print('  initiate at:  sec {0[0]} cycle {0[1]}'.format(initiate_cycle))
        print('  sent at:      sec {0[0]} cycle {0[1]}'.format(req_sent_cycle))
        print('  received at:  sec {0[0]} cycle {0[1]}'.format(req_responded_cycle))

        print('FCP response:')
        print_frame(response)
        print('  received at:  sec {0[0]} cycle {0[1]}'.format(resp_sent_cycle))
        print('  finished at:  sec {0[0]} cycle {0[1]}'.format(finish_cycle))

        yield
    except Exception as e:
        print(e)

    fcp.disconnect(handler)
    fcp.unbind()


@contextmanager
def listen_node_event(node: Hinawa.FwNode, path: Path):
    root = Path.cwd().parents[-1]
    sysfs_path = root.joinpath('sys', 'bus', 'firewire', 'devices', path.name, 'units')

    # Linux FireWire subsystem exports all of pairs of specifier_id and version in unit directory
    # via sysfs, thus not need to parse the content of configuration ROM.
    with sysfs_path.open('r') as f:
        content = f.read()

    # The specifier_id for 1394TA is expected to express the device implements FCP.
    has_fcp = content.find('0x00a02d') >= 0

    if has_fcp:
        with run_dispatcher(node), listen_bus_update(node), listen_region(node), listen_fcp(node):
            yield
    else:
        with run_dispatcher(node), listen_bus_update(node):
            yield


__all__ = ['print_help_with_msg', 'detect_fw_cdev', 'run_async_transaction',
           'dump_fw_node_information', 'listen_node_event']
