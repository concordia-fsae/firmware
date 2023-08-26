import serial
import serial.tools.list_ports as list_ports


def connect():
    # Tried with and without the last 3 parameters, and also at 1Mbps, same happens.
    device_signature = '0403:6001' 
    candidates = list(list_ports.grep(device_signature))

    print(candidates[0].device)

    ser = serial.Serial(candidates[0].device, 921600, timeout=100000)
    ser.flushInput()
    ser.flushOutput()

    return ser


def close(ser):
    ser.write("C\r\n".encode())

    while receive(ser) != "\x06":
        None


def setup(ser, baud):
    ser.write("V\r\n".encode())
    receive(ser)
    if baud == 1000:
        ser.write("S7\r\n".encode())
        
        while receive(ser) != "\x06":
            None

    ser.write("A1\r\n".encode())
    ser.write("O\r\n".encode())

    while receive(ser) != "\x06":
        None

    print("CAN Channel open")


def send_message(ser, id, length, data, extended=False):
    msg_ID = ""

    if extended:
        msg_ID = "x"
    else:
        msg_ID = "t"

    message = msg_ID
    message += id
    message += str(length)
    message += data
    message += "\r\n"

    ser.write(message.encode())
    
    print("Sending message: " + message)


def receive(ser):
    data_raw = ser.read().decode()
    data = ""

    while data_raw != '\r' and data_raw != '\x06' and data_raw != '\x07':
        data += data_raw
        data_raw = ser.read().decode()

    data += data_raw
    # print(data)

    return data
