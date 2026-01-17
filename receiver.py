import serial
import time
import csv
import sys
import re
import hashlib
import msvcrt   # <-- Windows keyboard input

# =============== CONFIG ===============
SERIAL_RX = 'COM6' # <-- Update this port as your setup
BAUD = 115200
CSV_PATH = r'...\data.csv' # <-- Update this path as your setup

COUNTER_FILE = 'rx_counter.txt'
WINDOW_SIZE = 5
# =====================================


# ---------- Counter handling ----------
def load_counter():
    try:
        with open(COUNTER_FILE, 'r') as f:
            return int(f.read().strip())
    except:
        return 0

def save_counter(c):
    with open(COUNTER_FILE, 'w') as f:
        f.write(str(c))

counter = load_counter()


# ---------- Key generation ----------
def generate_key(counter):
    h = hashlib.sha256(str(counter).encode()).digest()
    return h[0]   # 1 byte key


# ---------- Load CSV ----------
binary_to_sentence = {}

with open(CSV_PATH, 'r') as f:
    reader = csv.reader(f)
    next(reader)
    for row in reader:
        binary_to_sentence[int(row[0], 2)] = row[1].strip()

print(f"[RX] Loaded {len(binary_to_sentence)} sentences")


# ---------- Helpers ----------
def is_valid_hex_byte(s):
    return bool(re.fullmatch(r'[0-9A-Fa-f]{1,2}', s.strip()))


# ---------- Serial ----------
rx = serial.Serial(SERIAL_RX, BAUD, timeout=0.1)
time.sleep(2)

print("[RX] Serial connected")
print("[RX] Waiting for Li-Fi data or user input...")
print("Type RESET and press Enter to reset counter\n")


# ---------- Keyboard buffer ----------
user_buffer = ""


# ---------- MAIN LOOP ----------
while True:

    # -------- 1️⃣ Serial (Li-Fi) --------
    line = rx.readline().decode(errors='ignore').strip()

    if line and is_valid_hex_byte(line):
        received_enc = int(line, 16)
        print(f"\n[RX] Encrypted byte received: 0x{received_enc:02X}")

        found = False

        for offset in range(WINDOW_SIZE):
            test_counter = counter + offset
            key = generate_key(test_counter)
            decrypted = received_enc ^ key

            if decrypted in binary_to_sentence:
                print(f"[RX] Counter matched: {test_counter}")
                print(f"[RX] Key used: 0x{key:02X}")

                print("\n*** MESSAGE RECEIVED ***")
                print(binary_to_sentence[decrypted])
                print("************************\n")

                counter = test_counter + 1
                save_counter(counter)
                found = True
                break

        if not found:
            print("[RX] Decryption failed (counter out of window)\n")

        continue


    # -------- 2️⃣ Keyboard input (Windows safe) --------
    if msvcrt.kbhit():
        ch = msvcrt.getwch()

        if ch in ('\r', '\n'):   # Enter pressed
            cmd = user_buffer.strip().upper()
            user_buffer = ""

            if cmd == "RESET":
                counter = 0
                save_counter(counter)
                print("\n[RX] Counter RESET to 0\n")
            elif cmd:
                print(f"\n[RX] Unknown command: {cmd}\n")

        elif ch == '\b':   # Backspace
            user_buffer = user_buffer[:-1]
        else:
            user_buffer += ch

    time.sleep(0.05)
