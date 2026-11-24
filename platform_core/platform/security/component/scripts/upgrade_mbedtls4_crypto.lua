-- PLATFORM_HYD-9652: migrate legacy Mbed TLS crypto selections to PSA Crypto.
-- Do not auto-enable mbedtls_declare_private_identifiers; apps that still call
-- classic mbedtls_* crypto APIs must opt into that component explicitly.
-- Keep selected mbedtls_* components: psa_crypto_*.slcc gates
-- MBEDTLS_PSA_BUILTIN_* on those selections for software fallback when HW
-- acceleration is incomplete.
local changeset = {}

local map = {
    mbedtls_aes = 'psa_crypto_aes',
    mbedtls_ccm = 'psa_crypto_ccm',
    mbedtls_gcm = 'psa_crypto_gcm',
    mbedtls_chachapoly = 'psa_crypto_chachapoly',
    mbedtls_cipher_cbc = 'psa_crypto_cipher_cbc',
    mbedtls_cipher_cfb = 'psa_crypto_cipher_cfb',
    mbedtls_cipher_ctr = 'psa_crypto_cipher_ctr',
    mbedtls_cipher_ofb = 'psa_crypto_cipher_ofb',
    mbedtls_cipher_xts = 'psa_crypto_cipher_xts',
    mbedtls_cipher_padding_pkcs7 = 'psa_crypto_cipher_cbc_pkcs7',
    mbedtls_cmac = 'psa_crypto_cmac',
    mbedtls_hash = 'psa_crypto_hash',
    mbedtls_hmac = 'psa_crypto_hmac',
    mbedtls_hkdf = 'psa_crypto_hkdf',
    mbedtls_md5 = 'psa_crypto_md5',
    mbedtls_sha1 = 'psa_crypto_sha1',
    mbedtls_sha224 = 'psa_crypto_sha224',
    mbedtls_sha256 = 'psa_crypto_sha256',
    mbedtls_sha384 = 'psa_crypto_sha384',
    mbedtls_sha512 = 'psa_crypto_sha512',
    mbedtls_ecdh = 'psa_crypto_ecdh',
    mbedtls_ecdsa = 'psa_crypto_ecdsa',
    mbedtls_ecdsa_deterministic = 'psa_crypto_ecdsa_deterministic',
    mbedtls_ecc = 'psa_crypto_ecc',
    mbedtls_ecc_secp256r1 = 'psa_crypto_ecc_secp256r1',
    mbedtls_ecc_secp384r1 = 'psa_crypto_ecc_secp384r1',
    mbedtls_ecc_secp521r1 = 'psa_crypto_ecc_secp521r1',
    mbedtls_ecc_curve25519 = 'psa_crypto_ecc_curve25519',
    mbedtls_ecc_curve448 = 'psa_crypto_ecc_curve448',
    mbedtls_rsa = 'psa_crypto_rsa',
    mbedtls_rsa_1024 = 'psa_crypto_rsa_1024',
    mbedtls_rsa_2048 = 'psa_crypto_rsa_2048',
    mbedtls_rsa_4096 = 'psa_crypto_rsa_4096',
    mbedtls_rsa_8192 = 'psa_crypto_rsa_8192',
}

local adds = {}

local function queue_add(id)
    if adds[id] then
        return
    end
    adds[id] = true
    table.insert(changeset, {
        ['component'] = id,
        ['action'] = 'add'
    })
end

-- Deterministic order for reviewable upgrade output.
local old_ids = {}
for old_id, _ in pairs(map) do
    table.insert(old_ids, old_id)
end
table.sort(old_ids)

for _, old_id in ipairs(old_ids) do
    if slc.is_selected(old_id) then
        queue_add(map[old_id])
    end
end

-- mbedtls_sha is an umbrella for all Silicon Labs SHA families.
if slc.is_selected('mbedtls_sha') then
    queue_add('psa_crypto_sha1')
    queue_add('psa_crypto_sha224')
    queue_add('psa_crypto_sha256')
    queue_add('psa_crypto_sha384')
    queue_add('psa_crypto_sha512')
end

if #changeset == 0 then
    return changeset
end

changeset[1]['status'] = 'user_verification'
changeset[1]['description'] =
    'Added PSA Crypto equivalents for selected Mbed TLS crypto components. '
    .. 'Left mbedtls_* selections in place so MBEDTLS_PSA_BUILTIN_* software '
    .. 'fallback remains available when HW/driver support is incomplete. '
    .. 'Port classic mbedtls_* crypto API calls to PSA Crypto. Do not enable '
    .. 'mbedtls_declare_private_identifiers implicitly; add it only if sources '
    .. 'still require private mbedtls_* identifiers. Drop mbedtls_* later only '
    .. 'for intentional driver-only / size-optimized configs.'

return changeset
