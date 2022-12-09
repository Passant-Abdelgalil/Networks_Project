/*
 * Node.h
 *
 *  Created on: Dec 8, 2022
 *      Author: passant-abdelgalil
 */

#ifndef NODE_H_
#define NODE_H_
#include <omnetpp.h>
#include <algorithm>
#include <vector>
#include "message_m.h"

using namespace omnetpp;

enum error_code {
    NO_ERROR,           // 0000
    DELAY,              // 0001
    DUP,                // 0010
    DUP_DELAY,          // 0011
    LOSS,               // 0100 , 1100 , 1101
    LOSS_BOTH,          // 0101 , 0111 , 1110 , 1111
    MOD,                // 1000
    MOD_DELAY,          // 1001
    MOD_DUP,            // 1010
    MOD_DUP_DELAY,      // 1011
};


class Node : public cSimpleModule {
public:
    std::pair<error_code, std::string> get_next_message();
    void start_protocol();
    bool handle_frame_arrival(Message_Base *frame);
    void handle_network_layer_ready();
    void handle_checksum_err(cMessage *frame);
    void handle_timeout(int frame_seq);
    void send_data(std::string payload, error_code error);
    void send_control(Message_Base *msg);
    void start_timer(int frame_seq_num);
    void stop_timer(int frame_seq_num);
    void inc(int& frame_seq_num);
    void update_window(int ack_num);
    void acknowledge_frame(int ack_num);

    void apply_error(error_code error, double time, Message_Base *msg);
    double delay_frame(double time, Message_Base *msg);
    double duplicate_frame(double time, Message_Base *msg);
    std::string modify_frame(double time, Message_Base *msg);


    std::string frame_packet(std::string payload);
    void error_detection(Message_Base *msg);

    protected:
    bool sender = false;    // flag to act upon, by default it's false
                            // set to true when the coordinator activate the node

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

    // GO Back N Fields
    int MAX_SEQ = 1;
    int next_frame_to_send_seq_num;
    int ack_expected;
    int frame_expected;
    int nbuffered = 0;

    std::vector<std::pair<error_code,Message_Base *> > frames_buffer;
    std::vector<cMessage *> timeouts_buffer;
};


#endif /* NODE_H_ */
