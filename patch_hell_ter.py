#!/usr/bin/env python3
"""
Patch Hell.ter.rsrc:
- Remove tree items (type 17) if any
- Add exit log (type 39) at lowest terrain point
- Add 2 queen bees (type 49) at 3/4 depth, middle of terrain
"""

import struct
import sys
import os

RSRC_PATH = os.path.join(os.path.dirname(__file__), "Data/Terrain/Hell.ter.rsrc")

# TerrainItemEntryType: x(u16), y(u16), type(u16), parm[4](s8x4), flags(u16) = 12 bytes
ITEM_FMT = ">HHH4bH"
ITEM_SIZE = 12

def pack_item(x, y, type_, parms=(0,0,0,0), flags=0):
    return struct.pack(ITEM_FMT, x, y, type_, parms[0], parms[1], parms[2], parms[3], flags)


def read_file(path):
    with open(path, "rb") as f:
        return bytearray(f.read())


def write_file(path, data):
    with open(path, "wb") as f:
        f.write(data)


def parse_appledouble(data):
    """Return offset of resource fork data within the AppleDouble file."""
    magic = struct.unpack_from(">Q", data, 0)[0]
    assert magic == 0x0005160700020000, f"Not AppleDouble: {magic:016x}"
    # Skip 16 bytes of header (magic 8 + version 4 + filler 4... actually magic+version=12, then 16 filler = 28 total before entry count)
    # AppleDouble format: 4 magic, 4 version, 16 filler, 2 num_entries
    num_entries = struct.unpack_from(">H", data, 24)[0]
    entries = {}
    for i in range(num_entries):
        off = 26 + i * 12
        entry_id, entry_offset, entry_length = struct.unpack_from(">III", data, off)
        entries[entry_id] = (entry_offset, entry_length)
    # Entry ID 2 = resource fork
    assert 2 in entries, "No resource fork entry"
    return entries[2][0]  # offset into file where resource fork begins


def parse_resource_fork(data, rf_start):
    """Parse Mac resource fork, return dict of resources."""
    # Resource fork header: data offset, map offset, data length, map length (all u32)
    data_off, map_off, data_len, map_len = struct.unpack_from(">IIII", data, rf_start)
    abs_data_off = rf_start + data_off
    abs_map_off = rf_start + map_off

    # Map header: copy of header (16 bytes), next handle (4), file ref (2), attributes (2),
    #             type list offset (2), name list offset (2)
    type_list_off = struct.unpack_from(">H", data, abs_map_off + 24)[0]
    abs_type_list = abs_map_off + type_list_off

    num_types = struct.unpack_from(">H", data, abs_type_list)[0] + 1

    resources = {}
    for i in range(num_types):
        t_off = abs_type_list + 2 + i * 8
        res_type = data[t_off:t_off+4].decode("latin-1")
        num_refs = struct.unpack_from(">H", data, t_off+4)[0] + 1
        ref_list_off = struct.unpack_from(">H", data, t_off+6)[0]
        abs_ref_list = abs_type_list + ref_list_off

        for j in range(num_refs):
            r_off = abs_ref_list + j * 12
            res_id = struct.unpack_from(">H", data, r_off)[0]
            # name offset (2), attributes (1), data offset 3 bytes
            attrs_and_data = struct.unpack_from(">I", data, r_off+4)[0]
            res_data_rel = attrs_and_data & 0x00FFFFFF
            abs_res_data = abs_data_off + res_data_rel
            res_data_len = struct.unpack_from(">I", data, abs_res_data)[0]
            abs_res_payload = abs_res_data + 4

            key = (res_type, res_id)
            resources[key] = {
                "abs_data_start": abs_res_data,  # points to the 4-byte length prefix
                "abs_payload": abs_res_payload,
                "length": res_data_len,
                "ref_off": r_off,  # offset of reference entry (for patching data offset field)
                "res_data_rel": res_data_rel,
            }

    return resources, abs_data_off, abs_map_off, data_off, map_off


def main():
    data = read_file(RSRC_PATH)
    rf_start = parse_appledouble(data)
    print(f"Resource fork at file offset: {rf_start}")

    resources, abs_data_off, abs_map_off, data_off, map_off = parse_resource_fork(data, rf_start)

    # --- Read Hedr ---
    # PlayfieldHeaderType format: ">I5i3f2i"
    # offset 0:  version      (uint32)
    # offset 4:  numItems     (int32)
    # offset 8:  mapWidth     (int32)
    # offset 12: mapHeight    (int32)
    # offset 16: numTilePages (int32)
    # offset 20: numTilesInList (int32)
    # offset 24: tileSize     (float)
    # offset 28: minY         (float)
    # offset 32: maxY         (float)
    # offset 36: numSplines   (int32)
    # offset 40: numFences    (int32)
    hedr = resources[("Hedr", 1000)]
    hp = hedr["abs_payload"]
    num_items  = struct.unpack_from(">i", data, hp + 4)[0]
    map_width  = struct.unpack_from(">i", data, hp + 8)[0]
    map_height = struct.unpack_from(">i", data, hp + 12)[0]
    tile_size  = struct.unpack_from(">f", data, hp + 24)[0]
    print(f"Map: {map_width}×{map_height} tiles, {num_items} items, tileSize={tile_size}")

    # --- Read YCrd to find lowest vertex ---
    ycrd = resources[("YCrd", 1000)]
    yp = ycrd["abs_payload"]
    yc_len = ycrd["length"]
    num_y = yc_len // 4
    min_y = float("inf")
    min_idx = 0
    for i in range(num_y):
        y = struct.unpack_from(">f", data, yp + i * 4)[0]
        if y < min_y:
            min_y = y
            min_idx = i
    # YCrd stores (map_width+1) * (map_height+1) vertices, row-major
    verts_wide = map_width + 1
    min_row = min_idx // verts_wide
    min_col = min_idx % verts_wide
    print(f"Lowest vertex: tile ({min_col},{min_row}), world_y={min_y:.1f}")

    # --- Read Itms ---
    itms = resources[("Itms", 1000)]
    ip = itms["abs_payload"]
    il = itms["length"]
    assert il % ITEM_SIZE == 0
    n = il // ITEM_SIZE

    items = []
    for i in range(n):
        off = ip + i * ITEM_SIZE
        x, y, type_, p0, p1, p2, p3, flags = struct.unpack_from(ITEM_FMT, data, off)
        items.append({"x": x, "y": y, "type": type_, "parms": (p0,p1,p2,p3), "flags": flags})

    types_present = sorted(set(it["type"] for it in items))
    print(f"Types before: {types_present}")

    # Remove trees (type 17), exit logs (type 39), and all queen bee items (type 49)
    # — exit logs and queen bees are rebuilt below from scratch, making the script idempotent
    for t, label in [(17, "Trees"), (39, "Exit logs"), (49, "Queen bee items")]:
        removed = [it for it in items if it["type"] == t]
        print(f"{label} (type {t}) removed: {len(removed)}")
    items = [it for it in items if it["type"] not in (17, 39, 49)]

    # Lowest point in item coords: tile col × 32, tile row × 32
    OREOMAP_TILE_SIZE = 32
    exit_x = min_col * OREOMAP_TILE_SIZE
    exit_y = min_row * OREOMAP_TILE_SIZE
    print(f"Exit log → tile ({min_col},{min_row}), item coords ({exit_x},{exit_y})")

    # Queens: one spawner at the center + patrol bases around it
    mid_row  = int(map_height * 0.5)
    mid_col  = int(map_width  * 0.5)
    spread_c = int(map_width  * 0.12)
    spread_r = int(map_height * 0.12)

    qb_x_mid  = mid_col              * OREOMAP_TILE_SIZE
    qb_y_mid  = mid_row              * OREOMAP_TILE_SIZE
    qb_x_left = (mid_col - spread_c) * OREOMAP_TILE_SIZE
    qb_x_rght = (mid_col + spread_c) * OREOMAP_TILE_SIZE
    qb_y_top  = (mid_row - spread_r) * OREOMAP_TILE_SIZE
    qb_y_bot  = (mid_row + spread_r) * OREOMAP_TILE_SIZE
    print(f"Queen spawner  → ({mid_col},{mid_row})")
    print(f"Patrol bases   → left/right/top/bot of center")

    # Add new items (no exit log — level ends when both queens die)
    # One spawner at center — code spawns both queens from this single item (Hell-specific)
    items.append({"x": qb_x_mid, "y": qb_y_mid, "type": 49, "parms": (0,0,0,0), "flags": 0})
    # Patrol bases the queens fly between
    items.append({"x": qb_x_left, "y": qb_y_mid, "type": 49, "parms": (1,0,0,0), "flags": 0})
    items.append({"x": qb_x_rght, "y": qb_y_mid, "type": 49, "parms": (2,0,0,0), "flags": 0})
    items.append({"x": qb_x_mid,  "y": qb_y_top, "type": 49, "parms": (3,0,0,0), "flags": 0})
    items.append({"x": qb_x_mid,  "y": qb_y_bot, "type": 49, "parms": (4,0,0,0), "flags": 0})

    # Items must be sorted by x
    items.sort(key=lambda it: it["x"])

    new_n = len(items)
    old_il = il
    new_il = new_n * ITEM_SIZE
    delta = new_il - old_il
    print(f"Total items: {n} → {new_n}")
    print(f"Items data size change: {old_il} → {new_il} (delta={delta:+d} bytes)")

    # Build new items blob
    new_itms_blob = b""
    for it in items:
        new_itms_blob += pack_item(it["x"], it["y"], it["type"], it["parms"], it["flags"])

    # The Itms resource data area = 4-byte length prefix + payload
    itms_abs_start = itms["abs_data_start"]  # points to 4-byte length
    itms_abs_end   = itms_abs_start + 4 + old_il

    # Splice: replace old Itms payload region
    new_data = data[:itms_abs_start] + \
               struct.pack(">I", new_il) + \
               new_itms_blob + \
               data[itms_abs_end:]

    # Now fix up all resource reference entries that pointed AFTER itms_abs_start
    # (their res_data_rel offsets need +delta)
    # Also fix up Hedr numItems field and the resource fork map/data offsets

    # The resource fork header itself needs map_off updated if map comes after data
    # (Usually map is after data section in classic Mac resource forks)
    # Reparse after splice to update everything correctly.
    # Easier: manually patch reference list offsets and the fork header.

    # --- Update resource fork header (data_off and map_off are relative to rf_start) ---
    # data_off stays the same (data section starts at same place)
    # map_off increases by delta (map comes after data section)
    rf_h_off = rf_start
    old_map_off = map_off
    new_map_off = map_off + delta

    struct.pack_into(">I", new_data, rf_h_off + 4, new_map_off)  # map_off field
    # data_len increases by delta
    old_data_len = struct.unpack_from(">I", data, rf_h_off + 8)[0]
    struct.pack_into(">I", new_data, rf_h_off + 8, old_data_len + delta)

    # --- Update copy of header at start of map (same values) ---
    new_abs_map_off = rf_start + new_map_off
    struct.pack_into(">I", new_data, new_abs_map_off + 4, new_map_off)
    struct.pack_into(">I", new_data, new_abs_map_off + 8, old_data_len + delta)

    # --- Update Itms resource reference entry: its res_data_rel stays unchanged
    #     (it's still the same offset from abs_data_off) ---
    # BUT: the Itms length field in the data section is already patched above.

    # --- Update all OTHER resource reference entries that pointed AFTER Itms ---
    # We need to re-parse the resource map from the (now shifted) new position.
    # The reference entries store 3-byte data offsets relative to abs_data_off.
    # Any resource whose abs_data_start > itms_abs_end needs +delta to its stored offset.

    new_abs_map_off2 = rf_start + new_map_off
    type_list_off2 = struct.unpack_from(">H", new_data, new_abs_map_off2 + 24)[0]
    abs_type_list2 = new_abs_map_off2 + type_list_off2
    num_types2 = struct.unpack_from(">H", new_data, abs_type_list2)[0] + 1

    for i in range(num_types2):
        t_off2 = abs_type_list2 + 2 + i * 8
        res_type2 = new_data[t_off2:t_off2+4].decode("latin-1")
        num_refs2 = struct.unpack_from(">H", new_data, t_off2+4)[0] + 1
        ref_list_off2 = struct.unpack_from(">H", new_data, t_off2+6)[0]
        abs_ref_list2 = abs_type_list2 + ref_list_off2

        for j in range(num_refs2):
            r_off2 = abs_ref_list2 + j * 12
            res_id2 = struct.unpack_from(">H", new_data, r_off2)[0]
            attrs_and_data2 = struct.unpack_from(">I", new_data, r_off2+4)[0]
            res_data_rel2 = attrs_and_data2 & 0x00FFFFFF
            abs_res_data2 = abs_data_off + res_data_rel2

            # If this resource's data is located after where we spliced, shift it
            if abs_res_data2 > itms_abs_start and not (res_type2 == "Itms" and res_id2 == 1000):
                new_rel = res_data_rel2 + delta
                new_attrs_and_data = (attrs_and_data2 & 0xFF000000) | (new_rel & 0x00FFFFFF)
                struct.pack_into(">I", new_data, r_off2+4, new_attrs_and_data)
                print(f"  Shifted {res_type2}/{res_id2}: rel {res_data_rel2:#x} → {new_rel:#x}")

    # --- Update Hedr numItems ---
    # Find Hedr resource in updated data
    hedr_abs_start2 = resources[("Hedr", 1000)]["abs_data_start"]
    # Hedr is before Itms in the data section? Check.
    if hedr_abs_start2 > itms_abs_start:
        hedr_abs_start2 += delta
    hedr_payload2 = hedr_abs_start2 + 4
    struct.pack_into(">i", new_data, hedr_payload2 + 4, new_n)
    print(f"Updated Hedr numItems: {num_items} → {new_n}")

    # Verify
    check_n = struct.unpack_from(">i", new_data, hedr_payload2 + 4)[0]
    assert check_n == new_n, f"Hedr numItems mismatch: {check_n} != {new_n}"

    write_file(RSRC_PATH, new_data)
    print(f"Written {len(new_data)} bytes to {RSRC_PATH}")
    print("Done.")


if __name__ == "__main__":
    main()
