#ifndef SERVICE_CHAIN_H
#define SERVICE_CHAIN_H

#include "ice_service.h"

#define MAX_SERVICES 10

class ServiceChain {
public:
    ServiceChain() : _count(0) {}

    void add(IceService& service) {
        if (_count < MAX_SERVICES) {
            _services[_count++] = &service;
        }
    }

    void loop() {
        for (int i = 0; i < _count; i++) {
            _services[i]->loop();
        }
    }

private:
    IceService* _services[MAX_SERVICES];
    int _count;
};

#endif