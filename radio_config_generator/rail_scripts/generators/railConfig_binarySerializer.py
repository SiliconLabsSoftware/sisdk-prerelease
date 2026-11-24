#!/usr/bin/python
"""Serialize in-memory RAIL model context to a generic object stream."""

from __future__ import annotations

import sys
from typing import Any, Dict, List, Optional, Tuple

from rail_scripts.serialization import serial_ir


def _part_family(rail_adapter) -> str:
    family = getattr(rail_adapter, 'partFamily', None)
    if not family and getattr(rail_adapter, 'mphyConfig', None) is not None:
        family = getattr(rail_adapter.mphyConfig, 'part_family', None)
    return str(family or '').lower()


def _part_features(part_family: str) -> dict:
    if part_family not in serial_ir.PARTS:
        raise ValueError(
            f'binary serializer supports xg23 and xg25; '
            f'got part family {part_family!r}'
        )
    return serial_ir.PARTS[part_family]


def _channel_attr_cal_value_count(part_family: str) -> int:
    return int(_part_features(part_family)['channel_attr_u32_count'])


def _hfxo_compensation(part_family: str) -> bool:
    return bool(_part_features(part_family)['hfxo_compensation'])


def _model_ref_name(obj) -> Optional[str]:
    if obj is None:
        return None
    if isinstance(obj, str):
        return obj
    if hasattr(obj, '_uniqueName'):
        return obj._uniqueName
    if hasattr(obj, 'name'):
        return obj.name
    return None


def _as_int(value, default=0) -> int:
    if value is None:
        return default
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, (int, float)):
        return int(value)
    if isinstance(value, str):
        text = value.strip()
        if text.lower().startswith('0x'):
            return int(text, 16)
        return int(text)
    raise TypeError(f'cannot convert {type(value).__name__} to int: {value!r}')


def _as_byte_list(values) -> List[int]:
    if values is None:
        return []
    if hasattr(values, 'values'):
        values = values.values
    out = []
    for v in values:
        if isinstance(v, bool):
            out.append(int(v))
        elif isinstance(v, (int, float)):
            out.append(int(v) & 0xFF)
        else:
            out.append(_as_int(v) & 0xFF)
    return out


def _as_length_prefixed_bytes(values) -> bytes:
    payload = _as_byte_list(values)
    if len(payload) > 255:
        raise ValueError(
            f'length-prefixed payload exceeds 255 bytes: {len(payload)}'
        )
    return bytes([len(payload)]) + bytes(payload)


def _as_int_list(values) -> List[int]:
    if values is None:
        return []
    if hasattr(values, 'values'):
        values = values.values
    return [_as_int(v) for v in values]


def _build_tx_ir_cal_bytes(values) -> bytes:
    ints = _as_int_list(values)
    if not ints:
        return b''
    data = bytearray(serial_ir.pack_u16(ints[0] & 0xFFFF))
    for v in ints[1:]:
        data.append(v & 0xFF)
    return bytes(data)


def _as_power(value, default=0) -> int:
    if value == 'RAIL_TX_POWER_MAX':
        return 32767
    return _as_int(value, default)


def _modem_config_to_uint32_array(modem_config: dict) -> Tuple[List[int], List[Tuple[int, Any]]]:
    """Return (words, [(word_offset, ref), ...]) for pointer slots."""
    result = []
    fixups = []
    for _name, entry in modem_config.items():
        result.append(_as_int(entry['encodedAction']))
        for value in entry['encodedValues']:
            if isinstance(value, (int, float)):
                result.append(_as_int(value))
            else:
                fixups.append((len(result), value))
                result.append(0)
    result.append(0xFFFFFFFF)
    return result, fixups


def _build_hfxo_bytes(hfxo_entry: dict) -> bytes:
    info_entries = list(hfxo_entry.get('hfxoRetimingInfoConfigEntries', {}).values())
    band_entries = list(hfxo_entry.get('hfxoRetimingBandConfigEntries', {}).values())
    if len(info_entries) > 255:
        raise ValueError(
            f'hfxoRetimingInfoConfigEntries exceeds 255 entries: {len(info_entries)}'
        )
    data = bytearray()
    data.append(len(info_entries))
    data.extend(b'\x00\x00\x00')
    for info in info_entries:
        data.extend(serial_ir.pack_u32(_as_int(info['dpllClock'])))
        data.append(_as_int(info['numBand']) & 0xFF)
        data.append(_as_int(info['offset']) & 0xFF)
        data.extend(b'\x00\x00')
    for band in band_entries:
        data.extend(serial_ir.pack_u16(_as_int(band['loFreqUpperRange'])))
        data.append(_as_int(band['valid']) & 0xFF)
        data.append(_as_int(band['sMuxDiv']) & 0xFF)
        data.append(_as_int(band['hfxoLimitL']) & 0xFF)
        data.append(_as_int(band['hfxoLimitH']) & 0xFF)
    return bytes(data)


# phyInfo uint32 word indices that hold pointers (rail_config_multi_phy_2x.c.j2).
_PHY_INFO_PTR_WORDS = {
    'irCalConfig': 3,
    'timingConfig': 4,
    'dcdcRetimingConfig': 11,
    'hfxoRetimingConfig': 12,
    'rffpllConfig': 17,
    'txIrCalConfig': 18,
    'modemTxCompensation': 19,
}


def _add_recipe(recipes: List[serial_ir.Recipe], byte_offset: int, obj_id: Optional[int]) -> None:
    if obj_id is None:
        return
    recipes.append((byte_offset, obj_id))


class ObjectPool(object):
    """Name → SerialObject, with sequential ids. ChannelConfig always uses ROOT_OBJECT_ID."""

    def __init__(self):
        self.by_name: Dict[str, serial_ir.SerialObject] = {}
        self.objects: List[serial_ir.SerialObject] = []
        self._next_id = 1
        self.root_object_id = serial_ir.ROOT_OBJECT_ID

    def add(self, name: str, data: bytes, recipes: Optional[List[serial_ir.Recipe]] = None,
            comment: str = '') -> int:
        recipes = list(recipes or [])
        if name in self.by_name:
            existing = self.by_name[name]
            if existing.data != data or existing.recipes != recipes:
                raise ValueError(
                    f'object name collision with different payload: {name!r}'
                )
            return existing.id
        obj = serial_ir.SerialObject(
            id=self._next_id,
            data=data,
            recipes=recipes,
            comment=comment or name,
        )
        self._next_id += 1
        self.by_name[name] = obj
        self.objects.append(obj)
        return obj.id

    def add_channel_config(self, data: bytes, recipes: List[serial_ir.Recipe],
                           comment: str = '') -> int:
        obj = serial_ir.SerialObject(
            id=self.root_object_id,
            data=data,
            recipes=list(recipes),
            comment=comment,
        )
        self.objects.append(obj)
        return obj.id

    def lookup_id(self, ref) -> Optional[int]:
        name = _model_ref_name(ref)
        if not name:
            return None
        obj = self.by_name.get(name)
        if obj is None:
            raise ValueError(f'unresolved serial object reference: {name!r}')
        return obj.id


class RAILConfigBinarySerializer(object):
    """Build SerialObjects from a rail adapter, then encode the .bin + plan JSON."""

    def __init__(self, rail_adapter, output_basename='rail_config'):
        self.rail_adapter = rail_adapter
        self.output_basename = output_basename
        self.part_family = _part_family(rail_adapter)

    def build_objects(self) -> List[serial_ir.SerialObject]:
        context = self.rail_adapter.generateRailModelContext()
        multi_phy = context['multiPhyConfig']
        common = multi_phy['commonStructures']
        entries = multi_phy['multiPhyConfigEntries']

        if len(entries) != 1:
            print(
                'warning: serialization expects a single base_channel_configuration; '
                f'found {len(entries)}. Every RAIL_ChannelConfig_t will use '
                f'ROOT_OBJECT_ID={serial_ir.ROOT_OBJECT_ID}, which collides.',
                file=sys.stderr,
            )

        pool = ObjectPool()
        self._build_common_objects(pool, common)

        for config_name, entry in entries.items():
            self._build_channel_config(pool, config_name, entry)

        return pool.objects

    def _build_common_objects(self, pool: ObjectPool, common: dict) -> None:
        for name, values in common.get('frameCodingTableEntries', {}).items():
            pool.add(name, serial_ir.pack_uint32_array(_as_int_list(values)))

        for name, values in common.get('irCalConfigEntries', {}).items():
            pool.add(name, _as_length_prefixed_bytes(values))

        for name, values in common.get('txIrCalConfigEntries', {}).items():
            pool.add(name, _build_tx_ir_cal_bytes(values))

        for name, values in common.get('rffpllConfigEntries', {}).items():
            pool.add(name, serial_ir.pack_uint32_array(_as_int_list(values)))

        if _hfxo_compensation(self.part_family):
            for name, values in common.get('modemTxCompensationEntries', {}).items():
                pool.add(name, serial_ir.pack_uint16_array(_as_int_list(values)))

        for name, values in common.get('dcdcRetimingConfigEntries', {}).items():
            pool.add(name, _as_length_prefixed_bytes(values))

        for name, values in common.get('timingConfigEntries', {}).items():
            pool.add(name, serial_ir.pack_int32_array(_as_int_list(values)))

        for name, entry in common.get('hfxoRetimingTableEntries', {}).items():
            pool.add(name, _build_hfxo_bytes(entry))

        for name, values in common.get('stackInfoEntries', {}).items():
            pool.add(name, bytes(_as_byte_list(values)))

        attr_words = [0xFFFFFFFF] * _channel_attr_cal_value_count(self.part_family)
        for name, _entry in common.get('railChannelConfigEntryAttrEntries', {}).items():
            pool.add(name, serial_ir.pack_uint32_array(attr_words))

        for name, alt in common.get('railAlternatePhyEntries', {}).items():
            pool.add(name, _pack_alternate_phy(alt), comment=name)

        for name, phy_info_entry in common.get('phyInfoEntries', {}).items():
            data, recipes = self._pack_phy_info(phy_info_entry['phyInfoData'], pool)
            pool.add(name, data, recipes)

        for name, modem_config in common.get('modemConfigEntriesBase', {}).items():
            data, recipes = self._pack_modem_config(modem_config, pool)
            pool.add(name, data, recipes)

        for name, modem_config in common.get('modemConfigEntries', {}).items():
            data, recipes = self._pack_modem_config(modem_config, pool)
            pool.add(name, data, recipes)

        for name, modem_config in common.get('modemConfigEntriesSubtract', {}).items():
            data, recipes = self._pack_modem_config(modem_config, pool)
            pool.add(name, data, recipes)

    def _pack_phy_info(self, phy_info_data: dict, pool: ObjectPool):
        words = [
            _as_int(phy_info_data.get('version')),
            _as_int(phy_info_data.get('freqOffsetFactor_fxp')),
            0,  # frameTypeConfig: not serialized
            0,  # irCalConfig
            0,  # timingConfig
            _as_int(phy_info_data.get('antDivRxAutoConfig')),
            _as_int(phy_info_data.get('src1Denominator')),
            _as_int(phy_info_data.get('src2Denominator')),
            _as_int(phy_info_data.get('txBaudRate')),
            _as_int(phy_info_data.get('rateInfo')),
            _as_int(phy_info_data.get('synthCache')),
            0,  # dcdcRetimingConfig
            0,  # hfxoRetimingConfig
            _as_int(phy_info_data.get('zWaveChannelHopTiming')),
            _as_int(phy_info_data.get('paParams')),
            _as_int(phy_info_data.get('psmMaxSleepUs')),
            _as_int(phy_info_data.get('rxBaudRate')),
            0,  # rffpllConfig
            0,  # txIrCalConfig
            0,  # modemTxCompensation
            _as_int(phy_info_data.get('trecsPreBitsToSync')),
            _as_int(phy_info_data.get('preambleDurationUs')),
            _as_int(phy_info_data.get('syncwordDurationUs')),
        ]
        recipes: List[serial_ir.Recipe] = []
        for field, word_index in _PHY_INFO_PTR_WORDS.items():
            if field == 'modemTxCompensation' and not _hfxo_compensation(self.part_family):
                continue
            _add_recipe(recipes, word_index * 4, pool.lookup_id(phy_info_data.get(field)))
        return serial_ir.pack_uint32_array(words), recipes

    def _pack_modem_config(self, modem_config: dict, pool: ObjectPool):
        words, raw_fixups = _modem_config_to_uint32_array(modem_config)
        recipes: List[serial_ir.Recipe] = []
        for word_offset, ref in raw_fixups:
            _add_recipe(recipes, word_offset * 4, pool.lookup_id(ref))
        return serial_ir.pack_uint32_array(words), recipes

    def _build_channel_config(self, pool: ObjectPool, config_name: str, entry: dict) -> None:
        channel_config = entry['channelConfig']
        channel_entries = list(entry.get('channelConfigEntries', {}).items())

        array_data = bytearray()
        array_recipes: List[serial_ir.Recipe] = []
        entry_size = serial_ir.CHANNEL_CONFIG_ENTRY_SIZE
        for entry_index, (ch_name, ch_entry) in enumerate(channel_entries):
            packed, recipes = _pack_channel_config_entry(ch_entry, pool)
            base = entry_index * entry_size
            array_data.extend(packed)
            for offset, obj_id in recipes:
                array_recipes.append((base + offset, obj_id))

        channels_name = f'{config_name}_channels'
        channels_id = pool.add(
            channels_name, bytes(array_data), array_recipes, comment=channels_name,
        )

        cfg_recipes: List[serial_ir.Recipe] = []
        data = bytearray(serial_ir.CHANNEL_CONFIG_SIZE)
        # phyConfigBase @0
        base_id = pool.lookup_id(channel_config.get('modemConfigBase'))
        _add_recipe(cfg_recipes, 0, base_id)
        # phyConfigDeltaSubtract @4
        _add_recipe(cfg_recipes, 4, pool.lookup_id(channel_config.get('modemConfigDeltaSubtract')))
        # configs @8
        _add_recipe(cfg_recipes, 8, channels_id)
        data[12:16] = serial_ir.pack_u32(_as_int(channel_config.get('length')))
        data[16:20] = serial_ir.pack_u32(_as_int(channel_config.get('signature')))
        data[20:24] = serial_ir.pack_u32(_as_int(channel_config.get('xtalFrequency')))
        pool.add_channel_config(bytes(data), cfg_recipes, comment=config_name)

    def serialize(self) -> Tuple[Dict[str, bytes], str]:
        objects = self.build_objects()
        ordered = serial_ir.sort_objects_for_stream(objects)
        encoded = serial_ir.encode_sorted_stream(ordered)
        plan = serial_ir.render_plan_text(ordered, self.output_basename, encoded)
        files = {
            f'{self.output_basename}.bin': encoded,
            f'{self.output_basename}_serialization.json': plan.encode('ascii'),
        }
        return files, plan


def _pack_alternate_phy(entry: dict) -> bytes:
    packed = (
        serial_ir.pack_u32(_as_int(entry.get('baseFrequency')))
        + serial_ir.pack_u32(_as_int(entry.get('channelSpacing')))
        + serial_ir.pack_u16(_as_int(entry.get('numberOfChannels')))
        + serial_ir.pack_u16(_as_int(entry.get('minIf_kHz')))
        + serial_ir.pack_u16(_as_int(entry.get('minBaseIf_kHz')))
        + serial_ir.pack_u8(1 if entry.get('isOfdmModem') else 0)
        + b'\x00'
        + serial_ir.pack_u32(_as_int(entry.get('rateInfo')))
        + serial_ir.pack_u32(_as_int(entry.get('hwModemAgcCtrl1')))
    )
    if len(packed) != serial_ir.ALTERNATE_PHY_SIZE:
        raise ValueError(
            f'RAIL_AlternatePhy_t pack size {len(packed)} != '
            f'{serial_ir.ALTERNATE_PHY_SIZE}'
        )
    return packed


def _pack_channel_config_entry(ch_entry: dict, pool: ObjectPool):
    if ch_entry.get('physicalChannelOffset') == 'SAME_AS_FIRST_CHANNEL':
        phys_offset = _as_int(ch_entry.get('channelNumberStart'))
    else:
        phys_offset = _as_int(ch_entry.get('physicalChannelOffset'))

    recipes: List[serial_ir.Recipe] = []
    data = bytearray(serial_ir.CHANNEL_CONFIG_ENTRY_SIZE)
    _add_recipe(recipes, 0, pool.lookup_id(ch_entry.get('modemConfigDeltaAdd')))
    data[4:8] = serial_ir.pack_u32(_as_int(ch_entry.get('baseFrequency')))
    data[8:12] = serial_ir.pack_u32(_as_int(ch_entry.get('channelSpacing')))
    data[12:14] = serial_ir.pack_u16(phys_offset)
    data[14:16] = serial_ir.pack_u16(_as_int(ch_entry.get('channelNumberStart')))
    data[16:18] = serial_ir.pack_u16(_as_int(ch_entry.get('channelNumberEnd')))
    data[18:20] = serial_ir.pack_i16(_as_power(ch_entry.get('maxPower'), 0))
    _add_recipe(recipes, 20, pool.lookup_id(ch_entry.get('attr')))
    data[24] = _as_int(ch_entry.get('entryType')) & 0xFF
    # reserved[3] already zero
    stack_ref = ch_entry.get('stackInfo')
    _add_recipe(recipes, 28, pool.lookup_id(stack_ref))
    _add_recipe(recipes, 32, pool.lookup_id(ch_entry.get('alternatePhy')))
    return bytes(data), recipes


def serialize_rail_config(rail_adapter, output_basename='rail_config'):
    serializer = RAILConfigBinarySerializer(rail_adapter, output_basename)
    return serializer.serialize()
