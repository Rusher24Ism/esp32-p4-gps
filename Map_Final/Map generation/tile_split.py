import os
import subprocess

# Input and output paths
input_file = "bangladesh_filtered.osm.pbf"
output_dir = "tiles"
os.makedirs(output_dir, exist_ok=True)

# Bangladesh bounding box
min_lon, min_lat = 88.0, 20.0
max_lon, max_lat = 93.0, 27.0

# Tile size (degrees)
tile_size = 0.25

lat = min_lat
while lat < max_lat:
    lon = min_lon
    while lon < max_lon:
        bbox = f"{lon},{lat},{lon+tile_size},{lat+tile_size}"
        out_file = f"{output_dir}/tile_{int(lat*100)}_{int(lon*100)}.osm.pbf"
        print(f"Extracting {bbox} -> {out_file}")
        subprocess.run([
            "osmium", "extract",
            "--bbox", bbox,
            input_file,
            "-o", out_file
        ])
        lon += tile_size
    lat += tile_size
