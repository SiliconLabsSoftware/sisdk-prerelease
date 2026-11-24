"""Generic serial-object IR and binary encode helpers."""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Sequence, Tuple

# Stream / NVM3 contract (little-endian). Mirror CRC params in the C deserializer.
ROOT_OBJECT_ID = 0
POINTER_SIZE = 4
STREAM_LENGTH_SIZE = 4  # u32
OBJECT_HEADER_SIZE = 6  # u16 id + u16 len_data + u16 len_recipe
RECIPE_ENTRY_SIZE = 4   # u16 offset + u16 id
# NVM3 stores len_data|len_recipe|data|recipe (id is in the NVM3 key).
NVM3_MAX_OBJECT_SIZE = 4096  # NVM3_MAX_OBJECT_SIZE_HIGH_LIMIT

# Bit-reflected CRC-32/ISO-HDLC (LSB-first).
CRC_SIZE = 4
CRC_WIDTH = 32
CRC_POLY = 0xEDB88320
CRC_INIT = 0xFFFFFFFF
CRC_XOROUT = 0xFFFFFFFF

# RAIL 2.x memcpy layouts used by the packer (xg23/xg25, ARM EABI).
CHANNEL_CONFIG_SIZE = 24
CHANNEL_CONFIG_ENTRY_SIZE = 36
ALTERNATE_PHY_SIZE = 24

# Supported serializer parts keyed by internal partFamily names.
# hfxo_compensation mirrors RAIL_SUPPORTS_HFXO_COMPENSATION (true only on
# SERIES_2_CONFIG == 5 / xg25). channel_attr_u32_count follows
# RAIL_SUPPORTS_OFDM_PA (RxIrCalValues vs IrCalValues).
PARTS = {
    'ocelot': {  # xg23
        'hfxo_compensation': False,
        'channel_attr_u32_count': 2,
    },
    'sol': {  # xg25
        'hfxo_compensation': True,
        'channel_attr_u32_count': 4,
    },
}


def pack_u8(value: int) -> bytes:
    return struct.pack('<B', value & 0xFF)


def pack_u16(value: int) -> bytes:
    return struct.pack('<H', value & 0xFFFF)


def pack_i16(value: int) -> bytes:
    return struct.pack('<h', int(value))


def pack_u32(value: int) -> bytes:
    return struct.pack('<I', value & 0xFFFFFFFF)


def pack_i32(value: int) -> bytes:
    return struct.pack('<i', int(value))


def pack_uint32_array(values: List[int]) -> bytes:
    return b''.join(pack_u32(v) for v in values)


def pack_uint16_array(values: List[int]) -> bytes:
    return b''.join(pack_u16(v) for v in values)


def pack_int32_array(values: List[int]) -> bytes:
    return b''.join(pack_i32(v) for v in values)


def crc32(data: bytes, crc: int | None = None) -> int:
    mask = (1 << CRC_WIDTH) - 1
    if crc is None:
        crc = CRC_INIT
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ CRC_POLY
            else:
                crc >>= 1
            crc &= mask
    return crc ^ CRC_XOROUT


Recipe = Tuple[int, int]  # (byte_offset, object_id)


@dataclass
class SerialObject:
    """One untyped stream object: memcpy-ready data plus pointer recipes."""
    id: int
    data: bytes
    recipes: List[Recipe] = field(default_factory=list)
    comment: str = ''

    @property
    def len_data(self) -> int:
        return len(self.data)

    @property
    def recipe_bytes(self) -> bytes:
        out = bytearray()
        for offset, obj_id in self.recipes:
            out.extend(pack_u16(offset))
            out.extend(pack_u16(obj_id))
        return bytes(out)

    @property
    def len_recipe(self) -> int:
        return len(self.recipe_bytes)

    @property
    def nvm3_value_size(self) -> int:
        # Stored in NVM3 as len_data|len_recipe|data|recipe (no id).
        return 4 + self.len_data + self.len_recipe


def _check_object_sizes(obj: SerialObject) -> None:
    if obj.id < 0 or obj.id > 0xFFFF:
        raise ValueError(f'object id out of range: {obj.id}')
    if obj.len_data > 0xFFFF:
        raise ValueError(
            f'object data too large: {obj.len_data} bytes (id={obj.id})'
        )
    if obj.len_recipe > 0xFFFF:
        raise ValueError(
            f'object recipe too large: {obj.len_recipe} bytes (id={obj.id})'
        )
    if obj.nvm3_value_size > NVM3_MAX_OBJECT_SIZE:
        raise ValueError(
            f'NVM3 object exceeds {NVM3_MAX_OBJECT_SIZE} bytes: '
            f'4 + len_data({obj.len_data}) + len_recipe({obj.len_recipe}) '
            f'= {obj.nvm3_value_size} (id={obj.id} {obj.comment!r})'
        )
    for offset, _obj_id in obj.recipes:
        if offset < 0 or offset > 0xFFFF:
            raise ValueError(f'recipe offset out of range: {offset}')
        if offset % POINTER_SIZE != 0:
            raise ValueError(f'recipe offset {offset} is not pointer-aligned')
        if offset + POINTER_SIZE > obj.len_data:
            raise ValueError(
                f'recipe offset {offset} past data of length {obj.len_data}'
            )


def encode_object(obj: SerialObject) -> bytes:
    _check_object_sizes(obj)
    recipe = obj.recipe_bytes
    return (
        pack_u16(obj.id)
        + pack_u16(obj.len_data)
        + pack_u16(obj.len_recipe)
        + obj.data
        + recipe
    )


def sort_objects_for_stream(objects: Iterable[SerialObject]) -> List[SerialObject]:
    """Largest (len_data + len_recipe) first; ROOT_OBJECT_ID always last.

    Non-root objects keep descending size order (stable on insert order).
    Every RAIL_ChannelConfig_t (id == ROOT_OBJECT_ID) is placed after all
    other objects, still size-ordered among themselves.
    """
    indexed = list(enumerate(objects))
    indexed.sort(
        key=lambda item: (
            item[1].id == ROOT_OBJECT_ID,
            -(item[1].len_data + item[1].len_recipe),
            item[0],
        )
    )
    return [obj for _, obj in indexed]


def encode_sorted_stream(sorted_objects: Sequence[SerialObject]) -> bytes:
    """Encode an already-sorted object list into a framed stream.

    Callers must pass objects from sort_objects_for_stream(); this does not
    re-sort.
    """
    objects_blob = b''.join(encode_object(obj) for obj in sorted_objects)
    stream_length = len(objects_blob) + CRC_SIZE
    header = pack_u32(stream_length)
    if len(header) != STREAM_LENGTH_SIZE:
        raise ValueError('STREAM_LENGTH_SIZE does not match packed u32')
    trailer = pack_u32(crc32(header + objects_blob))
    if len(trailer) != CRC_SIZE:
        raise ValueError('CRC_SIZE does not match packed u32')
    return header + objects_blob + trailer


def _data_preview(data: bytes, bytes_per_line: int = 32) -> List[str]:
    if not data:
        return []
    lines = []
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i:i + bytes_per_line]
        lines.append(' '.join(f'{b:02X}' for b in chunk))
    return lines


def render_plan_text(sorted_objects: List[SerialObject], basename: str,
                     encoded: bytes) -> str:
    """Build the human-readable plan JSON for an already-encoded sorted stream."""
    plan: Dict[str, object] = {
        'basename': basename,
        'filename': f'{basename}.bin',
        'size_bytes': len(encoded),
        'objects': [],
    }
    for obj in sorted_objects:
        plan['objects'].append({
            'comment': obj.comment,
            'id': obj.id,
            'len_data': obj.len_data,
            'len_recipe': obj.len_recipe,
            'data_preview': _data_preview(obj.data),
            'recipes': [
                {'offset': offset, 'id': ref_id} for offset, ref_id in obj.recipes
            ],
        })
    return json.dumps(plan, indent=2, sort_keys=False) + '\n'
