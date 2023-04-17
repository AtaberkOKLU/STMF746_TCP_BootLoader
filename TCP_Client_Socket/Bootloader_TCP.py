from intelhex import IntelHex
import socket
import time
import hashlib

# CONFIGURATIONS
_BUFFER_SIZE    = 512
_BOOT_ID        = b'\x00'
HOST = "192.168.1.10"  # The server's hostname or IP address
PORT = 80  # The port used by the server

# IntelHex Object Initiation
intelHex = IntelHex()

# HEX FILE SELECTION
_file_name = str(input("HEX File Name to be uploaded:"))

# Get Data From HEX File
intelHex.fromfile(_file_name + ".hex", format='hex')  # Read Hex File
hex_dict = intelHex.todict()  # Dump into DICT Object

hex_byte_list = list(hex_dict.values())  # Convert to list
hex_byte_list.pop()  # POP: {'EIP': 134218121}
# print("FILE-Decimal Bytes:", hex_byte_list)  # All bytes in decimal form
md5_hex = hashlib.md5(bytearray(hex_byte_list)).hexdigest()
md5_bytelist = bytearray.fromhex(md5_hex)

hex_chunk_list = [hex_byte_list[i: i + _BUFFER_SIZE]  # Creating new list: Chunk List
                  for i in range(0, len(hex_byte_list), _BUFFER_SIZE)]  # Each containing specified many

# print("\nChunks:", hex_chunk_list)  # See Chunks
print('\n1st Chunk (HEX): [{}]'.format(', '  # See First Chunk in HEX
                                       .join(hex(x) for x in hex_chunk_list[0][0:15])))  # HEX Conversion (CHECKING)
print('\nLast Chunk (HEX): [{}]'.format(', '  # See First Chunk in HEX
                                        .join(hex(x) for x in hex_chunk_list[-1][-16:])))  # HEX Conversion (CHECKING)

# Some Other Information
print("\nGeneral Code Information:")
print("Total # of Bytes:\t\t", len(hex_byte_list))
print("Buffer Size:\t\t\t", _BUFFER_SIZE)
print("Total # of Chunks:\t\t", len(hex_chunk_list))
print("# of Last Chunk bytes:\t", len(hex_chunk_list[-1]))

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))

print("Send BootID:", _BOOT_ID)
s.sendall(_BOOT_ID)
# time.sleep(100 / 1000.0)  # Sleep for 100 ms?
for i in range(len(hex_chunk_list)):

    if i == len(hex_chunk_list) - 1:
        hex_chunk_list[i].insert(0, 1)  # Last Package
    else:
        hex_chunk_list[i].insert(0, 0)  # Not Last Package

    s.sendall(bytearray(hex_chunk_list[i]))
    time.sleep(100 / 1000.0)  # Sleep for 100 ms?
    print("Sent Package -", i)
    if i == len(hex_chunk_list) - 1:
        print("Send MD5 Checksum:", md5_hex)
        s.sendall(md5_bytelist)  # Send Calculated Checksum


s.shutdown(socket.SHUT_WR)
# s.close()
