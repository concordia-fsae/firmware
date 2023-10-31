import candapter_driver_serial as candapter_driver


def get_eeprom(can, id, address):
    message_read = address + "000000000000"

    msg_resp = "t"
    msg_resp += str(id[0])
    msg_resp += str(id[1])
    msg_resp += str(int(id[2]) + 1)
    # print(msg_resp)
    
    can.flush()
    candapter_driver.send_message(can, id, 8, message_read)

    response = candapter_driver.receive(can)

    while response[:4] != msg_resp:
        # print(response[:4])
        response = candapter_driver.receive(can)
    
    print("Response: " + response)

    print("Parameter set to (hex big-endian): " + response[15] + response[16] +
          response[13] + response[14])


def set_eeprom(can, id, address, data):
    message = address + "0100" + data + "0000"
    
    msg_resp = "t"
    msg_resp += str(id[0])
    msg_resp += str(id[1])
    msg_resp += str(int(id[2]) + 1)
    print(msg_resp)

    can.flush()
    candapter_driver.send_message(can, id, 8, message)

    response = candapter_driver.receive(can)

    while response[:4] != msg_resp[:4]:
        # print(response[:4])
        response = candapter_driver.receive(can)

    print("Response: " + response)

    if response[10] == str(1):
        print("EEPROM write success! Power cycle the car for it to be in effect.")
        return True
    else:
        print("ERROR writing to EEPROM!")
        return False


def checkHex(usr_in):
    if (usr_in[0] + usr_in[1] == "0x"):
        return True
    return False


def convertToHex(usr_in, big_endian, length=0):
    inp = ""
    output = ""

    if checkHex(usr_in):
        inp = usr_in[2:]
    else:
        inp = hex(int(usr_in))[2:]

    if len(inp) < length:
        for x in range(length-len(inp)):
            inp = "0"+inp

    if not big_endian:
        if len(inp) % 2 == 1:
            inp = "0" + inp
        for x in range(0, len(inp), 2):
            output = output + inp[len(inp) - 1 - x - 1]
            output = output + inp[len(inp) - x - 1]
    else:
        output = inp

    return output


def main():
    can = candapter_driver.connect()

    if can is None:
        print("No valid CANDapter, Exiting...")
    else:
        candapter_driver.setup(can, 1000)
        done = False

        while not done:
            usr_in = int(input("\n\nWhat would you like to do?\n" +  
                           "1) Exit\n" +
                           "2) Get EEPROM data\n" +
                           "3) Set EEPROM data\n"))

            if usr_in == 1:
                done = True
            elif usr_in == 2:
                id = convertToHex(input("Motor Controller EEPROM command ID: "), True, 3)
                address = convertToHex(input("EEPROM CAN address: "), False, 4)
                get_eeprom(can, id, address)
            elif usr_in == 3:
                id = input("Motor Controller EEPROM command ID, 3 chars: (hex little-endian, no 0x) ")
                address = input("EEPROM CAN address, 4 chars: (hex little-endian, no 0x) ")
                data = input("EEPROM value to flash, 4 chars: (hex, little-endian, no 0x) ")
                set_eeprom(can, id, address, data)

        candapter_driver.close(can)
