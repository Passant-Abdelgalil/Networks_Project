/*
 * Node.h
 *
 *  Created on: Dec 8, 2022
 *      Author: passant-abdelgalil
 */

#ifndef NODE_H_
#define NODE_H_

#include <omnetpp.h>

using namespace omnetpp;

class Node : public cSimpleModule {
public:
    std::string get_next_message();

    protected:
    bool sender = false;    // flag to act upon, by default it's false
                            // set to true when the coordinator activate the node

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
};


#endif /* NODE_H_ */
