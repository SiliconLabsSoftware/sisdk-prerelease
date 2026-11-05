from collections import OrderedDict

import numpy as np
import numpy.matlib

from pyradioconfig.calculator_model_framework.Utils.cache_flags import is_advanced_cache_enabled


def freeze_for_cache(value):
    """Convert nested values into a hashable, deterministic cache-key structure.

    Why this exists:
    - Numpy arrays/lists/dicts are not directly safe as dictionary keys.
    - We need logically-identical inputs to produce exactly the same key.
    """
    if isinstance(value, np.ndarray):
        return ("ndarray", value.dtype.str, value.shape, value.tobytes())

    if isinstance(value, np.generic):
        return value.item()

    if isinstance(value, (list, tuple)):
        return tuple(freeze_for_cache(item) for item in value)

    if isinstance(value, dict):
        return tuple(
            (freeze_for_cache(k), freeze_for_cache(v))
            for k, v in sorted(value.items(), key=lambda item: str(item[0]))
        )

    if isinstance(value, set):
        return tuple(sorted(freeze_for_cache(item) for item in value))

    return value



_ADVANCED_CACHE_EN = is_advanced_cache_enabled()

_KSI_CALC_CACHE_MAXSIZE = 1024
_ksi_calc_cache = OrderedDict()


def _ksi_cache_get(key):
    if not _ADVANCED_CACHE_EN:
        return None
    cached = _ksi_calc_cache.get(key)
    if cached is not None:
        _ksi_calc_cache.move_to_end(key)
    return cached


def _ksi_cache_put(key, value):
    if not _ADVANCED_CACHE_EN:
        return
    if key in _ksi_calc_cache:
        _ksi_calc_cache.move_to_end(key)
    _ksi_calc_cache[key] = value
    if len(_ksi_calc_cache) > _KSI_CALC_CACHE_MAXSIZE:
        _ksi_calc_cache.popitem(last=False)



class KsiCacheMixin:
    """Mixin providing KSI cache key helpers and the cached return_ksi2_ksi3_calc
    implementation shared across all LPW modem r_08 CalcDemodulator subclasses.

    New-part guide:
    - If your part uses the same SRC2 model variable, no overrides are needed.
    - If only SRC2 source changes (e.g. another FEFILT path), override
      _get_ksi_cache_src2_key() to return the correct model variable value.
    - If gen_frequency_signal() reads additional model variables unique to your
      part, mirror them in _get_ksi_cache_gen_signal_inputs() by overriding it
      or by extending _get_ksi_cache_additional_inputs().
    """

    def _get_ksi_cache_impl_discriminator(self):
        """Return implementation identity used to prevent cache collisions."""
        gen_signal_fn = getattr(self.gen_frequency_signal, "__func__", self.gen_frequency_signal)
        return (
            getattr(gen_signal_fn, "__module__", type(self).__module__),
            getattr(gen_signal_fn, "__qualname__", gen_signal_fn.__class__.__name__),
            getattr(self, "SRC2DENUM", None),
        )

    def _get_ksi_cache_src2_key(self, model):
        """Return the SRC2 value that gen_frequency_signal() uses for this part."""
        return model.vars.FEFILT_SRCCHF_SRCRATIO2.value

    def _get_ksi_cache_additional_inputs(self, model):
        """Hook for inheritors to extend KSI cache key inputs."""
        return ()

    def _get_ksi_cache_gen_signal_inputs(self, model):
        """Inputs that affect gen_frequency_signal() output.

        The left-hand strings (e.g. "deviation", "remoden") are stable labels
        for cache-key readability only. The right-hand values are the actual
        model values used by the math path.

        New-part guide:
        - If you override gen_frequency_signal(), mirror every newly-read model
          variable here so cache invalidates correctly.
        - If only SRC2 source changes (e.g. another FEFILT path), override
          _get_ksi_cache_src2_key() instead of rewriting this whole method.
        - If return_ksi2_ksi3_calc() adds logic outside gen_frequency_signal(),
          add those fields in _get_ksi_cache_additional_inputs().
        """
        demod_select = model.vars.demod_select.value
        is_bcr = demod_select == model.vars.demod_select.var_enum.BCR
        return (
            ("deviation", model.vars.deviation.value),
            ("baudrate", model.vars.baudrate.value),
            ("src2_ratio", self._get_ksi_cache_src2_key(model)),
            ("datafilter", model.vars.MODEM_CTRL2_DATAFILTER.value),
            ("remoden", model.vars.MODEM_PHDMODCTRL_REMODEN.value),
            ("remodoutsel", model.vars.MODEM_PHDMODCTRL_REMODOUTSEL.value),
            ("demod_select", str(demod_select)),
            ("dec2_actual", model.vars.dec2_actual.value),
            ("bcr_rawndec", model.vars.MODEM_BCRDEMODOOK_RAWNDEC.value if is_bcr else None),
            ("bcr_rawgain", model.vars.MODEM_BCRDEMODOOK_RAWGAIN.value if is_bcr else None),
            ("bcr_rawfltsel", model.vars.MODEM_BCRDEMODCTRL_RAWFLTSEL.value if is_bcr else None),
        )

    def _build_ksi_cache_key(self, model, ksi1, chf_acq, chf_lock, osr, sf):
        cache_inputs = (
            ("ksi1", ksi1),
            ("chf_acq", chf_acq),
            ("chf_lock", chf_lock),
            ("osr", osr),
            ("sf", sf),
            ("impl", self._get_ksi_cache_impl_discriminator()),
            ("gen_signal", self._get_ksi_cache_gen_signal_inputs(model)),
        )
        cache_inputs += tuple(self._get_ksi_cache_additional_inputs(model))
        return ("lpw_modem_ksi2_ksi3_v1", freeze_for_cache(cache_inputs))

    def return_ksi2_ksi3_calc(self, model, ksi1):
        chf_acq = model.vars.channel_filter_coeffs_acq_bwsel.value
        chf_lock = model.vars.channel_filter_coeffs_lock_bwsel.value
        osr = int(round(model.vars.oversampling_rate_actual.value))
        shaping_filter_coeffs = model.vars.shaping_filter_coeffs.value

        _ksi_key = None

        if (ksi1 == 0):
            best_ksi2 = 0
            best_ksi3 = 0
            best_ksi3wb = 0
        else:
            sf = np.asarray(shaping_filter_coeffs)
            sfosr = 8  # shaping filter coeffs are sampled at 8x

            if _ADVANCED_CACHE_EN:
                try:
                    _ksi_key = self._build_ksi_cache_key(model, ksi1, chf_acq, chf_lock, osr, sf)
                    _cached = _ksi_cache_get(_ksi_key)
                    if _cached is not None:
                        return _cached
                except Exception:
                    _ksi_key = None

            cfh = np.asarray(chf_lock)
            cf = np.block([cfh, cfh[-2::-1]]) / 1.0
            cfh = np.asarray(chf_acq)
            cfwb = np.block([cfh, cfh[-2::-1]]) / 1.0
            a = np.array([1.0, 0, 0, 0, 0, 0, 0, 0])
            b = np.array([-1.0, 0, 0, 0, 0, 0, 0, 0])
            x1 = np.matlib.repmat(np.append(np.matlib.repmat(a, 1, 3), np.matlib.repmat(b, 1, 3)), 1, 4)
            f1 = self.gen_frequency_signal(x1[0], sf, cf, sfosr, model)
            x2 = np.matlib.repmat(np.append(np.matlib.repmat(a, 1, 2), np.matlib.repmat(b, 1, 2)), 1, 6)
            f2 = self.gen_frequency_signal(x2[0], sf, cf, sfosr, model)
            x3 = np.matlib.repmat(np.append(np.matlib.repmat(a, 1, 1), np.matlib.repmat(b, 1, 1)), 1, 12)
            f3 = self.gen_frequency_signal(x3[0], sf, cf, sfosr, model)
            f3wb = self.gen_frequency_signal(x3[0], sf, cfwb, sfosr, model)
            ind = osr - 1
            scaler = ksi1 / np.max(np.abs(f1[ind + 8 * osr - 1: - 2 * osr: osr]))
            f2 = scaler * f2
            f3 = scaler * f3
            f3wb = scaler * f3wb
            best_ksi3 = 0
            for ph in range(osr):
                ksi3 = np.max(np.round(np.abs(f3[- 6 * osr + ph: - 2 * osr: osr])))
                if ksi3 > best_ksi3:
                    best_ksi3 = ksi3
            best_ksi3wb = 0
            for ph in range(osr):
                ksi3wb = np.max(np.round(np.abs(f3wb[- 6 * osr + ph: - 2 * osr: osr])))
                if ksi3wb > best_ksi3wb:
                    best_ksi3wb = ksi3wb
            best_cost = 1e9
            for ph in range(osr):
                x = np.round(np.abs(f2[- 6 * osr + ph: - 2 * osr: osr]))
                cost = np.sum(np.abs(x - np.mean(x)))
                if cost < best_cost:
                    best_cost = cost
                    best_ksi2 = np.round(np.mean(x))

        best_ksi2 = ksi1 if best_ksi2 > ksi1 else best_ksi2
        best_ksi3 = best_ksi2 if best_ksi3 > best_ksi2 else best_ksi3
        best_ksi3wb = best_ksi2 if best_ksi3wb > best_ksi2 else best_ksi3wb

        _result = (best_ksi2, best_ksi3, best_ksi3wb)
        if _ADVANCED_CACHE_EN and _ksi_key is not None:
            _ksi_cache_put(_ksi_key, _result)
        return _result
