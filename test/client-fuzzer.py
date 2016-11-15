#!/usr/bin/python
import os
import socket
import struct
import sys
from collections import OrderedDict

# Upon connecting, first send the process ID and the length of the
# traceback which will be sent.
#
# These are packed into a 32 bit signed integer for the process ID,
# and an unsigned 64 bit integer for the message length. This corresponds to
# the types used to define pid_t and size_t, respectively.
#
# We pack it using '<iQ' as 'L' doesn't do the right thing on LP64.
import select

test_values = OrderedDict()
str_nulled = "properly terminated\x00"
str_nonull = "not properly terminated"
test_values['correct_len'] = [len(str_nulled), str_nulled]
test_values['correct_len_no_nul'] = [len(str_nonull), str_nonull]
test_values['len_too_small'] = [32, ('a' * 32) + '\x00']
test_values['len_too_big'] = [64, ('a' * 32) + '\x00']
test_values['len_too_small_no_nul'] = [32, 'a' * 8192]
test_values['len_too_big_no_nul'] = [8192, ('a' * 32)]


def test_length(sockpath):
    tb_pid = os.getpid()
    for tv in test_values:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
        print('%s (stated length: %d, actual length: %d)' % (tv, test_values[tv][0], len(test_values[tv][1])))
        tb_len = test_values[tv][0]
        packed = struct.pack('<iQ', tb_pid, tb_len)
        s.connect(sockpath)
        s.send(packed)
        s.send(test_values[tv][1])
        s.close()
        select.select([], [], [], 0.1)
        del s


def test_badpid(sockpath):
    tb_pid = -((2 << 30) - 1)
    for tv in test_values:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
        print('%s (stated length: %d, actual length: %d)' % (tv, test_values[tv][0], len(test_values[tv][1])))
        tb_len = test_values[tv][0]
        packed = struct.pack('<iQ', tb_pid, tb_len)
        s.connect(sockpath)
        s.send(packed)
        s.send(test_values[tv][1])
        s.close()
        select.select([], [], [], 0.1)
        del s


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: %s <socket>' % sys.argv[0])
        sys.exit(0)
    test_length(sys.argv[1])
    test_badpid(sys.argv[1])
