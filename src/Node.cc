/*
 * Node.cc
 *
 *  Created on: Dec 8, 2022
 *      Author: passant-abdelgalil
 */


#include "Node.h"
#include "message_m.h"
Define_Module(Node);

void Node::initialize(){

}

void Node::handleMessage(cMessage *msg)
{
    Message_Base *message;
    // try to cast the message to Message_Base
    // this will succeed if the message is sent by a node
    try {
        message =  check_and_cast<Message_Base *>(msg);
    }
    catch(...) {
        // otherwise, this message is sent by the coordinator
        // so initialize this node as the sender
        sender = true;

        // read from the input file the next message to send
        std::string next_message = get_next_message();
        message = new Message_Base("node message");

        // fill the message fields
        message->setAck_Num(0);
        message->setHeader(0);
        message->setTrailer('0');
        message->setFrame_Type(Data);
        message->setM_Payload(next_message.c_str());

        // send the message delayed
        sendDelayed(message, 1, "out");
        // delete old msg
        delete msg;
        // return from this function
        return;
    }

}

std::string Node::get_next_message(){
    // TODO: Implement File Parser read function
return "Hello";
}
