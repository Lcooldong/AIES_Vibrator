import ctypes
import sys
import time
from ctypes import *
from enum import Enum
import serial
import serial.tools.list_ports
import random

sequence_number = 0
magic_number = random.randint(0, 255)


class Packet(Structure):

    _pack = 1  # 1byte 정렬

    _fields_ = [("STX", c_uint8),
                ("seq_num", c_uint8),
                ("device_id", c_uint8),
                ("state", c_uint8),
                ("magic", c_uint8),
                ("checksum", c_uint8),
                ("payload", c_uint8),
                ("ETX", c_uint8)]


def read_packet_data(fields):
    print("------------------packet------------------------")
    print(f"Start_Sign : 0x{fields.STX}\n" +
          f"sequence : {fields.seq_num}\n" +
          f"device_id : {fields.device_id}\n" +
          f"state : {fields.state}\n" +
          f"magic : {fields.magic}\n" +
          f"checksum : {fields.checksum}\n" +
          f"payload : {fields.payload}\n" +
          f"End_Sign : 0x{fields.ETX}")
    print("------------------------------------------------")
    print(f"bytes : {bytes(fields)}")


def set_packet(device_id, state, payload):
    global sequence_number
    global magic_number
    sequence_number = sequence_number + 1
    # c_array = (c_uint8 * len(rgb_list))(*rgb_list)
    data = Packet(0x02, sequence_number, device_id, state, magic_number, 0, payload, 0x03)
    read_packet_data(data)
    return data


def get_payload():
    packet = Packet(0x0, 0, 0, 0, magic_number, 0, 0, 0x0)
    return sys.getsizeof(packet)


# 세팅 끝 알려주는 메서드
def readUntilString(ser, exitcode=b'Setup_Done'):
    count = 0
    while True:
        data = ser.read_until()
        print(data)
        # print(count)
        if data == b'':
            count = count + 1
        else:
            count = 0

        if exitcode in data or count > 50:
            return print("====Serial Now available====")


def clear_serial_buffer(ser, delay):
    close_time = time.time() + delay
    while True:
        # if py_serial.readable():
        res = ser.readline()
        # print(res[:len(res)-1].decode('utf-8').rstrip()
        print(res[:len(res) - 1].decode().rstrip())

        if time.time() > close_time:
            break


def serial_ports(com_port=None):
    ports = serial.tools.list_ports.comports()
    uart_port = ['CP210x', 'CH340', 'CH340K', 'CH9102']
    dic = {}

    if com_port is not None:
        dic[com_port] = "manually"
        return dic

    for port, desc, hwid in sorted(ports):
        # print("{}: {} [{}]".format(port, desc, hwid))
        for uart in uart_port:
            if uart in desc:
                # print(uart)
                dic[port] = uart
                # print(dic)

    if len(dic.items()) > 0:
        return dic


def connect_port(com_port=None):
    connected_ports = serial_ports(com_port)
    board_port = list(connected_ports.keys())
    # print(board_port[0])
    return board_port[0]


def serial_receive_callback(ser, data):
    recv_data = ser.read(data)
    recv_data = Packet.from_buffer_copy(recv_data)
    read_packet_data(recv_data)
   # return recv_data


if __name__ == '__main__':
    print(serial_ports())
    # py_serial = serial.Serial(port="COM8", baudrate=115200, timeout=0.1)
    py_serial = serial.Serial(port=connect_port('COM7'), baudrate=115200, timeout=0.1)
    # py_serial = serial.Serial(port=connect_port(), baudrate=115200, timeout=0.1)    # 포트 연결

    readUntilString(py_serial)

    clear_serial_buffer(py_serial, 1)

    packet_payload = get_payload()

    # global sequence_number
    # global magic_number
    while True:
        # trans = set_packet(0x10, 3, 120)
        sequence_number = sequence_number + 1
        data = Packet(0x02, sequence_number, 0x10, 1, magic_number, 0, packet_payload, 0x03)
        send_data = py_serial.write(bytes(data))
        time.sleep(0.001)

        receive_data = py_serial.read(send_data)
        if receive_data > 0:
            print_data = Packet.from_buffer_copy(receive_data)
            read_packet_data(print_data)
        time.sleep(1)
        # data = Packet(0x02, sequence_number, 0x10, 2, magic_number, 0, packet_payload, 0x03)
        # send_data = py_serial.write(bytes(data))
        # time.sleep(1)
        # data = Packet(0x02, sequence_number, 0x10, 3, magic_number, 0, packet_payload, 0x03)
        # send_data = py_serial.write(bytes(data))
        # time.sleep(1)


    py_serial.close()
