"""Zip a folder's contents (the card layout: Roms/...) for a release: zip_dir.py <dir> <out.zip>
Scripts and the program get mode 755, everything else 644 (the card is FAT anyway; for other
unzip tools)."""
import os
import sys
import zipfile

src, out = sys.argv[1], sys.argv[2]
EXEC = {"launch.sh", "prehistorik"}
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, dirs, files in os.walk(src):
        dirs.sort()
        for name in sorted(files):
            path = os.path.join(root, name)
            arc = os.path.relpath(path, src).replace(os.sep, "/")
            info = zipfile.ZipInfo.from_file(path, arc)
            mode = 0o755 if name in EXEC or name.endswith(".port") else 0o644
            info.external_attr = (0o100000 | mode) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as f:
                z.writestr(info, f.read(), compresslevel=9)
print(out)
