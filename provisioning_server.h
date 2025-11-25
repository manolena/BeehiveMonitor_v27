#ifndef PROVISIONING_SERVER_H
#define PROVISIONING_SERVER_H

// Initialize provisioning server (serves HTML form on port 8080)
void provisioning_init();
void provisioning_loop(); // call from main loop periodically

#endif // PROVISIONING_SERVER_H