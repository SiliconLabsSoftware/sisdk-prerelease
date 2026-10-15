# mbed TLS / TF-PSA-Crypto

## URL

https://github.com/Mbed-TLS/mbedtls/releases/tag/v4.1.0

TF-PSA-Crypto (submodule of Mbed TLS 4.x):
https://github.com/Mbed-TLS/TF-PSA-Crypto

## Version

4.1.0 (Mbed TLS) / 1.1.0 (TF-PSA-Crypto, as bundled with Mbed TLS v4.1.0)

Aligned with Silicon Labs `mbedtls/4.1.0@silabs/feature_mbedtls4` (OPENTHREAD-6613 / OPENTHREAD-7020).

## License

Apache 2.0

## License File

[LICENSE](repo/LICENSE)

## Description

Mbed TLS 4.x splits the former monolithic crypto+TLS tree:

- **TF-PSA-Crypto** (`repo/tf-psa-crypto`) — PSA Crypto API and cryptographic primitives
- **Mbed TLS** (`repo/library`, `repo/include/mbedtls`) — TLS / DTLS / X.509 upper layers

OpenThread build files:

- `BUILD.gn` — `tfpsacrypto` + `mbedtls` static libraries
- `CMakeLists.txt` — links `mbedtls`, `mbedx509`, and `tfpsacrypto` (`GEN_FILES=ON`)
- `mbedtls-config.h` — TLS/X.509 options (`MBEDTLS_CONFIG_FILE`)
- `psa-crypto-config.h` — PSA / crypto options (`TF_PSA_CRYPTO_CONFIG_FILE`)

After checking out `repo` at `v4.1.0`, initialize nested submodules:

```bash
git -C repo submodule update --init --recursive
```

CMake generates helper sources at build time when `GEN_FILES=ON` (OpenThread default).
Install Python packages first:

```bash
# Debian/Ubuntu
sudo apt-get install -y python3-jinja2 python3-jsonschema
# or: pip3 install jinja2 jsonschema
```

For GN (or a one-shot regenerate into the source tree):

```bash
# from third_party/mbedtls/repo
python3 framework/scripts/generate_ssl_debug_helpers.py --mbedtls-root . library/
perl scripts/generate_features.pl include/mbedtls scripts/data_files library/version_features.c
perl scripts/generate_errors.pl tf-psa-crypto/drivers/builtin/include/mbedtls include/mbedtls scripts/data_files library/error.c
python3 tf-psa-crypto/scripts/generate_driver_wrappers.py
```
