-- PLATFORM_HYD-9652: preserve the conditional selections of the legacy wrapper.
-- ECDHE-RSA replacement is deferred; map PSK/ECDSA only for now.
local changeset = {}

if not slc.is_selected('mbedtls_ecdhe_key_exchange') then
    return changeset
end

table.insert(changeset, {
    ['component'] = 'mbedtls_ecdhe_key_exchange',
    ['action'] = 'remove',
    ['status'] = 'user_verification',
    ['description'] = 'Replaced the deprecated ECDHE component. Verify TLS '
        .. 'ciphersuites and any SL_MBEDTLS_KEY_EXCHANGE_* overrides; '
        .. 'source/API migration must be completed separately. ECDHE-RSA '
        .. 'is not auto-migrated yet.'
})
table.insert(changeset, {
    ['component'] = 'mbedtls_key_exchange_ecdhe_psk',
    ['action'] = 'add'
})

-- The old wrapper enabled ECDHE-ECDSA only when psa_crypto_ecdsa was provided.
if slc.is_provided('psa_crypto_ecdsa') then
    table.insert(changeset, {
        ['component'] = 'mbedtls_key_exchange_ecdhe_ecdsa',
        ['action'] = 'add'
    })
end

return changeset
