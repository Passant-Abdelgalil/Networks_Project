/*
 * Coordinator.h
 *
 *  Created on: Dec 9, 2022
 *      Author: passant-abdelgalil
 */

#ifndef COORDINATOR_H_
#define COORDINATOR_H_
#include <omnetpp.h>
using namespace omnetpp;


class Coordinator : public cSimpleModule{
protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
};


#endif /* COORDINATOR_H_ */
