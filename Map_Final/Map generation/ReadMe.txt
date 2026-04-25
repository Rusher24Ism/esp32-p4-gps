Requirements:
1.Python
2.Anaconda/Miniconda(for windows)
3.Osmium
4.If there are errors, search them to find solutions online

Steps:

1.Use osmium (on conda for windows) to get vector roads, rivers, buildings etc. from original pbf.
2.Use tile_split.py to split the filtered obf into tiles.
3.Use toles_bin.py to convert tiles into bin.
4.Use verify_bins.py to verify bins.
5.Store the bins in /Map/tiles/