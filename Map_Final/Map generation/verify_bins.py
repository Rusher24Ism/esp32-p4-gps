import struct
import os

# Make sure this matches your actual folder name
bin_dir = "bin_tiles" 

def verify_bin_file(path):
    valid_points = 0
    separators = 0
    errors = 0
    with open(path, "rb") as f:
        while True:
            data = f.read(8)
            if not data:
                break
            try:
                lat, lon = struct.unpack("ff", data)
                if lat == 999.0:
                    separators += 1
                elif -90 <= lat <= 90 and -180 <= lon <= 180:
                    valid_points += 1
                else:
                    errors += 1
            except:
                errors += 1
    return valid_points, separators, errors

# Check if directory exists
if not os.path.exists(bin_dir):
    print(f"ERROR: Directory '{bin_dir}' not found!")
else:
    files = [f for f in os.listdir(bin_dir) if f.endswith(".bin")]
    print(f"Found {len(files)} .bin files in {bin_dir}\n")
    
    for fname in files:
        path = os.path.join(bin_dir, fname)
        v, s, e = verify_bin_file(path)
        # Only print if the file isn't empty
        if v + s + e > 0:
            print(f"{fname:25} | Valid: {v:5} | Pens-Up: {s:4} | Errors: {e}")