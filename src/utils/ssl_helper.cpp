#include "utils/ssl_helper.hpp"
#include "utils/logger.hpp"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <sys/socket.h>
#include <poll.h>
#include <chrono>

namespace titanshare {

SslHelper& SslHelper::instance() {
    static SslHelper inst;
    return inst;
}

SslHelper::SslHelper() {
    initOpenSsl();
}

SslHelper::~SslHelper() {
    if (m_cert) X509_free(m_cert);
    if (m_pkey) EVP_PKEY_free(m_pkey);
    if (m_ctx)  SSL_CTX_free(m_ctx);
}

void SslHelper::initOpenSsl() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD* method = TLS_server_method();
    m_ctx = SSL_CTX_new(method);

    if (!m_ctx) {
        Logger::error("SSL", "Failed to create SSL_CTX");
        return;
    }

    // Set TLS 1.2 / 1.3 minimum
    SSL_CTX_set_min_proto_version(m_ctx, TLS1_2_VERSION);

    if (!generateSelfSignedCert()) {
        Logger::error("SSL", "Failed to generate in-memory TLS certificate");
    } else {
        Logger::info("SSL", "🔒 Ephemeral TLS 1.3/1.2 Certificate generated successfully");
    }
}

bool SslHelper::generateSelfSignedCert() {
    // Generate 2048-bit RSA key pair
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!pctx) return false;

    if (EVP_PKEY_keygen_init(pctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0 ||
        EVP_PKEY_keygen(pctx, &m_pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return false;
    }
    EVP_PKEY_CTX_free(pctx);

    m_cert = X509_new();
    if (!m_cert) return false;

    ASN1_INTEGER_set(X509_get_serialNumber(m_cert), 1);
    X509_gmtime_adj(X509_get_notBefore(m_cert), 0);
    X509_gmtime_adj(X509_get_notAfter(m_cert), 315360000L); // 10 years

    X509_set_pubkey(m_cert, m_pkey);

    X509_NAME* name = X509_get_subject_name(m_cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"TitanShare Daemon", -1, -1, 0);
    X509_set_issuer_name(m_cert, name);

    if (!X509_sign(m_cert, m_pkey, EVP_sha256())) return false;

    if (SSL_CTX_use_certificate(m_ctx, m_cert) <= 0 ||
        SSL_CTX_use_PrivateKey(m_ctx, m_pkey) <= 0) {
        return false;
    }

    return true;
}

SSL* SslHelper::acceptTls(int clientFd) {
    if (!m_ctx) return nullptr;

    SSL* ssl = SSL_new(m_ctx);
    if (!ssl) return nullptr;

    SSL_set_fd(ssl, clientFd);

    // The client fd is non-blocking (see TcpServer::acceptConnection), so a
    // single SSL_accept() may legitimately return SSL_ERROR_WANT_READ /
    // SSL_ERROR_WANT_WRITE mid-handshake. Loop with poll() until the
    // handshake completes or the deadline expires; this also lets a non-TLS
    // client (raw fallback) fail fast instead of hanging.
    struct pollfd pfd{};
    pfd.fd = clientFd;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(5000);

    for (;;) {
        int ret = SSL_accept(ssl);
        if (ret == 1) {
            Logger::info("SSL", "🔒 TLS Handshake accepted on client fd " +
                          std::to_string(clientFd));
            return ssl;
        }

        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
            pfd.events = POLLIN;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            pfd.events = POLLOUT;
        } else {
            // Real failure or a non-TLS connection.
            SSL_free(ssl);
            return nullptr;
        }

        const auto now = std::chrono::steady_clock::now();
        const long remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now).count();
        if (remaining <= 0) {
            SSL_free(ssl);
            return nullptr;
        }

        if (poll(&pfd, 1, static_cast<int>(remaining)) <= 0) {
            SSL_free(ssl);
            return nullptr;
        }
    }
}

void SslHelper::closeTls(SSL* ssl) {
    if (!ssl) return;
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

} // namespace titanshare
