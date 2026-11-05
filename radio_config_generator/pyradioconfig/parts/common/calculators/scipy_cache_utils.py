"""Helpers for optional SciPy resampling caches.

This module is intentionally conservative:
- Caching is enabled only when RADIO_CONF_ADVANCED_CACHE_EN is true.
- Only the default scipy.signal.resample_poly Kaiser window path is cached.
- All fallback paths preserve scipy.signal.resample_poly behavior.
"""

from collections import OrderedDict
import math
from threading import Lock

import numpy as np
from scipy import signal as sp

from pyradioconfig.calculator_model_framework.Utils.cache_flags import is_advanced_cache_enabled


_ADVANCED_CACHE_EN = is_advanced_cache_enabled()
_DEFAULT_RESAMPLE_WINDOW = ("kaiser", 5.0)
_RESAMPLE_TAPS_CACHE_MAXSIZE = 256

_resample_taps_cache = OrderedDict()
_resample_taps_cache_lock = Lock()


def _reduce_ratio(up, down):
    """Return the canonical up/down ratio used internally by resample_poly.

    SciPy reduces the ratio by gcd(up, down) before designing the filter.
    We do the same so cache keys match SciPy behavior:
      (4, 2), (2, 1), and (200, 100) all mean the same resampling ratio.
    """
    up_i = int(up)
    down_i = int(down)
    divisor = math.gcd(up_i, down_i)
    return up_i // divisor, down_i // divisor


def _build_resample_taps(window, up_reduced, down_reduced, x_dtype):
    """Build FIR taps equivalent to SciPy's default window-design path.

    resample_poly designs a low-pass FIR via firwin when `window` is not
    precomputed taps. This helper builds that same FIR so we can reuse it.
    """
    max_rate = max(up_reduced, down_reduced)
    f_c = 1.0 / max_rate
    half_len = 10 * max_rate

    taps = sp.firwin(2 * half_len + 1, f_c, window=window)

    dtype = np.dtype(x_dtype)
    if np.issubdtype(dtype, np.complexfloating) or np.issubdtype(dtype, np.floating):
        taps = taps.astype(dtype, copy=False)

    return taps


def _get_resample_tap_dtype_key(x_dtype):
    """Return the tap-precision key for cache reuse.

    Float/complex callers need exact dtype separation because we cast taps to
    that dtype before caching. Non-float inputs all reuse the default float64
    taps produced by firwin, so one shared bucket is sufficient there.
    """
    dtype = np.dtype(x_dtype)
    if np.issubdtype(dtype, np.complexfloating) or np.issubdtype(dtype, np.floating):
        return dtype.str
    return "default"


def _get_cached_resample_taps(window, up, down, x_dtype):
    """Fetch or build taps for the default resample_poly Kaiser window.

    Returns None for a no-op ratio (1/1), where SciPy already has a fast path.
    """
    up_reduced, down_reduced = _reduce_ratio(up, down)
    if up_reduced == 1 and down_reduced == 1:
        return None

    key = (window, up_reduced, down_reduced, _get_resample_tap_dtype_key(x_dtype))

    with _resample_taps_cache_lock:
        cached = _resample_taps_cache.get(key)
        if cached is not None:
            _resample_taps_cache.move_to_end(key)
            return cached

    taps = _build_resample_taps(window, up_reduced, down_reduced, x_dtype)

    with _resample_taps_cache_lock:
        cached = _resample_taps_cache.get(key)
        if cached is not None:
            _resample_taps_cache.move_to_end(key)
            return cached

        _resample_taps_cache[key] = taps
        if len(_resample_taps_cache) > _RESAMPLE_TAPS_CACHE_MAXSIZE:
            _resample_taps_cache.popitem(last=False)

    return taps


def resample_poly_cached(x, up, down, axis=0, window=_DEFAULT_RESAMPLE_WINDOW, padtype="constant", cval=None):
    """resample_poly wrapper with optional FIR tap cache.

    If advanced cache is disabled, this is equivalent to scipy.signal.resample_poly.
    """
    if not _ADVANCED_CACHE_EN:
        return sp.resample_poly(x, up, down, axis=axis, window=window, padtype=padtype, cval=cval)

    if window != _DEFAULT_RESAMPLE_WINDOW:
        return sp.resample_poly(x, up, down, axis=axis, window=window, padtype=padtype, cval=cval)

    try:
        x_array = np.asarray(x)
        taps = _get_cached_resample_taps(window, up, down, x_array.dtype)
        if taps is None:
            return sp.resample_poly(x_array, up, down, axis=axis, window=window, padtype=padtype, cval=cval)

        return sp.resample_poly(x_array, up, down, axis=axis, window=taps, padtype=padtype, cval=cval)
    except Exception:
        return sp.resample_poly(x, up, down, axis=axis, window=window, padtype=padtype, cval=cval)
