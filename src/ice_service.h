#ifndef ICE_SERVICE_H
#define ICE_SERVICE_H

class IceService {
public:
    virtual void loop() = 0;
    virtual ~IceService() {}
};

#endif