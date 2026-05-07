import osmium
import struct
import math
import os
import shutil

# --- CONFIGURATION ---
INPUT_PBF = "bangladesh-260423.osm.pbf"
OUTPUT_DIR = "Map/bin_tiles"
GRID_SIZE = 0.03125  

class MapProcessor(osmium.SimpleHandler):
    def __init__(self):
        super(MapProcessor, self).__init__()
        self.files = {}
        self.count = 0

    def get_file(self, lat, lon):
        glat = int(math.floor(lat / GRID_SIZE))
        glon = int(math.floor(lon / GRID_SIZE))
        path = f"{OUTPUT_DIR}/{glat}"
        os.makedirs(path, exist_ok=True)
        fpath = f"{path}/tile_{glon}.bin"
        
        if fpath not in self.files:
            self.files[fpath] = open(fpath, "ab")
        return self.files[fpath]

    def write_to_tile(self, f_type, p1, p2):
        """Helper to write a 2-point segment using your original Delta/Header logic"""
        f = self.get_file(p1[0], p1[1])
        
        # Original Header logic: numPoints is 2
        lats = [p1[0], p2[0]]
        lons = [p1[1], p2[1]]
        
        f.write(struct.pack("fffffI", 
                            float(f_type), 
                            min(lats), max(lats), 
                            min(lons), max(lons), 
                            2)) # 2 points
        
        # Original First Point (Absolute)
        ref_lat, ref_lon = p1[0], p1[1]
        f.write(struct.pack("ff", ref_lat, ref_lon))
        
        # Original Delta Method for the second point
        d_lat = int(round((p2[0] - ref_lat) * 100000))
        d_lon = int(round((p2[1] - ref_lon) * 100000))
        
        d_lat = max(-32768, min(32767, d_lat))
        d_lon = max(-32768, min(32767, d_lon))
        
        f.write(struct.pack("hh", d_lat, d_lon))

    def way(self, w):
        f_type = 0
        if 'highway' in w.tags:
            hw = w.tags.get('highway')
            if hw in ['motorway', 'trunk', 'primary', 'secondary', 'motorway_link', 'trunk_link']:
                f_type = 1
            else:
                f_type = 2 
        elif 'waterway' in w.tags or w.tags.get('natural') == 'water':
            f_type = 3
        elif 'railway' in w.tags:
            f_type = 4
        elif 'building' in w.tags:
            f_type = 5

        if f_type == 0 or len(w.nodes) < 2:
            return

        try:
            valid_nodes = []
            for n in w.nodes:
                try:
                    if n.lat is not None and n.lon is not None:
                        valid_nodes.append((n.lat, n.lon))
                except (osmium.InvalidLocationError, AttributeError):
                    continue

            if len(valid_nodes) < 2:
                return

            # --- SEGMENT SPLITTING LOGIC ---
            # We iterate through each segment of the road
            for i in range(len(valid_nodes) - 1):
                p1 = valid_nodes[i]
                p2 = valid_nodes[i+1]
                
                # Calculate tile indices for both nodes
                t1_lat, t1_lon = int(math.floor(p1[0]/GRID_SIZE)), int(math.floor(p1[1]/GRID_SIZE))
                t2_lat, t2_lon = int(math.floor(p2[0]/GRID_SIZE)), int(math.floor(p2[1]/GRID_SIZE))

                if (t1_lat == t2_lat) and (t1_lon == t2_lon):
                    # Segment is entirely within one tile
                    self.write_to_tile(f_type, p1, p2)
                else:
                    # Segment crosses a boundary. 
                    # We write the segment to BOTH tiles so it's visible in either.
                    self.write_to_tile(f_type, p1, p2) # Write to Tile 1
                    
                    # Manually trigger get_file for the second tile to write there too
                    f2 = self.get_file(p2[0], p2[1])
                    # Re-running the write logic for the second tile
                    lats, lons = [p1[0], p2[0]], [p1[1], p2[1]]
                    f2.write(struct.pack("fffffI", float(f_type), min(lats), max(lats), min(lons), max(lons), 2))
                    f2.write(struct.pack("ff", p1[0], p1[1]))
                    f2.write(struct.pack("hh", 
                        max(-32768, min(32767, int(round((p2[0]-p1[0])*100000)))), 
                        max(-32768, min(32767, int(round((p2[1]-p1[1])*100000))))))

            self.count += 1
            if self.count % 10000 == 0:
                print(f"Processed {self.count} features...")

        except Exception:
            pass 

    def close_all(self):
        for f in self.files.values():
            f.close()

if __name__ == "__main__":
    if os.path.exists(OUTPUT_DIR):
        shutil.rmtree(OUTPUT_DIR)
    
    handler = MapProcessor()
    handler.apply_file(INPUT_PBF, locations=True, idx='flex_mem')
    handler.close_all()
    print(f"Split tiling complete. Total features saved: {handler.count}")