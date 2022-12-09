/*
 * Coordinator.cc
 *
 *  Created on: Dec 9, 2022
 *      Author: passant-abdelgalil
 */


#include "Coordinator.h"


Define_Module(Coordinator);

void Coordinator::initialize(){
    cMessage *msg = new cMessage("start");
    // send node0 to be sender
    send(msg, "out_ports", 0);
}
void Coordinator::handleMessage(cMessage *msg){

}
