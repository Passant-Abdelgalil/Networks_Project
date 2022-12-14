/*
 * Coordinator.cc
 *
 *  Created on: Dec 9, 2022
 *      Author: passant-abdelgalil
 */


#include "Coordinator.h"
#include<fstream>
#include<string>
#include<unistd.h>
#include<stdio.h>
Define_Module(Coordinator);

//void Coordinator::initialize(){
//    cMessage *msg = new cMessage("start");
//    // send node0 to be sender
//    send(msg, "out_ports", 0);
//}

void Coordinator::initialize()
{
    std::string line;
    std::ifstream filestream("../src/coordinator.txt");

//    EV << " File is open = " << filestream.is_open() <<endl;
    if(filestream.is_open()) {
        while (getline(filestream, line)) {

//            EV << "Line = " << line << endl;
            int* data;
            data = tokenize(line);
            int nodeId = data[0]; //atoi(line.c_str());
            int startTime = data[1]; //atoi(line.c_str() + 1);

            EV << "nodeNumber From Coordinator = " << nodeId << endl;
            EV << "startingTime From Coordinator  = " << startTime << endl;

            cMessage *msg = new cMessage("Coordinator start message");
            msg->addPar("nodeId");
            msg->par("nodeId").setLongValue(nodeId);

            msg->addPar("startTime");
            msg->par("startTime").setLongValue(startTime);

            if(nodeId == 0)
                send(msg, "out_ports", 0);
            else if(nodeId == 1)
                send(msg, "out_ports", 1);
        }

    filestream.close();
    }
}

int * Coordinator::tokenize(std::string s, std::string del)
{
    int start, end = -1*del.size(), i = 0;
    int data [2];

    do {
        start = end + del.size();
        end = s.find(del, start);
        data[i] = atoi(s.substr(start, end - start).c_str());
        i++;
    } while (end != -1);

    return data;
}

void Coordinator::handleMessage(cMessage *msg){

}
