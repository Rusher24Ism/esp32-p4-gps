import os
import struct

# --- CONFIGURATION ---
BIN_DIR = "Map/bin_tiles"
MAX_SIZE_BYTES = 1 * 1024 * 1024  # 1 MB
HEADER_SIZE = 24 # Your current header (not the padded one yet)

def verify_bins():
    total_files = 0
    over_limit_files = []
    total_features = 0
    errors = []

    print(f"--- Starting Verification of {BIN_DIR} ---")

    if not os.path.exists(BIN_DIR):
        print("❌ Error: Directory not found!")
        return

    for root, dirs, files in os.walk(BIN_DIR):
        for file in files:
            if file.endswith(".bin"):
                total_files += 1
                fpath = os.path.join(root, file)
                fsize = os.path.getsize(fpath)

                if fsize > MAX_SIZE_BYTES:
                    over_limit_files.append((fpath, fsize / 1024))

                # Check Binary Integrity
                try:
                    with open(fpath, "rb") as f:
                        while True:
                            header_data = f.read(HEADER_SIZE)
                            if not header_data:
                                break # End of file
                            
                            if len(header_data) < HEADER_SIZE:
                                errors.append(f"{fpath}: Partial header at end of file.")
                                break

                            # Unpack current 22-byte header
                            # f, f, f, f, f, H
                            header = struct.unpack("fffffI", header_data)
                            num_points = header[5]
                            
                            if num_points != 2:
                                errors.append(f"{fpath}: Unexpected numPoints {num_points}. Expected 2.")

                            # Skip the points (First Absolute: 8 bytes, Delta: 4 bytes)
                            # Total data after header for a 2-point segment = 12 bytes
                            points_data_size = 8 + ((num_points - 1) * 4)
                            f.seek(points_data_size, 1) 
                            
                            total_features += 1

                except Exception as e:
                    errors.append(f"{fpath}: Corruption error -> {e}")

    # --- REPORTING ---
    print("\n--- Summary Report ---")
    print(f"Total bin files found: {total_files}")
    print(f"Total segments verified: {total_features}")
    
    if over_limit_files:
        print(f"\n⚠️  Tiles over 1MB: {len(over_limit_files)}")
        for path, size in over_limit_files:
            print(f"  - {path}: {size:.2f} KB")
    else:
        print("\n✅ All tiles are within the 1MB limit.")

    if errors:
        print(f"\n❌ Structural Errors Found: {len(errors)}")
        for err in errors[:10]: # Show first 10 errors
            print(f"  - {err}")
    else:
        print("✅ Binary structure is valid (All headers/points align).")

if __name__ == "__main__":
    verify_bins()