import candapter_driver_serial as candapter_driver


def main():
    can = candapter_driver.connect()

    if can is None:
        print("No valid CANDapter, Exiting...")
    else:
        candapter_driver.setup(can, 500)
        done = False

        while not done:
            usr_in = int(input("\n\nWhat would you like to do?\n" +  
                           "1) Exit\n" +
                           "2) Start Brusa\n"))

            if usr_in == 1:
                done = True
            elif usr_in == 2:
                candapter_driver.send_message(can, 618, 8, "40000000000000")
                print("Brusa charging started!\n")

        candapter_driver.close(can)
