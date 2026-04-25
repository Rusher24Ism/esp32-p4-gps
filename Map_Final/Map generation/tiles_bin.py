import osmium
import struct
import os

input_dir = "tiles"
output_dir = "bin_tiles"
os.makedirs(output_dir, exist_ok=True)

class MapHandler(osmium.SimpleHandler):
    def __init__(self, out_file):
        super(MapHandler, self).__init__()
        self.f = open(out_file, "wb")
        # 999.0 is our "Pen Up" signal
        self.sentinel = struct.pack("ff", 999.0, 999.0)

    def way(self, w):
        # Only process ways that have coordinates
        if len(w.nodes) < 2:
            return
            
        for node in w.nodes:
            try:
                # This requires locations=True in apply_file
                self.f.write(struct.pack("ff", node.location.lat, node.location.lon))
            except osmium.InvalidLocationError:
                continue # Skip nodes without valid coords
        
        # End of road/building: lift the pen
        self.f.write(self.sentinel)

    def close(self):
        self.f.close()

for fname in os.listdir(input_dir):
    if fname.endswith(".osm.pbf"):
        in_file = os.path.join(input_dir, fname)
        out_file = os.path.join(output_dir, fname.replace(".osm.pbf", ".bin"))
        print(f"Processing: {fname}")
        
        handler = MapHandler(out_file)
        # CRITICAL: locations=True allows 'way' to see node coordinates
        handler.apply_file(in_file, locations=True)
        handler.close()