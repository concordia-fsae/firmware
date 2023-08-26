import usb.core
import usb.backend.libusb1

VENDOR_ID = 0x0403
PRODUCT_ID = 0x6001


def connect():
    device = usb.core.find(idVendor=VENDOR_ID, idProduct=PRODUCT_ID)

    if device is None:
        print("No device found...")
        return None

    i = device[0].interfaces()[0].bInterfaceNumber

    # Claim interface 0 - this interface provides IN and OUT endpoints to write
    # to and read from
    if device.is_kernel_driver_active(i):
        try:
            device.detach_kernel_driver(i)
        except usb.core.USBError as e:
            print("Could not detatch kernel driver from interface({0}): {1}"
                  .format(i, str(e)))

    usb.util.claim_interface(device, 0)
    return device


def disconnect(device):
    usb.util.release_interface(device, 0)
    # device.close()


def send_message(device, id, length, data, extended=False):
    msg_ID = ""

    if extended:
        msg_ID = "X"
    else:
        msg_ID = "T"

    message = msg_ID
    message += id
    message += str(length)
    message += data

    _send_to_candapter(device, message)


def setup(device, speed):
    if speed == 1000:
        _send_to_candapter(device, "S7")

    response = receive_message(device)[0]

    if response == chr(0x06):
        print("CANDapter set to ", speed)
    elif response == chr(0x07):
        print("CANDapter set to ", speed, " ERROR!")

    _send_to_candapter(device, "O\n")

    response = receive_message(device)[0]

    if response == chr(0x06):
        print("CANDapter channel open!")
    elif response == chr(0x07):
        print("CANDapter channel open ERROR!")


def close(device):
    _send_to_candapter(device, "C\n")

    response = receive_message(device)[0]

    if response == chr(0x06):
        print("CANDapter channel closed!")
    elif response == chr(0x07):
        print("CANDapter channel close ERROR!")


def _send_to_candapter(device, msg_str):
    byte_str = str(msg_str) + '\n'

    num_bytes_written = 0

    try:
        # 0x01 is the OUT endpoint
        num_bytes_written = device.write(0x02, byte_str)
    except usb.core.USBError as e:
        print(e.args)

    return num_bytes_written


def receive_message(device, timeout=10000):
    try:
        # try to read a maximum of 64 bytes from 0x81 (IN endpoint)
        data = device.read(0x81, 64, timeout)
    except usb.core.USBError as e:
        print("Error reading response: {}".format(e.args))
        return None

    # construct a string out of the read values, starting from the 2nd byte
    byte_str = ''.join(chr(n) for n in data[2:])
    # .split('\x00', 1)[0]  # remove the trailing null '\x00' characters
    result_str = byte_str
    if len(result_str) == 0:
        return None

    return result_str


def exists():
    return True
