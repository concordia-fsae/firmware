import usb.core
import usb.backend.libusb1

VENDOR_ID = 0x0403
PRODUCT_ID = 0x6001

def connect():
    device = usb.core.find(idVendor=VENDOR_ID, idProduct=PRODUCT_ID)
    
    if device is None:
        print("No device found...")
    # Claim interface 0 - this interface provides IN and OUT endpoints to write to and read from
    usb.util.claim_interface(device, 0)
    return device

def disconnect(device):
    usb.util.release_interface(device, 0)
    device.close() 
