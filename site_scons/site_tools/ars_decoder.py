##
# @file ars_decoder.py
# @brief  Python decoder with SCons and terminal support
# @author Joshua Lafleur (josh.lafleur@outlook.com)
# @version 0.1
# @date 2022-08-27


import importlib

from os.path import isfile
from os import listdir
import os
import glob
import struct
import csv

scons = importlib.find_loader('SCons.Script')
scons_found = scons is not None

if scons_found:
    from SCons.Script import *

bin_folder = '/bin' # @brief Relative location of bin folder from parent data folder
decodedraw_folder = '/decoded_raw' # @brief Relative location of decoded raw folder from parent data folder
decoded_folder = '/decoded' # @brief Relative location of decoded raw folder from parent data folder

##
# @brief  Converts from psi to Pascals
#
# @param psi PSI to convert
#
# @retval   Pressure (Pa)
def psi_to_Pa(psi):
    return psi * 6895

##
# @brief  Calculates the psi of the Differential pressure sensor
#
# @param raw Raw value from the sensor
#
# @retval   Differential pressure in Pa
def diff_press_transfer_func(raw):
    psi =((raw - 1638)/(14745-1638) * 30) #https://www.farnell.com/datasheets/2918116.pdf pg2 
    return psi_to_Pa(psi)

##
# @brief Calculates temperature of the sensor 
#
# @param raw Raw value from the sensor
#
# @retval   Returns temperature in degree Celsius
def temp_transfer_func(raw): #Assumes 12bit temp
    return ((raw * 200) / pow(2, 11)) - 50 #https://www.farnell.com/datasheets/2918116.pdf pg4

##
# @brief  Calculates absolute pressure of sensor
#
# @param raw Raw value from sensor
#
# @retval   Returns pressure of sensor (Pa)
def press_transfer_func(raw):
    psi = ((raw - 0.1 * pow(2, 24))*25)/(0.8 * pow(2, 24)) #https://www.mouser.ca/datasheet/2/187/honeywell_sensing_micropressure_board_mount_pressu-2448717.pdf pg7,19 
    return psi_to_Pa(psi)

##
# @brief  Decode's a logged file
#
# @param raw_file Location of raw binary file
# @param decodedraw_file Location of decoded raw values in csv
# @param decoded_file Location of file with values in standard units
# @param file_header Header of csv
# @param structure_fmt Structure of data
#
# @retval   None
def decode_file(raw_file, decodedraw_file, decoded_file, file_header, structure_fmt):
    struct_len = struct.calcsize(structure_fmt)
    struct_unpack = struct.Struct(structure_fmt).unpack_from
    raw_headers = file_header.split(', ')

    raw_results = []
    with open(raw_file, "rb") as f:
        if not os.stat(raw_file).st_size % struct_len == 0:
            print("Warning: File size is not a multiple of struct length")
        while True:
            data = f.read(struct_len)
            if not data: break
            s = struct_unpack(data)
            raw_results.append(s)

    headers = []
    for t in raw_headers:
        if any(i.isdigit() for i in t):
            for i in range(int(t[4])):
                tmp = list(t)
                tmp[4] = str(i)
                headers.append("".join(tmp))
        else:
            headers.append(t)
    
    if len(headers) != len(raw_results[0]):
        print("Header length and data sample length do not match")
        return

    with open(decodedraw_file, 'w') as f:
        write = csv.writer(f)
        write.writerow(headers)
        write.writerows(raw_results)

    results = []
    for r in raw_results:
        res = []
        for i in range(len(raw_results[0])):
            if headers[i] == "timestamp":
                res.append(r[i])
            elif headers[i] == "diff_press":
                res.append(diff_press_transfer_func(r[i]))
            elif headers[i] == "temp":
                res.append(temp_transfer_func(r[i]))
            elif "top" in headers[i] or "bot" in headers[i]:
                res.append(press_transfer_func(r[i]))
        results.append(res)

    with open(decoded_file, 'w') as f:
        write = csv.writer(f)
        write.writerow(headers)
        write.writerows(results)

##
# @brief  Docode's a data folder
#
# @param dir Directory of the parent data folder
#
# @retval   None
def decode_folder(dir):
    bin_dir = dir + bin_folder + '/'
    decodedraw_dir = dir + decodedraw_folder + '/'
    decoded_dir = dir + decoded_folder + '/'
    test_batch_name = ""

    config_file = open(bin_dir + "config.txt")
    file_header = config_file.readline()
    structure_fmt = config_file.readline()

    logs = [os.path.basename(f) for f in glob.glob(bin_dir + "*.bin")]
    print("In folder '" + os.path.basename(dir) + "', there are these files to decode:")
    for f in logs:
        print(f)

    answer = input("Use '" + os.path.basename(dir) + "' as the batch name? (y/n)\nThis will title each decoded file as: '" + os.path.basename(dir) + "*'")

    if answer == "y":
        test_batch_name = os.path.basename(dir)
    else:
        test_batch_name = input("What is the batch name? (Hint: use the date and time of the first run... ie: 13-02-2022-1455)")

    #os.mkdir(decodedraw_dir)
    #os.mkdir(decoded_dir)

    for f in logs:
        decode_file(bin_dir + f, decodedraw_dir + test_batch_name + f[0] + '.csv', decoded_dir + test_batch_name + f[0] + '.csv', file_header, structure_fmt)

##
# @brief  Called by the build system to start decoding
# @note Not to be called directly
#
# @param env Environment to be worked on
#
# @retval   None
def _scons_decode(env = None):
    if not "decode" in COMMAND_LINE_TARGETS:
        return
    _decode(env)

def _decode(env = None):
    subfolders = []
    folder_path = ""
    folders_to_decode = []

    if scons_found:
        REPO_ROOT_DIR = env["REPO_ROOT_DIR"]
        folder_path = REPO_ROOT_DIR.Dir("components/aero_sensors/data")
        subfolders = [ f.path for f in os.scandir(folder_path.get_abspath()) if f.is_dir() ]
    else:
        folder_path = input("What is the absolute path of the data folder?\n")
        subfolders = [ f.path for f in os.scandir(folder_path) if f.is_dir() ]
    
    for f in subfolders:
        if os.path.isdir(f + bin_folder):
            #if not os.path.isdir(f + decodedraw_folder) and not os.path.isdir(f + decoded_folder):
                if len(os.listdir(f + bin_folder)) != 0 and "config.txt" in os.listdir(f + bin_folder) :
                    folders_to_decode.append(f)

    print("\nFound these folders with data to decode:")
    for f in folders_to_decode:
        print(f) 
   
    print()

    for f in folders_to_decode:
        decode_folder(f)

    

##
# @brief  Starts decoder if executed from terminal
if not scons_found:
    _decode()

##
# @brief  Generate doxygen support for SCons
#
# @param env Environment to be worked on
#
# @retval   None
def generate(env):
    if scons_found:
        env.AddMethod(_scons_decode, "decode")

##
# @brief  Shows module exists
#
# @retval   True
def exists():
    return True