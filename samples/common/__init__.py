from sys import stderr
from pathlib import Path
from struct import unpack


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


def print_transaction_result(
    addr: int,
    payload: list[int],
    initiate_cycle: list[2],
    sent_cycle: list[2],
    recv_cycle: list[2],
    finish_cycle: list[2],
):
    quadlet = unpack(">I", payload)[0]

    print("Read quadlet transaction:")
    print("  addr 0x{:012x}, quadlet: 0x{:08x}".format(addr, quadlet))
    print(
        "  initiate at:  {} sec {} cycle".format(initiate_cycle[0], initiate_cycle[1])
    )
    print("  sent at:      {} sec {} cycle".format(sent_cycle[0], sent_cycle[1]))
    print("  received at:  {} sec {} cycle".format(recv_cycle[0], recv_cycle[1]))
    print("  finish at:    {} sec {} cycle".format(finish_cycle[0], finish_cycle[1]))
