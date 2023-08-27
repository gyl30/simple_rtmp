#define MS_CLASS "RTC::DtlsTransport"
// #define MS_LOG_DEV_LEVEL 3

#include "DtlsTransport.hpp"
#include "log.h"
#include "timer_task_manger.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <cstdio>     // std::snprintf(), std::fopen()
#include <cstring>    // std::memcpy(), std::strcmp()

#define LOG_OPENSSL_ERROR(desc)                                                                           \
    do                                                                                                    \
    {                                                                                                     \
        if (ERR_peek_error() == 0)                                                                        \
            LOG_ERROR("OpenSSL error [desc:'{}']", desc);                                                 \
        else                                                                                              \
        {                                                                                                 \
            int64_t err;                                                                                  \
            while ((err = ERR_get_error()) != 0)                                                          \
            {                                                                                             \
                LOG_ERROR("OpenSSL error [desc:'{}', error:'{}']", desc, ERR_error_string(err, nullptr)); \
            }                                                                                             \
            ERR_clear_error();                                                                            \
        }                                                                                                 \
    } while (false)

/* Static methods for OpenSSL callbacks. */

inline static int onSslCertificateVerify(int /*preverifyOk*/, X509_STORE_CTX* /*ctx*/)
{
    // Always valid since DTLS certificates are self-signed.
    return 1;
}

inline static void onSslInfo(const SSL* ssl, int where, int ret)
{
    static_cast<RTC::DtlsTransport*>(SSL_get_ex_data(ssl, 0))->OnSslInfo(where, ret);
}

inline static unsigned int onSslDtlsTimer(SSL* /*ssl*/, unsigned int timerUs)
{
    if (timerUs == 0)
        return 100000;
    else if (timerUs >= 4000000)
        return 4000000;
    else
        return 2 * timerUs;
}

namespace RTC
{
/* Static. */

// clang-format off
	static constexpr int DtlsMtu{ 1350 };
	static constexpr int SslReadBufferSize{ 65536 };
	// AES-HMAC: http://tools.ietf.org/html/rfc3711
	static constexpr size_t SrtpMasterKeyLength{ 16 };
	static constexpr size_t SrtpMasterSaltLength{ 14 };
	static constexpr size_t SrtpMasterLength{ SrtpMasterKeyLength + SrtpMasterSaltLength };
	// AES-GCM: http://tools.ietf.org/html/rfc7714
	static constexpr size_t SrtpAesGcm256MasterKeyLength{ 32 };
	static constexpr size_t SrtpAesGcm256MasterSaltLength{ 12 };
	static constexpr size_t SrtpAesGcm256MasterLength{ SrtpAesGcm256MasterKeyLength + SrtpAesGcm256MasterSaltLength };
	static constexpr size_t SrtpAesGcm128MasterKeyLength{ 16 };
	static constexpr size_t SrtpAesGcm128MasterSaltLength{ 12 };
	static constexpr size_t SrtpAesGcm128MasterLength{ SrtpAesGcm128MasterKeyLength + SrtpAesGcm128MasterSaltLength };
// clang-format on

/* Class variables. */

thread_local X509* DtlsTransport::certificate{nullptr};
thread_local EVP_PKEY* DtlsTransport::privateKey{nullptr};
thread_local SSL_CTX* DtlsTransport::sslCtx{nullptr};
thread_local uint8_t DtlsTransport::sslReadBuffer[SslReadBufferSize];
// clang-format off
	std::map<std::string, DtlsTransport::FingerprintAlgorithm> DtlsTransport::string2FingerprintAlgorithm =
	{
		{ "sha-1",   DtlsTransport::FingerprintAlgorithm::SHA1   },
		{ "sha-224", DtlsTransport::FingerprintAlgorithm::SHA224 },
		{ "sha-256", DtlsTransport::FingerprintAlgorithm::SHA256 },
		{ "sha-384", DtlsTransport::FingerprintAlgorithm::SHA384 },
		{ "sha-512", DtlsTransport::FingerprintAlgorithm::SHA512 }
	};
	std::map<DtlsTransport::FingerprintAlgorithm, std::string> DtlsTransport::fingerprintAlgorithm2String =
	{
		{ DtlsTransport::FingerprintAlgorithm::SHA1,   "sha-1"   },
		{ DtlsTransport::FingerprintAlgorithm::SHA224, "sha-224" },
		{ DtlsTransport::FingerprintAlgorithm::SHA256, "sha-256" },
		{ DtlsTransport::FingerprintAlgorithm::SHA384, "sha-384" },
		{ DtlsTransport::FingerprintAlgorithm::SHA512, "sha-512" }
	};
	std::map<std::string, DtlsTransport::Role> DtlsTransport::string2Role =
	{
		{ "auto",   DtlsTransport::Role::AUTO   },
		{ "client", DtlsTransport::Role::CLIENT },
		{ "server", DtlsTransport::Role::SERVER }
	};
	thread_local std::vector<DtlsTransport::Fingerprint> DtlsTransport::localFingerprints;
	std::vector<DtlsTransport::SrtpCryptoSuiteMapEntry> DtlsTransport::srtpCryptoSuites =
	{
		{ RTC::SrtpSession::CryptoSuite::AEAD_AES_256_GCM,        "SRTP_AEAD_AES_256_GCM"  },
		{ RTC::SrtpSession::CryptoSuite::AEAD_AES_128_GCM,        "SRTP_AEAD_AES_128_GCM"  },
		{ RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80, "SRTP_AES128_CM_SHA1_80" },
		{ RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_32, "SRTP_AES128_CM_SHA1_32" }
	};
// clang-format on

/* Class methods. */

void DtlsTransport::ClassInit()
{
    // Generate a X509 certificate and private key (unless PEM files are provided).
    GenerateCertificateAndPrivateKey();
    // ReadCertificateAndPrivateKeyFromFiles();

    // Create a global SSL_CTX.
    CreateSslCtx();

    // Generate certificate fingerprints.
    GenerateFingerprints();
}

void DtlsTransport::ClassDestroy()
{
    if (DtlsTransport::privateKey)
        EVP_PKEY_free(DtlsTransport::privateKey);
    if (DtlsTransport::certificate)
        X509_free(DtlsTransport::certificate);
    if (DtlsTransport::sslCtx)
        SSL_CTX_free(DtlsTransport::sslCtx);
}

void DtlsTransport::GenerateCertificateAndPrivateKey()
{
    int ret{0};
    X509_NAME* certName{nullptr};
    std::string subject = std::string("mediasoup") + std::to_string(rand() % 999999 + 100000);

    // Create key with curve.
    DtlsTransport::privateKey = EVP_EC_gen(SN_X9_62_prime256v1);

    if (!DtlsTransport::privateKey)
    {
        LOG_OPENSSL_ERROR("EVP_EC_gen() failed");

        goto error;
    }

    // Create the X509 certificate.
    DtlsTransport::certificate = X509_new();

    if (!DtlsTransport::certificate)
    {
        LOG_OPENSSL_ERROR("X509_new() failed");

        goto error;
    }

    // Set version 3 (note that 0 means version 1).
    X509_set_version(DtlsTransport::certificate, 2);

    // Set serial number (avoid default 0).
    ASN1_INTEGER_set(X509_get_serialNumber(DtlsTransport::certificate), static_cast<uint64_t>(rand() % 999999 + 100000));

    // Set valid period.
    X509_gmtime_adj(X509_get_notBefore(DtlsTransport::certificate), -315360000);    // -10 years.
    X509_gmtime_adj(X509_get_notAfter(DtlsTransport::certificate), 315360000);      // 10 years.

    // Set the public key for the certificate using the key.
    ret = X509_set_pubkey(DtlsTransport::certificate, DtlsTransport::privateKey);

    if (ret == 0)
    {
        LOG_OPENSSL_ERROR("X509_set_pubkey() failed");

        goto error;
    }

    // Set certificate fields.
    certName = X509_get_subject_name(DtlsTransport::certificate);

    if (!certName)
    {
        LOG_OPENSSL_ERROR("X509_get_subject_name() failed");

        goto error;
    }

    X509_NAME_add_entry_by_txt(certName, "O", MBSTRING_ASC, reinterpret_cast<const uint8_t*>(subject.c_str()), -1, -1, 0);
    X509_NAME_add_entry_by_txt(certName, "CN", MBSTRING_ASC, reinterpret_cast<const uint8_t*>(subject.c_str()), -1, -1, 0);

    // It is self-signed so set the issuer name to be the same as the subject.
    ret = X509_set_issuer_name(DtlsTransport::certificate, certName);

    if (ret == 0)
    {
        LOG_OPENSSL_ERROR("X509_set_issuer_name() failed");

        goto error;
    }

    // Sign the certificate with its own private key.
    ret = X509_sign(DtlsTransport::certificate, DtlsTransport::privateKey, EVP_sha1());

    if (ret == 0)
    {
        LOG_OPENSSL_ERROR("X509_sign() failed");

        goto error;
    }

    return;

error:

    if (DtlsTransport::privateKey)
        EVP_PKEY_free(DtlsTransport::privateKey);

    if (DtlsTransport::certificate)
        X509_free(DtlsTransport::certificate);

    LOG_ERROR("DTLS certificate and private key generation failed");
}

void DtlsTransport::ReadCertificateAndPrivateKeyFromFiles()
{
}

void DtlsTransport::CreateSslCtx()
{
    std::string dtlsSrtpCryptoSuites;
    int ret;

    /* Set the global DTLS context. */

    // Both DTLS 1.0 and 1.2 (requires OpenSSL >= 1.1.0).
    DtlsTransport::sslCtx = SSL_CTX_new(DTLS_method());

    if (!DtlsTransport::sslCtx)
    {
        LOG_OPENSSL_ERROR("SSL_CTX_new() failed");

        goto error;
    }

    ret = SSL_CTX_use_certificate(DtlsTransport::sslCtx, DtlsTransport::certificate);

    if (ret == 0)
    {
        LOG_OPENSSL_ERROR("SSL_CTX_use_certificate() failed");

        goto error;
    }

    ret = SSL_CTX_use_PrivateKey(DtlsTransport::sslCtx, DtlsTransport::privateKey);

    if (ret == 0)
    {
        LOG_OPENSSL_ERROR("SSL_CTX_use_PrivateKey() failed");

        goto error;
    }

    ret = SSL_CTX_check_private_key(DtlsTransport::sslCtx);

    if (ret == 0)
    {
        LOG_OPENSSL_ERROR("SSL_CTX_check_private_key() failed");

        goto error;
    }

    // Set options.
    SSL_CTX_set_options(DtlsTransport::sslCtx, SSL_OP_CIPHER_SERVER_PREFERENCE | SSL_OP_NO_TICKET | SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_QUERY_MTU);

    // Don't use sessions cache.
    SSL_CTX_set_session_cache_mode(DtlsTransport::sslCtx, SSL_SESS_CACHE_OFF);

    // Read always as much into the buffer as possible.
    // NOTE: This is the default for DTLS, but a bug in non latest OpenSSL
    // versions makes this call required.
    SSL_CTX_set_read_ahead(DtlsTransport::sslCtx, 1);

    SSL_CTX_set_verify_depth(DtlsTransport::sslCtx, 4);

    // Require certificate from peer.
    SSL_CTX_set_verify(DtlsTransport::sslCtx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, onSslCertificateVerify);

    // Set SSL info callback.
    SSL_CTX_set_info_callback(DtlsTransport::sslCtx, onSslInfo);

    // Set ciphers.
    ret = SSL_CTX_set_cipher_list(DtlsTransport::sslCtx, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");

    if (ret == 0)
    {
        LOG_OPENSSL_ERROR("SSL_CTX_set_cipher_list() failed");

        goto error;
    }

    // Enable ECDH ciphers.
    // DOC: http://en.wikibooks.org/wiki/OpenSSL/Diffie-Hellman_parameters
    // NOTE: https://code.google.com/p/chromium/issues/detail?id=406458
    // NOTE: https://bugs.ruby-lang.org/issues/12324

    // For OpenSSL >= 1.0.2.
    SSL_CTX_set_ecdh_auto(DtlsTransport::sslCtx, 1);

    // Set the "use_srtp" DTLS extension.
    for (auto it = DtlsTransport::srtpCryptoSuites.begin(); it != DtlsTransport::srtpCryptoSuites.end(); ++it)
    {
        if (it != DtlsTransport::srtpCryptoSuites.begin())
            dtlsSrtpCryptoSuites += ":";

        SrtpCryptoSuiteMapEntry* cryptoSuiteEntry = std::addressof(*it);
        dtlsSrtpCryptoSuites += cryptoSuiteEntry->name;
    }

    LOG_DEBUG("setting SRTP cryptoSuites for DTLS: {}", dtlsSrtpCryptoSuites);

    // NOTE: This function returns 0 on success.
    ret = SSL_CTX_set_tlsext_use_srtp(DtlsTransport::sslCtx, dtlsSrtpCryptoSuites.data());

    if (ret != 0)
    {
        LOG_ERROR("SSL_CTX_set_tlsext_use_srtp() failed when entering '{}'", dtlsSrtpCryptoSuites);
        LOG_OPENSSL_ERROR("SSL_CTX_set_tlsext_use_srtp() failed");

        goto error;
    }

    return;

error:

    if (DtlsTransport::sslCtx)
    {
        SSL_CTX_free(DtlsTransport::sslCtx);
        DtlsTransport::sslCtx = nullptr;
    }

    LOG_ERROR("SSL context creation failed");
}

void DtlsTransport::GenerateFingerprints()
{
    for (auto& kv : DtlsTransport::string2FingerprintAlgorithm)
    {
        const std::string& algorithmString = kv.first;
        FingerprintAlgorithm algorithm = kv.second;
        uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
        unsigned int size{0};
        char hexFingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
        const EVP_MD* hashFunction;
        int ret;

        switch (algorithm)
        {
            case FingerprintAlgorithm::SHA1:
                hashFunction = EVP_sha1();
                break;

            case FingerprintAlgorithm::SHA224:
                hashFunction = EVP_sha224();
                break;

            case FingerprintAlgorithm::SHA256:
                hashFunction = EVP_sha256();
                break;

            case FingerprintAlgorithm::SHA384:
                hashFunction = EVP_sha384();
                break;

            case FingerprintAlgorithm::SHA512:
                hashFunction = EVP_sha512();
                break;

            default:
                LOG_ERROR("unknown algorithm");
        }

        ret = X509_digest(DtlsTransport::certificate, hashFunction, binaryFingerprint, &size);

        if (ret == 0)
        {
            LOG_ERROR("X509_digest() failed");
            LOG_ERROR("Fingerprints generation failed");
        }

        // Convert to hexadecimal format in uppercase with colons.
        for (unsigned int i{0}; i < size; ++i)
        {
            std::snprintf(hexFingerprint + (i * 3), 4, "%.2X:", binaryFingerprint[i]);
        }
        hexFingerprint[(size * 3) - 1] = '\0';

        LOG_ERROR("{} fingerprint: {}", algorithmString, hexFingerprint);

        // Store it in the vector.
        DtlsTransport::Fingerprint fingerprint;

        fingerprint.algorithm = DtlsTransport::GetFingerprintAlgorithm(algorithmString);
        fingerprint.value = hexFingerprint;

        DtlsTransport::localFingerprints.push_back(fingerprint);
    }
}

/* Instance methods. */

DtlsTransport::DtlsTransport(Listener* listener) : listener(listener)
{
    /* Set SSL. */

    this->ssl = SSL_new(DtlsTransport::sslCtx);

    if (!this->ssl)
    {
        LOG_OPENSSL_ERROR("SSL_new() failed");

        goto error;
    }

    // Set this as custom data.
    SSL_set_ex_data(this->ssl, 0, static_cast<void*>(this));

    this->sslBioFromNetwork = BIO_new(BIO_s_mem());

    if (!this->sslBioFromNetwork)
    {
        LOG_OPENSSL_ERROR("BIO_new() failed");

        SSL_free(this->ssl);

        goto error;
    }

    this->sslBioToNetwork = BIO_new(BIO_s_mem());

    if (!this->sslBioToNetwork)
    {
        LOG_OPENSSL_ERROR("BIO_new() failed");

        BIO_free(this->sslBioFromNetwork);
        SSL_free(this->ssl);

        goto error;
    }

    SSL_set_bio(this->ssl, this->sslBioFromNetwork, this->sslBioToNetwork);

    // Set the MTU so that we don't send packets that are too large with no fragmentation.
    SSL_set_mtu(this->ssl, DtlsMtu);
    DTLS_set_link_mtu(this->ssl, DtlsMtu);

    // Set callback handler for setting DTLS timer interval.
    DTLS_set_timer_cb(this->ssl, onSslDtlsTimer);

    return;

error:

    // NOTE: At this point SSL_set_bio() was not called so we must free BIOs as
    // well.
    if (this->sslBioFromNetwork)
        BIO_free(this->sslBioFromNetwork);

    if (this->sslBioToNetwork)
        BIO_free(this->sslBioToNetwork);

    if (this->ssl)
        SSL_free(this->ssl);

    // NOTE: If this is not catched by the caller the program will abort, but
    // this should never happen.
    LOG_ERROR("DtlsTransport instance creation failed");
}

DtlsTransport::~DtlsTransport()
{
    if (IsRunning())
    {
        // Send close alert to the peer.
        SSL_shutdown(this->ssl);
        SendPendingOutgoingDtlsData();
    }

    if (this->ssl)
    {
        SSL_free(this->ssl);

        this->ssl = nullptr;
        this->sslBioFromNetwork = nullptr;
        this->sslBioToNetwork = nullptr;
    }
}

void DtlsTransport::Dump() const
{
    std::string state{"new"};
    std::string role{"none "};

    switch (this->state)
    {
        case DtlsState::CONNECTING:
            state = "connecting";
            break;
        case DtlsState::CONNECTED:
            state = "connected";
            break;
        case DtlsState::FAILED:
            state = "failed";
            break;
        case DtlsState::CLOSED:
            state = "closed";
            break;
        default:;
    }

    switch (this->localRole)
    {
        case Role::AUTO:
            role = "auto";
            break;
        case Role::SERVER:
            role = "server";
            break;
        case Role::CLIENT:
            role = "client";
            break;
        default:;
    }

    LOG_INFO("<DtlsTransport>");
    LOG_INFO("  state           : {}", state);
    LOG_INFO("  role            : {}", role);
    LOG_INFO("  handshake done: : {}", this->handshakeDone ? "yes" : "no");
    LOG_INFO("</DtlsTransport>");
}

void DtlsTransport::Run(Role localRole)
{
    if (localRole != Role::CLIENT && localRole != Role::SERVER)
    {
        LOG_ERROR("local DTLS role must be 'client' or 'server'");
        return;
    }

    const Role previousLocalRole = this->localRole;

    if (localRole == previousLocalRole)
    {
        LOG_ERROR("same local DTLS role provided, doing nothing");
        return;
    }

    // If the previous local DTLS role was 'client' or 'server' do reset.
    if (previousLocalRole == Role::CLIENT || previousLocalRole == Role::SERVER)
    {
        LOG_DEBUG("resetting DTLS due to local role change");

        Reset();
    }

    // Update local role.
    this->localRole = localRole;

    // Set state and notify the listener.
    this->state = DtlsState::CONNECTING;
    this->listener->OnDtlsTransportConnecting(this);

    switch (this->localRole)
    {
        case Role::CLIENT:
        {
            LOG_DEBUG("running [role:client]");

            SSL_set_connect_state(this->ssl);
            SSL_do_handshake(this->ssl);
            SendPendingOutgoingDtlsData();
            SetTimeout();

            break;
        }

        case Role::SERVER:
        {
            LOG_DEBUG("running [role:server]");

            SSL_set_accept_state(this->ssl);
            SSL_do_handshake(this->ssl);

            break;
        }

        default:
        {
            LOG_WARN("invalid local DTLS role");
        }
    }
}

bool DtlsTransport::SetRemoteFingerprint(const Fingerprint& fingerprint)
{
    if (fingerprint.algorithm == FingerprintAlgorithm::NONE)
    {
        LOG_DEBUG("no fingerprint algorithm provided");
        return false;
    }

    this->remoteFingerprint = fingerprint;

    // The remote fingerpring may have been set after DTLS handshake was done,
    // so we may need to process it now.
    if (this->handshakeDone && this->state != DtlsState::CONNECTED)
    {
        LOG_DEBUG("handshake already done, processing it right now");
        return ProcessHandshake();
    }

    return true;
}

void DtlsTransport::ProcessDtlsData(const uint8_t* data, size_t len)
{
    int written;
    int read;

    if (!IsRunning())
    {
        LOG_ERROR("cannot process data while not running");

        return;
    }

    // Write the received DTLS data into the sslBioFromNetwork.
    written = BIO_write(this->sslBioFromNetwork, static_cast<const void*>(data), static_cast<int>(len));

    if (written != static_cast<int>(len))
    {
        LOG_WARN("OpenSSL BIO_write() wrote less ({} bytes) than given data ({} bytes)", static_cast<size_t>(written), len);
    }

    // Must call SSL_read() to process received DTLS data.
    read = SSL_read(this->ssl, static_cast<void*>(DtlsTransport::sslReadBuffer), SslReadBufferSize);

    // Send data if it's ready.
    SendPendingOutgoingDtlsData();

    // Check SSL status and return if it is bad/closed.
    if (!CheckStatus(read))
        return;

    // Set/update the DTLS timeout.
    if (!SetTimeout())
        return;

    // Application data received. Notify to the listener.
    if (read > 0)
    {
        // It is allowed to receive DTLS data even before validating remote fingerprint.
        if (!this->handshakeDone)
        {
            LOG_WARN("ignoring application data received while DTLS handshake not done");

            return;
        }

        // Notify the listener.
        this->listener->OnDtlsTransportApplicationDataReceived(this, (uint8_t*)DtlsTransport::sslReadBuffer, static_cast<size_t>(read));
    }
}

void DtlsTransport::SendApplicationData(const uint8_t* data, size_t len)
{
    // We cannot send data to the peer if its remote fingerprint is not validated.
    if (this->state != DtlsState::CONNECTED)
    {
        LOG_WARN("cannot send application data while DTLS is not fully connected");

        return;
    }

    if (len == 0)
    {
        LOG_WARN("ignoring 0 length data");

        return;
    }

    int written;

    written = SSL_write(this->ssl, static_cast<const void*>(data), static_cast<int>(len));

    if (written < 0)
    {
        LOG_OPENSSL_ERROR("SSL_write() failed");

        if (!CheckStatus(written))
            return;
    }
    else if (written != static_cast<int>(len))
    {
        LOG_WARN("OpenSSL SSL_write() wrote less ({} bytes) than given data ({} bytes)", written, len);
    }

    // Send data.
    SendPendingOutgoingDtlsData();
}

void DtlsTransport::Reset()
{
    int ret;

    if (!IsRunning())
        return;

    LOG_WARN("resetting DTLS transport");

    // Stop the DTLS timer.
    // this->timer->Stop();

    // We need to reset the SSL instance so we need to "shutdown" it, but we
    // don't want to send a Close Alert to the peer, so just don't call
    // SendPendingOutgoingDTLSData().
    SSL_shutdown(this->ssl);

    this->localRole = Role::NONE;
    this->state = DtlsState::NEW;
    this->handshakeDone = false;
    this->handshakeDoneNow = false;

    // Reset SSL status.
    // NOTE: For this to properly work, SSL_shutdown() must be called before.
    // NOTE: This may fail if not enough DTLS handshake data has been received,
    // but we don't care so just clear the error queue.
    ret = SSL_clear(this->ssl);

    if (ret == 0)
        ERR_clear_error();
}

inline bool DtlsTransport::CheckStatus(int returnCode)
{
    int err;
    const bool wasHandshakeDone = this->handshakeDone;

    err = SSL_get_error(this->ssl, returnCode);

    switch (err)
    {
        case SSL_ERROR_NONE:
            break;

        case SSL_ERROR_SSL:
            LOG_OPENSSL_ERROR("SSL status: SSL_ERROR_SSL");
            break;

        case SSL_ERROR_WANT_READ:
            break;

        case SSL_ERROR_WANT_WRITE:
            LOG_WARN("SSL status: SSL_ERROR_WANT_WRITE");
            break;

        case SSL_ERROR_WANT_X509_LOOKUP:
            LOG_DEBUG("SSL status: SSL_ERROR_WANT_X509_LOOKUP");
            break;

        case SSL_ERROR_SYSCALL:
            LOG_OPENSSL_ERROR("SSL status: SSL_ERROR_SYSCALL");
            break;

        case SSL_ERROR_ZERO_RETURN:
            break;

        case SSL_ERROR_WANT_CONNECT:
            LOG_WARN("SSL status: SSL_ERROR_WANT_CONNECT");
            break;

        case SSL_ERROR_WANT_ACCEPT:
            LOG_WARN("SSL status: SSL_ERROR_WANT_ACCEPT");
            break;

        default:
            LOG_WARN("SSL status: unknown error");
    }

    // Check if the handshake (or re-handshake) has been done right now.
    if (this->handshakeDoneNow)
    {
        this->handshakeDoneNow = false;
        this->handshakeDone = true;

        // Stop the timer.
        if (timer_task_id != 0)
        {
            simple_rtmp::ttms::instance().del_task(timer_task_id);
            timer_task_id = 0;
        }

        // Process the handshake just once (ignore if DTLS renegotiation).
        if (!wasHandshakeDone && this->remoteFingerprint.algorithm != FingerprintAlgorithm::NONE)
            return ProcessHandshake();

        return true;
    }
    // Check if the peer sent close alert or a fatal error happened.
    else if (((SSL_get_shutdown(this->ssl) & SSL_RECEIVED_SHUTDOWN) != 0) || err == SSL_ERROR_SSL || err == SSL_ERROR_SYSCALL)
    {
        if (this->state == DtlsState::CONNECTED)
        {
            LOG_DEBUG("disconnected");

            Reset();

            // Set state and notify the listener.
            this->state = DtlsState::CLOSED;
            this->listener->OnDtlsTransportClosed(this);
        }
        else
        {
            LOG_WARN("connection failed");

            Reset();

            // Set state and notify the listener.
            this->state = DtlsState::FAILED;
            this->listener->OnDtlsTransportFailed(this);
        }

        return false;
    }
    else
    {
        return true;
    }
}

inline void DtlsTransport::SendPendingOutgoingDtlsData()
{
    if (BIO_eof(this->sslBioToNetwork))
        return;

    int64_t read;
    char* data{nullptr};

    read = BIO_get_mem_data(this->sslBioToNetwork, &data);    // NOLINT

    if (read <= 0)
        return;

    LOG_DEBUG("{} bytes of DTLS data ready to sent to the peer", read);

    // Notify the listener.
    this->listener->OnDtlsTransportSendData(this, reinterpret_cast<uint8_t*>(data), static_cast<size_t>(read));

    // Clear the BIO buffer.
    // NOTE: the (void) avoids the -Wunused-value warning.
    (void)BIO_reset(this->sslBioToNetwork);
}

inline bool DtlsTransport::SetTimeout()
{
    if (!this->ssl)
    {
        LOG_ERROR("this->ssl is not set");
        return false;
    }
    if (this->state != DtlsState::CONNECTING && this->state != DtlsState::CONNECTED)
    {
        LOG_ERROR("invalid DTLS state");
        return false;
    }

    struct timeval dtlsTimeout
    {
        0, 0
    };

    // DTLSv1_get_timeout queries the next DTLS handshake timeout. If there is
    // a timeout in progress, it sets *out to the time remaining and returns
    // one. Otherwise, it returns zero.
    DTLSv1_get_timeout(this->ssl, static_cast<void*>(&dtlsTimeout));    // NOLINT

    uint64_t timeoutMs = (dtlsTimeout.tv_sec * static_cast<uint64_t>(1000)) + (dtlsTimeout.tv_usec / 1000);

    if (timeoutMs == 0)
    {
        LOG_DEBUG("timeout is 0, calling OnTimer() callback directly");

        OnTimer();

        return true;
    }
    else if (timeoutMs < 30000)
    {
        LOG_DEBUG("DTLS timer set in {} ms", timeoutMs);

        std::weak_ptr<DtlsTransport> weak_self = shared_from_this();
        simple_rtmp::ttms::instance().add_task(
            timeoutMs / 1000,
            [weak_self]()
            {
                auto strong_self = weak_self.lock();
                if (strong_self)
                {
                    strong_self->OnTimer();
                }
                return true;
            },
            1);

        return true;
    }
    // NOTE: Don't start the timer again if the timeout is greater than 30 seconds.
    else
    {
        LOG_WARN("DTLS timeout too high ({} ms), resetting DLTS", timeoutMs);

        Reset();

        // Set state and notify the listener.
        this->state = DtlsState::FAILED;
        this->listener->OnDtlsTransportFailed(this);

        return false;
    }
}

inline bool DtlsTransport::ProcessHandshake()
{
    if (!this->handshakeDone)
    {
        LOG_ERROR("handshake not done yet");
        return false;
    }
    if (this->remoteFingerprint.algorithm == FingerprintAlgorithm::NONE)
    {
        LOG_ERROR("remote fingerprint not set");
        return false;
    }

    // Validate the remote fingerprint.
    if (!CheckRemoteFingerprint())
    {
        Reset();

        // Set state and notify the listener.
        this->state = DtlsState::FAILED;
        this->listener->OnDtlsTransportFailed(this);

        return false;
    }

    // Get the negotiated SRTP crypto suite.
    RTC::SrtpSession::CryptoSuite srtpCryptoSuite = GetNegotiatedSrtpCryptoSuite();

    if (srtpCryptoSuite != RTC::SrtpSession::CryptoSuite::NONE)
    {
        // Extract the SRTP keys (will notify the listener with them).
        ExtractSrtpKeys(srtpCryptoSuite);

        return true;
    }

    // NOTE: We assume that "use_srtp" DTLS extension is required even if
    // there is no audio/video.
    LOG_WARN("SRTP crypto suite not negotiated");

    Reset();

    // Set state and notify the listener.
    this->state = DtlsState::FAILED;
    this->listener->OnDtlsTransportFailed(this);

    return false;
}

inline bool DtlsTransport::CheckRemoteFingerprint()
{
    if (this->remoteFingerprint.algorithm == FingerprintAlgorithm::NONE)
    {
        LOG_ERROR("remote fingerprint not set");
        return false;
    }

    X509* certificate;
    uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
    unsigned int size{0};
    char hexFingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
    const EVP_MD* hashFunction;
    int ret;

    certificate = SSL_get_peer_certificate(this->ssl);

    if (!certificate)
    {
        LOG_WARN("no certificate was provided by the peer");

        return false;
    }

    switch (this->remoteFingerprint.algorithm)
    {
        case FingerprintAlgorithm::SHA1:
            hashFunction = EVP_sha1();
            break;

        case FingerprintAlgorithm::SHA224:
            hashFunction = EVP_sha224();
            break;

        case FingerprintAlgorithm::SHA256:
            hashFunction = EVP_sha256();
            break;

        case FingerprintAlgorithm::SHA384:
            hashFunction = EVP_sha384();
            break;

        case FingerprintAlgorithm::SHA512:
            hashFunction = EVP_sha512();
            break;

        default:
            LOG_ERROR("unknown algorithm");
    }

    // Compare the remote fingerprint with the value given via signaling.
    ret = X509_digest(certificate, hashFunction, binaryFingerprint, &size);

    if (ret == 0)
    {
        LOG_ERROR("X509_digest() failed");

        X509_free(certificate);

        return false;
    }

    // Convert to hexadecimal format in uppercase with colons.
    for (unsigned int i{0}; i < size; ++i)
    {
        std::snprintf(hexFingerprint + (i * 3), 4, "%.2X:", binaryFingerprint[i]);
    }
    hexFingerprint[(size * 3) - 1] = '\0';

    if (this->remoteFingerprint.value != hexFingerprint)
    {
        LOG_ERROR("fingerprint in the remote certificate ({}) does not match the announced one ({})", hexFingerprint, this->remoteFingerprint.value.c_str());
        X509_free(certificate);
        return false;
    }

    LOG_DEBUG("valid remote fingerprint");

    // Get the remote certificate in PEM format.

    BIO* bio = BIO_new(BIO_s_mem());

    // Ensure the underlying BUF_MEM structure is also freed.
    // NOTE: Avoid stupid "warning: value computed is not used [-Wunused-value]" since
    // BIO_set_close() always returns 1.
    (void)BIO_set_close(bio, BIO_CLOSE);

    ret = PEM_write_bio_X509(bio, certificate);

    if (ret != 1)
    {
        LOG_OPENSSL_ERROR("PEM_write_bio_X509() failed");

        X509_free(certificate);
        BIO_free(bio);

        return false;
    }

    BUF_MEM* mem;

    BIO_get_mem_ptr(bio, &mem);    // NOLINT[cppcoreguidelines-pro-type-cstyle-cast]

    if (!mem || !mem->data || mem->length == 0u)
    {
        LOG_OPENSSL_ERROR("BIO_get_mem_ptr() failed");

        X509_free(certificate);
        BIO_free(bio);

        return false;
    }

    this->remoteCert = std::string(mem->data, mem->length);

    X509_free(certificate);
    BIO_free(bio);

    return true;
}

inline void DtlsTransport::ExtractSrtpKeys(RTC::SrtpSession::CryptoSuite srtpCryptoSuite)
{
    size_t srtpKeyLength{0};
    size_t srtpSaltLength{0};
    size_t srtpMasterLength{0};

    switch (srtpCryptoSuite)
    {
        case RTC::SrtpSession::CryptoSuite::AEAD_AES_256_GCM:
        {
            srtpKeyLength = SrtpAesGcm256MasterKeyLength;
            srtpSaltLength = SrtpAesGcm256MasterSaltLength;
            srtpMasterLength = SrtpAesGcm256MasterLength;

            break;
        }

        case RTC::SrtpSession::CryptoSuite::AEAD_AES_128_GCM:
        {
            srtpKeyLength = SrtpAesGcm128MasterKeyLength;
            srtpSaltLength = SrtpAesGcm128MasterSaltLength;
            srtpMasterLength = SrtpAesGcm128MasterLength;

            break;
        }

        case RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80:
        case RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_32:
        {
            srtpKeyLength = SrtpMasterKeyLength;
            srtpSaltLength = SrtpMasterSaltLength;
            srtpMasterLength = SrtpMasterLength;

            break;
        }

        default:
        {
            LOG_ERROR("unknown SRTP crypto suite");
        }
    }

    auto* srtpMaterial = new uint8_t[srtpMasterLength * 2];
    uint8_t* srtpLocalKey{nullptr};
    uint8_t* srtpLocalSalt{nullptr};
    uint8_t* srtpRemoteKey{nullptr};
    uint8_t* srtpRemoteSalt{nullptr};
    auto* srtpLocalMasterKey = new uint8_t[srtpMasterLength];
    auto* srtpRemoteMasterKey = new uint8_t[srtpMasterLength];
    int ret;

    ret = SSL_export_keying_material(this->ssl, srtpMaterial, srtpMasterLength * 2, "EXTRACTOR-dtls_srtp", 19, nullptr, 0, 0);

    if (ret == 0)
    {
        LOG_ERROR("SSL_export_keying_material() failed");
        return;
    }

    switch (this->localRole)
    {
        case Role::SERVER:
        {
            srtpRemoteKey = srtpMaterial;
            srtpLocalKey = srtpRemoteKey + srtpKeyLength;
            srtpRemoteSalt = srtpLocalKey + srtpKeyLength;
            srtpLocalSalt = srtpRemoteSalt + srtpSaltLength;

            break;
        }

        case Role::CLIENT:
        {
            srtpLocalKey = srtpMaterial;
            srtpRemoteKey = srtpLocalKey + srtpKeyLength;
            srtpLocalSalt = srtpRemoteKey + srtpKeyLength;
            srtpRemoteSalt = srtpLocalSalt + srtpSaltLength;

            break;
        }

        default:
        {
            LOG_ERROR("no DTLS role set");
        }
    }

    // Create the SRTP local master key.
    std::memcpy(srtpLocalMasterKey, srtpLocalKey, srtpKeyLength);
    std::memcpy(srtpLocalMasterKey + srtpKeyLength, srtpLocalSalt, srtpSaltLength);
    // Create the SRTP remote master key.
    std::memcpy(srtpRemoteMasterKey, srtpRemoteKey, srtpKeyLength);
    std::memcpy(srtpRemoteMasterKey + srtpKeyLength, srtpRemoteSalt, srtpSaltLength);

    // Set state and notify the listener.
    this->state = DtlsState::CONNECTED;
    this->listener->OnDtlsTransportConnected(this, srtpCryptoSuite, srtpLocalMasterKey, srtpMasterLength, srtpRemoteMasterKey, srtpMasterLength, this->remoteCert);

    delete[] srtpMaterial;
    delete[] srtpLocalMasterKey;
    delete[] srtpRemoteMasterKey;
}

inline RTC::SrtpSession::CryptoSuite DtlsTransport::GetNegotiatedSrtpCryptoSuite()
{
    RTC::SrtpSession::CryptoSuite negotiatedSrtpCryptoSuite = RTC::SrtpSession::CryptoSuite::NONE;

    // Ensure that the SRTP crypto suite has been negotiated.
    // NOTE: This is a OpenSSL type.
    SRTP_PROTECTION_PROFILE* sslSrtpCryptoSuite = SSL_get_selected_srtp_profile(this->ssl);

    if (!sslSrtpCryptoSuite)
        return negotiatedSrtpCryptoSuite;

    // Get the negotiated SRTP crypto suite.
    for (auto& srtpCryptoSuite : DtlsTransport::srtpCryptoSuites)
    {
        SrtpCryptoSuiteMapEntry* cryptoSuiteEntry = std::addressof(srtpCryptoSuite);

        if (std::strcmp(sslSrtpCryptoSuite->name, cryptoSuiteEntry->name) == 0)
        {
            LOG_DEBUG("chosen SRTP crypto suite: {}", cryptoSuiteEntry->name);

            negotiatedSrtpCryptoSuite = cryptoSuiteEntry->cryptoSuite;
        }
    }

    if (negotiatedSrtpCryptoSuite == RTC::SrtpSession::CryptoSuite::NONE)
    {
        LOG_ERROR("chosen SRTP crypto suite is not an available one");
    }

    return negotiatedSrtpCryptoSuite;
}

inline void DtlsTransport::OnSslInfo(int where, int ret)
{
    const int w = where & -SSL_ST_MASK;
    const char* role;

    if ((w & SSL_ST_CONNECT) != 0)
        role = "client";
    else if ((w & SSL_ST_ACCEPT) != 0)
        role = "server";
    else
        role = "undefined";

    if ((where & SSL_CB_LOOP) != 0)
    {
        LOG_DEBUG("[role:{}, action:'{}']", role, SSL_state_string_long(this->ssl));
    }
    else if ((where & SSL_CB_ALERT) != 0)
    {
        const char* alertType;

        switch (*SSL_alert_type_string(ret))
        {
            case 'W':
                alertType = "warning";
                break;

            case 'F':
                alertType = "fatal";
                break;

            default:
                alertType = "undefined";
        }

        if ((where & SSL_CB_READ) != 0)
        {
            LOG_WARN("received DTLS {} alert: {}", alertType, SSL_alert_desc_string_long(ret));
        }
        else if ((where & SSL_CB_WRITE) != 0)
        {
            LOG_DEBUG("sending DTLS {} alert: {}", alertType, SSL_alert_desc_string_long(ret));
        }
        else
        {
            LOG_DEBUG("DTLS {} alert: {}", alertType, SSL_alert_desc_string_long(ret));
        }
    }
    else if ((where & SSL_CB_EXIT) != 0)
    {
        if (ret == 0)
            LOG_DEBUG("[role:{}, failed:'{}']", role, SSL_state_string_long(this->ssl));
        else if (ret < 0)
            LOG_DEBUG("role: {}, waiting:'{}']", role, SSL_state_string_long(this->ssl));
    }
    else if ((where & SSL_CB_HANDSHAKE_START) != 0)
    {
        LOG_DEBUG("DTLS handshake start");
    }
    else if ((where & SSL_CB_HANDSHAKE_DONE) != 0)
    {
        LOG_DEBUG("DTLS handshake done");

        this->handshakeDoneNow = true;
    }

    // NOTE: checking SSL_get_shutdown(this->ssl) & SSL_RECEIVED_SHUTDOWN here upon
    // receipt of a close alert does not work (the flag is set after this callback).
}

inline void DtlsTransport::OnTimer()
{
    // Workaround for https://github.com/openssl/openssl/issues/7998.
    if (this->handshakeDone)
    {
        LOG_DEBUG("handshake is done so return");

        return;
    }

    // DTLSv1_handle_timeout is called when a DTLS handshake timeout expires.
    // If no timeout had expired, it returns 0. Otherwise, it retransmits the
    // previous flight of handshake messages and returns 1. If too many timeouts
    // had expired without progress or an error occurs, it returns -1.
    auto ret = DTLSv1_handle_timeout(this->ssl);

    if (ret == 1)
    {
        // If required, send DTLS data.
        SendPendingOutgoingDtlsData();

        // Set the DTLS timer again.
        SetTimeout();
    }
    else if (ret == -1)
    {
        LOG_WARN("DTLSv1_handle_timeout() failed");

        Reset();

        // Set state and notify the listener.
        this->state = DtlsState::FAILED;
        this->listener->OnDtlsTransportFailed(this);
    }
}
}    // namespace RTC
