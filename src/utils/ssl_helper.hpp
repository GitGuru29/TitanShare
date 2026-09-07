#pragma once
/*
 * TitanShare Daemon — OpenSSL Helper
 * Generates an in-memory self-signed certificate and manages TLS server contexts.
 */

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include <memory>
#include <string>

namespace titanshare {

class SslHelper {
public:
    static SslHelper& instance();

    SslHelper(const SslHelper&) = delete;
    SslHelper& operator=(const SslHelper&) = delete;

    SSL_CTX* getContext() const { return m_ctx; }

    // Wrap an accepted socket descriptor in TLS (returns nullptr on error or non-TLS fallback)
    SSL* acceptTls(int clientFd);

    // Free an SSL session cleanly
    void closeTls(SSL* ssl);

private:
    SslHelper();
    ~SslHelper();

    void initOpenSsl();
    bool generateSelfSignedCert();

    SSL_CTX* m_ctx = nullptr;
    EVP_PKEY* m_pkey = nullptr;
    X509* m_cert = nullptr;
};

} // namespace titanshare
