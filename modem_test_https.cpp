/*
  HTTPS test helper that supports multiple TinyGSM forks.
  Tries several known secure-client headers via __has_include and uses the
  first one found. If none are found, it prints a clear diagnostic.
*/

#include "modem_test.h"
#include "modem_manager.h"
#include <TinyGsmClient.h>

#if defined(__has_include)
  #if __has_include(<TinyGsmClientSecure.h>)
    #include <TinyGsmClientSecure.h>
    #define TINY_GSM_SECURE_AVAILABLE 1
    typedef TinyGsmClientSecure TinyGsmSecureClient_t;
  #elif __has_include(<TinyGsmClientTls.h>)
    #include <TinyGsmClientTls.h>
    #define TINY_GSM_SECURE_AVAILABLE 1
    typedef TinyGsmClientTls TinyGsmSecureClient_t;
  #elif __has_include(<TinyGsmClientSSL.h>)
    #include <TinyGsmClientSSL.h>
    #define TINY_GSM_SECURE_AVAILABLE 1
    typedef TinyGsmClientSSL TinyGsmSecureClient_t;
  #else
    #define TINY_GSM_SECURE_AVAILABLE 0
  #endif
#else
  #define TINY_GSM_SECURE_AVAILABLE 0
#endif

// NOTE: no default parameter values here — defaults exist only in the header.
bool modem_test_https_get(const char *apn, const char *host, const char *path, uint16_t port) {
  Serial.printf("[MODEM-HTTPS] Attempting GPRS attach with APN='%s'\n", apn);
  TinyGsm &m = modem_get();

  if (!m.gprsConnect(apn, "", "")) {
    Serial.println("[MODEM-HTTPS] gprsConnect() returned: FAIL");
    return false;
  }
  Serial.println("[MODEM-HTTPS] gprsConnect() returned: OK");

#if TINY_GSM_SECURE_AVAILABLE
  Serial.println("[MODEM-HTTPS] TinyGSM secure client found - attempting TLS connect");

  TinyGsmSecureClient_t client(m);

  // For quick testing we disable cert validation; remove or replace for production
  #if defined(TinyGsmClientSecure) || defined(TinyGsmClientTls) || defined(TinyGsmClientSSL)
    client.setInsecure();
  #endif

  Serial.printf("[MODEM-HTTPS] Connecting to %s:%u ...\n", host, port);
  if (!client.connect(host, port)) {
    Serial.println("[MODEM-HTTPS] SSL/TCP connect failed");
    m.gprsDisconnect();
    return false;
  }

  // Send HTTP/1.1 request with Host header
  String req = String("GET ") + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
  client.print(req);

  unsigned long start = millis();
  while (client.available() == 0 && millis() - start < 8000) { delay(10); }

  start = millis();
  int printed = 0;
  Serial.println("[MODEM-HTTPS] Response (first bytes):");
  while (client.available() && millis() - start < 7000 && printed < 1024) {
    char c = client.read();
    Serial.write(c);
    printed++;
  }
  Serial.println();
  client.stop();
  m.gprsDisconnect();
  Serial.println("[MODEM-HTTPS] Done, disconnected GPRS.");
  return true;
#else
  Serial.println("[MODEM-HTTPS] TinyGSM secure-client header not found in the installed library.");
  Serial.println("[MODEM-HTTPS] If you're using a TinyGSM fork, tell me the fork name and header path.");
  Serial.println("[MODEM-HTTPS] To enable HTTPS tests, either:");
  Serial.println("  * Install/update TinyGSM via Arduino Library Manager (recommended).");
  Serial.println("  * Or tell me which TinyGSM fork you use so I can adapt includes.");
  Serial.println("[MODEM-HTTPS] For now HTTPS test is skipped.");
  m.gprsDisconnect();
  return false;
#endif
}