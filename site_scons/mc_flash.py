import candapter_driver_serial as candapter_driver


def get_eeprom(can, id, address):
    message_read = address + "000000000000"

    msg_resp = "t"
    msg_resp += str(id[0])
    msg_resp += str(id[1])
    msg_resp += str(int(id[2]) + 1)

    # can.flush()
    candapter_driver.send_message(can, id, 8, message_read)

    response = candapter_driver.receive(can)

    while response[:4] != msg_resp:
        # print(response[:4])
        response = candapter_driver.receive(can)
        # if response is None:
        #     print("ERROR getting motor controller response!")
        #     return
        # while response[0:3] != msg_resp:
        #     response = candapter_driver.send_message(can, id, 8, message_read)

            # if response is None:
            #     print("ERROR getting motor controller response!")
            #     return
    print(response)

    print("Parameter set to (hex big-endian): " + response[15] + response[16] +
          response[13] + response[14])


def set_eeprom(can, id, address, data):
    message = address + "0100" + data + "0000"
    message_read = address + "000000000000"
    
    msg_resp = "t"
    msg_resp += str(id[0])
    msg_resp += str(id[1])
    msg_resp += str(int(id[2]) + 1)

    candapter_driver.send_message(can, id, 8, message)

    response = candapter_driver.receive(can)

    while response[:4] != msg_resp:
        # print(response[:4])
        response = candapter_driver.receive(can)

    print(response)

    if response[10] == str(1):
        print("EEPROM write success! Power cycle the car for it to be in effect.")
        return True
    else:
        print("ERROR writing to EEPROM!")
        return False


def main():
    can = candapter_driver.connect()

    if can is None:
        print("No valid CANDapter, Exiting...")
    else:
        candapter_driver.setup(can, 1000)
        get_eeprom(can, "022", "6f00")
        set_eeprom(can, "022", "6f00", "0000")

        # id = input("Motor Controller EEPROM command ID, 3 chars: (hex big-endian, no 0x) ")
        # address = input("EEPROM CAN address, 4 chars: (hex little-endian, no 0x) ")
        # data = input("EEPROM value to flash, 4 chars: (hex, little-endian, no 0x) ")

        # set_eeprom(can, id, address, data)
        candapter_driver.close(can)
