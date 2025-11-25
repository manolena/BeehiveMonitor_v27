#ifndef MODEM_TEST_H
#define MODEM_TEST_H

#include <Arduino.h>

// Print current modem status (operator, signal, registration)
void modem_test_status();

// Attempt a GPRS attach and perform a simple HTTP GET.
// Returns true on success, false on failure.
bool modem_test_http_get(const char *apn = "internet", const char *user = "", const char *pass = "",
                         const char *host = "example.com", uint16_t port = 80, const char *path = "/");

// HTTPS test (TLS) helper prototype. Implementation lives in modem_test_https.cpp.
bool modem_test_https_get(const char *apn = "internet", const char *host = "httpbin.org", const char *path = "/", uint16_t port = 443);

#endif // MODEM_TEST_H