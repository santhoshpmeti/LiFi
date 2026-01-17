import serial
import time
import csv
import sys
import hashlib

from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.metrics.pairwise import cosine_similarity

# =============== CONFIG ===============
SERIAL_TX = 'COM4' # <-- Update this port as your setup
BAUD = 115200
CSV_PATH = r'...\data.csv' # <-- Update this path as your setup
SIMILARITY_THRESHOLD = 0.2

COUNTER_FILE = 'tx_counter.txt'
WINDOW_DELAY = 0.05
# =====================================


# -------- Counter handling --------
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


# -------- Key generation --------
def generate_key(counter):
    h = hashlib.sha256(str(counter).encode()).digest()
    return h[0]   # 1 byte key


# -------- Load CSV --------
sentences = []
binary_map = {}

try:
    with open(CSV_PATH, 'r') as f:
        reader = csv.reader(f)
        next(reader)
        for row in reader:
            binary = int(row[0], 2)
            sentence = row[1].strip()
            sentences.append(sentence)
            binary_map[sentence] = binary
except Exception as e:
    print(f"[TX] CSV load failed: {e}")
    sys.exit(1)

print(f"[TX] Loaded {len(sentences)} sentences")


# -------- TF-IDF --------
vectorizer = TfidfVectorizer()
tfidf_matrix = vectorizer.fit_transform(sentences)

def find_best_match(text):
    qv = vectorizer.transform([text])
    sims = cosine_similarity(qv, tfidf_matrix).flatten()
    idx = sims.argmax()
    if sims[idx] < SIMILARITY_THRESHOLD:
        return None, None, sims[idx]
    return sentences[idx], binary_map[sentences[idx]], sims[idx]


# -------- Serial --------
try:
    tx = serial.Serial(SERIAL_TX, BAUD, timeout=2)
except serial.SerialException as e:
    print(f"[TX] Serial error: {e}")
    sys.exit(1)

time.sleep(2)
print("[TX] Serial connected")


# -------- MAIN LOOP --------
while True:
    user_input = input("\nEnter message (or RESET): ").strip()

    # ----- Manual counter reset -----
    if user_input.upper() == "RESET":
        counter = 0
        save_counter(counter)
        print("[TX] Counter reset to 0")
        continue   # ✅ NOW CORRECT (inside loop)

    sentence, binary, score = find_best_match(user_input)

    if sentence is None:
        print("[TX] No close match found")
        continue   # ✅ INSIDE LOOP

    key = generate_key(counter)
    encrypted = binary ^ key

    print(f"[TX] Matched: \"{sentence}\"")
    print(f"[TX] Similarity: {score:.3f}")
    print(f"[TX] Counter: {counter}")
    print(f"[TX] Key: 0x{key:02X}")
    print(f"[TX] Encrypted byte sent: 0x{encrypted:02X}")

    tx.write(bytes([encrypted]))
    time.sleep(WINDOW_DELAY)

    counter += 1
    save_counter(counter)
