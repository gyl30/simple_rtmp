#define MS_CLASS "RTC::SrtpSession"
// #define MS_LOG_DEV_LEVEL 3

#include "SrtpSession.hpp"
#include "DepLibSRTP.hpp"
#include "log.h"
#include <cstring>    // std::memset(), std::memcpy()

namespace RTC
{
/* Static. */

static constexpr size_t EncryptBufferSize{65536};
thread_local static uint8_t EncryptBuffer[EncryptBufferSize];

/* Class methods. */

void SrtpSession::ClassInit()
{
    // Set libsrtp event handler.
    const srtp_err_status_t err = srtp_install_event_handler(static_cast<srtp_event_handler_func_t*>(OnSrtpEvent));

    if (DepLibSRTP::IsError(err))
    {
        LOG_TRACE("srtp_install_event_handler failed: {}", DepLibSRTP::GetErrorString(err));
    }
}

void SrtpSession::OnSrtpEvent(srtp_event_data_t* data)
{
    switch (data->event)
    {
        case event_ssrc_collision:
            LOG_TRACE("SSRC collision occurred");
            break;

        case event_key_soft_limit:
            LOG_TRACE("stream reached the soft key usage limit and will expire soon");
            break;

        case event_key_hard_limit:
            LOG_TRACE("stream reached the hard key usage limit and has expired");
            break;

        case event_packet_index_limit:
            LOG_TRACE("stream reached the hard packet limit (2^48 packets)");
            break;
    }
}

/* Instance methods. */

SrtpSession::SrtpSession(Type type, CryptoSuite cryptoSuite, uint8_t* key, size_t keyLen)
{
    srtp_policy_t policy;    // NOLINT(cppcoreguidelines-pro-type-member-init)

    // Set all policy fields to 0.
    std::memset(&policy, 0, sizeof(srtp_policy_t));

    switch (cryptoSuite)
    {
        case CryptoSuite::AEAD_AES_256_GCM:
        {
            srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
            srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);

            break;
        }

        case CryptoSuite::AEAD_AES_128_GCM:
        {
            srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
            srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);

            break;
        }

        case CryptoSuite::AES_CM_128_HMAC_SHA1_80:
        {
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

            break;
        }

        case CryptoSuite::AES_CM_128_HMAC_SHA1_32:
        {
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
            // NOTE: Must be 80 for RTCP.
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

            break;
        }

        default:
        {
            LOG_TRACE("unknown SRTP crypto suite");
        }
    }

    if ((int)keyLen != policy.rtp.cipher_key_len)
    {
        LOG_WARN("given keyLen does not match policy.rtp.cipher_keyLen");
    }

    switch (type)
    {
        case Type::INBOUND:
            policy.ssrc.type = ssrc_any_inbound;
            break;

        case Type::OUTBOUND:
            policy.ssrc.type = ssrc_any_outbound;
            break;
    }

    policy.ssrc.value = 0;
    policy.key = key;
    // Required for sending RTP retransmission without RTX.
    policy.allow_repeat_tx = 1;
    policy.window_size = 1024;
    policy.next = nullptr;

    // Set the SRTP session.
    const srtp_err_status_t err = srtp_create(&this->session, &policy);

    if (DepLibSRTP::IsError(err))
    {
        LOG_ERROR("srtp_create failed {}", DepLibSRTP::GetErrorString(err));
    }
}

SrtpSession::~SrtpSession()
{
    if (this->session != nullptr)
    {
        const srtp_err_status_t err = srtp_dealloc(this->session);

        if (DepLibSRTP::IsError(err))
        {
            LOG_ERROR("srtp_dealloc failed {}", DepLibSRTP::GetErrorString(err));
        }
    }
}

bool SrtpSession::EncryptRtp(const uint8_t** data, int* len)
{
    if (static_cast<size_t>(*len) + SRTP_MAX_TRAILER_LEN > EncryptBufferSize)
    {
        LOG_WARN("cannot encrypt RTP packet, size too big {} bytes", *len);
        return false;
    }

    std::memcpy(EncryptBuffer, *data, *len);

    const srtp_err_status_t err = srtp_protect(this->session, static_cast<void*>(EncryptBuffer), len);

    if (DepLibSRTP::IsError(err))
    {
        LOG_WARN("srtp_protect failed {}", DepLibSRTP::GetErrorString(err));
        return false;
    }

    // Update the given data pointer.
    *data = (const uint8_t*)EncryptBuffer;

    return true;
}

bool SrtpSession::DecryptSrtp(uint8_t* data, int* len)
{
    const srtp_err_status_t err = srtp_unprotect(this->session, static_cast<void*>(data), len);

    if (DepLibSRTP::IsError(err))
    {
        LOG_DEBUG("srtp_unprotect failed {}", DepLibSRTP::GetErrorString(err));
        return false;
    }

    return true;
}

bool SrtpSession::EncryptRtcp(const uint8_t** data, int* len)
{
    // Ensure that the resulting SRTCP packet fits into the encrypt buffer.
    if (static_cast<size_t>(*len) + SRTP_MAX_TRAILER_LEN > EncryptBufferSize)
    {
        LOG_WARN("cannot encrypt RTCP packet, size too big {} bytes", *len);
        return false;
    }

    std::memcpy(EncryptBuffer, *data, *len);

    const srtp_err_status_t err = srtp_protect_rtcp(this->session, static_cast<void*>(EncryptBuffer), len);

    if (DepLibSRTP::IsError(err))
    {
        LOG_WARN("srtp_protect_rtcp failed {}", DepLibSRTP::GetErrorString(err));
        return false;
    }

    // Update the given data pointer.
    *data = (const uint8_t*)EncryptBuffer;

    return true;
}

bool SrtpSession::DecryptSrtcp(uint8_t* data, int* len)
{
    const srtp_err_status_t err = srtp_unprotect_rtcp(this->session, static_cast<void*>(data), len);

    if (DepLibSRTP::IsError(err))
    {
        LOG_DEBUG("srtp_unprotect_rtcp failed {}", DepLibSRTP::GetErrorString(err));
        return false;
    }

    return true;
}
}    // namespace RTC
