import importlib

from os.path import isfile
from os import listdir
import os

scons = importlib.find_loader('SCons.Script')
scons_found = scons is not None

if scons_found:
    from SCons.Script import *

bin_folder = '/bin'
decodedraw_folder = '/decoded_raw'
decoded_folder = '/decoded'

def decode_file(raw_file, decodedraw_file, decoded_file):
    print(raw_file)
    print(decodedraw_file)
    print(decoded_file)
    return

def decode_folder(dir):
    bin_dir = dir + bin_folder + '/'
    decodedraw_dir = dir + decodedraw_folder + '/'
    decoded_dir = dir + decoded_folder + '/'
    test_batch_name = ""

    logs = [f for f in listdir(bin_dir) if isfile(bin_dir + f)]
   
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
        decode_file(bin_dir + f, decodedraw_dir + test_batch_name + f[0], decoded_dir + test_batch_name + f[0])

    return

def _decode(env = None):
    if not "decode" in COMMAND_LINE_TARGETS:
        return

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
            if not os.path.isdir(f + decodedraw_folder) and not os.path.isdir(f + decoded_folder):
                if len(os.listdir(f + bin_folder)) != 0:
                    folders_to_decode.append(f)

    print("\nFound these folders with data to decode:")
    for f in folders_to_decode:
        print(f) 
   
    print()

    for f in folders_to_decode:
        decode_folder(f)

    

if not scons_found:
    _decode()

def generate(env):
    if scons_found:
        env.AddMethod(_decode, "decode")
    #env["BUILDERS"]["decode"] = SCons.Builder.Builder(
    #   action = _decode 
    #)
    return

def exists():
    return True
