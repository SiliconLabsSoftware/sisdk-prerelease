//#include <stdint.h>
#include <climits>
#include <sys/select.h>
//#include <openthread/instance.h>
#include "openthread-core-config.h"
#include "openthread/platform/radio.h"
#include "common/log.hpp"
#include "common/settings.hpp"
#include "instance/instance.hpp"
#include <openthread/dataset.h>
#include "meshcop/dataset.hpp"
#include "utils/parse_cmdline.hpp"
#include <psa/crypto.h>

//----------------------------------------------------------------------
// ParseAsHexString implementation (required for use in src/posix/platform/radio.cpp)
// Borrowed from util/third_party/openthread/src/core/utils/parse_cmdline.cpp

using namespace ot;
using namespace Utils;

static Error MapPsaStatus(psa_status_t aStatus)
{
    return (aStatus == PSA_SUCCESS) ? kErrorNone : kErrorFailed;
}

enum HexStringParseMode
{
    kModeExtactSize,   // Parse hex string expecting an exact size (number of bytes when parsed).
    kModeUpToSize,     // Parse hex string expecting less than or equal a given size.
    kModeAllowPartial, // Allow parsing of partial segments.
};

static Error ParseHexString(const char *&aString, uint16_t &aSize, uint8_t *aBuffer, HexStringParseMode aMode)
{
    Error  error      = kErrorNone;
    size_t parsedSize = 0;
    size_t stringLength;
    size_t expectedSize;
    bool   skipFirstDigit;

    VerifyOrExit(aString != nullptr, error = kErrorInvalidArgs);

    stringLength = strlen(aString);
    expectedSize = (stringLength + 1) / 2;

    switch (aMode)
    {
    case kModeExtactSize:
        VerifyOrExit(expectedSize == aSize, error = kErrorInvalidArgs);
        break;
    case kModeUpToSize:
        VerifyOrExit(expectedSize <= aSize, error = kErrorInvalidArgs);
        break;
    case kModeAllowPartial:
        break;
    }

    // If number of chars in hex string is odd, we skip parsing
    // the first digit.

    skipFirstDigit = ((stringLength & 1) != 0);

    while (parsedSize < expectedSize)
    {
        uint8_t digit;

        if ((aMode == kModeAllowPartial) && (parsedSize == aSize))
        {
            // If partial parse mode is allowed, stop once we read the
            // requested size.
            ExitNow(error = kErrorPending);
        }

        if (skipFirstDigit)
        {
            *aBuffer       = 0;
            skipFirstDigit = false;
        }
        else
        {
            SuccessOrExit(error = ParseHexDigit(*aString, digit));
            aString++;
            *aBuffer = static_cast<uint8_t>(digit << 4);
        }

        SuccessOrExit(error = ParseHexDigit(*aString, digit));
        aString++;
        *aBuffer |= digit;

        aBuffer++;
        parsedSize++;
    }

    aSize = static_cast<uint16_t>(parsedSize);

exit:
    return error;
}

Error CmdLineParser::ParseAsHexString(const char *aString, uint16_t &aSize, uint8_t *aBuffer)
{
    return ParseHexString(aString, aSize, aBuffer, kModeUpToSize);
}

//----------------------------------------------------------------------
// Crypto stubs (PSA): RNG + AES-ECB for otPlatCrypto*
// Replaces legacy mbedtls CTR-DRBG / AES (Mbed TLS 4.x).

using namespace ot;
using namespace Crypto;

static constexpr size_t kAesBlockSize = 16;

OT_TOOL_WEAK void otPlatCryptoRandomInit(void)
{
    psa_status_t status = psa_crypto_init();
    OT_ASSERT(status == PSA_SUCCESS);
    OT_UNUSED_VARIABLE(status);
}

OT_TOOL_WEAK void otPlatCryptoRandomDeinit(void)
{
    // PSA Crypto has no required global deinit for this host stub path.
}

OT_TOOL_WEAK otError otPlatCryptoRandomGet(uint8_t *aBuffer, uint16_t aSize)
{
    return MapPsaStatus(psa_generate_random(aBuffer, aSize));
}

OT_TOOL_WEAK otError otPlatCryptoAesInit(otCryptoContext *aContext)
{
    Error         error = kErrorNone;
    psa_key_id_t *keyId;

    VerifyOrExit(aContext != nullptr, error = kErrorInvalidArgs);
    VerifyOrExit(aContext->mContextSize >= sizeof(psa_key_id_t), error = kErrorFailed);

    keyId  = static_cast<psa_key_id_t *>(aContext->mContext);
    *keyId = 0;

exit:
    return error;
}

OT_TOOL_WEAK otError otPlatCryptoAesSetKey(otCryptoContext *aContext, const otCryptoKey *aKey)
{
    Error                error = kErrorNone;
    psa_key_id_t        *keyId;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t         status;
    const LiteralKey     key(*static_cast<const Key *>(aKey));

    VerifyOrExit(aContext != nullptr, error = kErrorInvalidArgs);
    VerifyOrExit(aContext->mContextSize >= sizeof(psa_key_id_t), error = kErrorFailed);

    keyId = static_cast<psa_key_id_t *>(aContext->mContext);
    if (*keyId != 0)
    {
        (void)psa_destroy_key(*keyId);
        *keyId = 0;
    }

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECB_NO_PADDING);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, key.GetLength() * CHAR_BIT);

    status = psa_import_key(&attributes, key.GetBytes(), key.GetLength(), keyId);
    psa_reset_key_attributes(&attributes);
    VerifyOrExit(status == PSA_SUCCESS, error = kErrorFailed);

exit:
    return error;
}

OT_TOOL_WEAK otError otPlatCryptoAesEncrypt(otCryptoContext *aContext, const uint8_t *aInput, uint8_t *aOutput)
{
    Error         error = kErrorNone;
    psa_key_id_t *keyId;
    psa_status_t  status;
    size_t        outputLength = 0;
    uint8_t       temp[kAesBlockSize];

    VerifyOrExit(aContext != nullptr, error = kErrorInvalidArgs);
    VerifyOrExit(aContext->mContextSize >= sizeof(psa_key_id_t), error = kErrorFailed);

    keyId = static_cast<psa_key_id_t *>(aContext->mContext);
    VerifyOrExit(*keyId != 0, error = kErrorInvalidState);

    status = psa_cipher_encrypt(*keyId,
                                PSA_ALG_ECB_NO_PADDING,
                                aInput,
                                kAesBlockSize,
                                temp,
                                sizeof(temp),
                                &outputLength);
    VerifyOrExit(status == PSA_SUCCESS && outputLength == kAesBlockSize, error = kErrorFailed);
    memcpy(aOutput, temp, kAesBlockSize);

exit:
    return error;
}

OT_TOOL_WEAK otError otPlatCryptoAesFree(otCryptoContext *aContext)
{
    Error         error = kErrorNone;
    psa_key_id_t *keyId;

    VerifyOrExit(aContext != nullptr, error = kErrorInvalidArgs);
    VerifyOrExit(aContext->mContextSize >= sizeof(psa_key_id_t), error = kErrorFailed);

    keyId = static_cast<psa_key_id_t *>(aContext->mContext);
    if (*keyId != 0)
    {
        (void)psa_destroy_key(*keyId);
        *keyId = 0;
    }

exit:
    return error;
}

//----------------------------------------------------------------------
// Stubs for MAC frame builder operation
// Used for mac-frame and aes-ccm code, not used for any Spinel operations

using namespace ot;

Error Message::Read(uint16_t aOffset, void *aBuf, uint16_t aLength) const
{
    (void) aOffset;
    (void) aBuf;
    (void) aLength;
    return OT_ERROR_NOT_IMPLEMENTED;
}

// Stubs for AesCcm::Process(Message&) — not used by zigbeed; satisfies linker only.

Error Message::IncreaseLength(uint16_t aSize)
{
    (void) aSize;
    return OT_ERROR_NOT_IMPLEMENTED;
}

void Message::WriteBytes(uint16_t aOffset, const void *aBuf, uint16_t aLength)
{
    (void) aOffset;
    (void) aBuf;
    (void) aLength;
}

bool Message::CompareBytes(uint16_t aOffset, const void *aBuf, uint16_t aLength, ByteMatcher aMatcher) const
{
    (void) aOffset;
    (void) aBuf;
    (void) aLength;
    (void) aMatcher;
    return false;
}

void Message::RemoveFooter(uint16_t aLength)
{
    (void) aLength;
}

// Stubs for AES-CCM GetFirstChunk/GetNextChunk operations

void Message::GetFirstChunk(uint16_t aOffset, uint16_t &aLength, Chunk &aChunk) const
{
    // This method gets the first message chunk (contiguous data
    // buffer) corresponding to a given offset and length. On exit
    // `aChunk` is updated such that `aChunk.GetBytes()` gives the
    // pointer to the start of chunk and `aChunk.GetLength()` gives
    // its length. The `aLength` is also decreased by the chunk
    // length.

    VerifyOrExit(aOffset < GetLength(), aChunk.SetLength(0));

    if (aOffset + aLength >= GetLength())
    {
        aLength = GetLength() - aOffset;
    }

    aOffset += GetReserved();

    aChunk.SetBuffer(this);

    // Special case for the first buffer

    if (aOffset < kHeadBufferDataSize)
    {
        aChunk.Init(GetFirstData() + aOffset, kHeadBufferDataSize - aOffset);
        ExitNow();
    }

    aOffset -= kHeadBufferDataSize;

    // Find the `Buffer` matching the offset

    while (true)
    {
        aChunk.SetBuffer(aChunk.GetBuffer()->GetNextBuffer());

        OT_ASSERT(aChunk.GetBuffer() != nullptr);

        if (aOffset < kBufferDataSize)
        {
            aChunk.Init(aChunk.GetBuffer()->GetData() + aOffset, kBufferDataSize - aOffset);
            ExitNow();
        }

        aOffset -= kBufferDataSize;
    }

exit:
    if (aChunk.GetLength() > aLength)
    {
        aChunk.SetLength(aLength);
    }

    aLength -= aChunk.GetLength();
}

void Message::GetNextChunk(uint16_t &aLength, Chunk &aChunk) const
{
    // This method gets the next message chunk. On input, the
    // `aChunk` should be the previous chunk. On exit, it is
    // updated to provide info about next chunk, and `aLength`
    // is decreased by the chunk length. If there is no more
    // chunk, `aChunk.GetLength()` would be zero.

    VerifyOrExit(aLength > 0, aChunk.SetLength(0));

    aChunk.SetBuffer(aChunk.GetBuffer()->GetNextBuffer());

    OT_ASSERT(aChunk.GetBuffer() != nullptr);

    aChunk.Init(aChunk.GetBuffer()->GetData(), kBufferDataSize);

    if (aChunk.GetLength() > aLength)
    {
        aChunk.SetLength(aLength);
    }

    aLength -= aChunk.GetLength();

exit:
    return;
}

//----------------------------------------------------------------------
// Logging stubs

using namespace ot;

#if OPENTHREAD_CONFIG_LOG_LEVEL_DYNAMIC_ENABLE

Error Instance::SetLogLevel(LogLevel aLogLevel)
{
    Error error = kErrorNone;

    if (aLogLevel != mLogLevel)
    {
        mLogLevel = aLogLevel;
    }

    return error;
}
#endif

//----------------------------------------------------------------------
// Misc. OpenThread stubs

using namespace ot;

// Defined in src/posix/platform/netif.cpp, not used for RCP.
// Referenced from src/posix/platform/system.cpp
char gNetifName[0];

// Found in src/core/api/instance_api.cpp and referenced from timer.cpp
bool otInstanceIsInitialized(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return true;
}

// Referenced from system.cpp
bool otTaskletsArePending(otInstance *aInstance)
{
    (void) aInstance;
    OT_UNUSED_VARIABLE(aInstance);
    return false;
}

void otInstanceFinalize(otInstance *aInstance)
{
    (void) aInstance;
}

uint32_t otLinkGetFrameCounter(otInstance *aInstance)
{
    (void) aInstance;
    return UINT32_MAX;
}

otError otSetStateChangedCallback(otInstance *aInstance, otStateChangedCallback aCallback, void *aContext)
{
    (void) aInstance;
    (void) aCallback;
    (void) aContext;
    return OT_ERROR_NONE;
}

namespace ot {
    OT_DEFINE_ALIGNED_VAR(gInstanceRaw, sizeof(Instance), uint64_t);

    // Found in instance_api.cpp and referenced  from system.cpp
    extern "C" otInstance *otInstanceInitSingle(void)
    {
        return (otInstance *)gInstanceRaw;
    }

    Instance &Instance::Get(void)
    {
        void *instance = &gInstanceRaw;

        return *static_cast<Instance *>(instance);
    }
} // namespace ot

bool otIp6IsEnabled(otInstance *aInstance)
{
    (void) aInstance;
    return true;
}
