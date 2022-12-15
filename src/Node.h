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
#include <fstream>
#include "message_m.h"

using namespace omnetpp;

enum error_code {
    NO_ERRORs,
    DELAY,
    DUP,
    DUP_DELAY,
    LOSS,
    LOSS_BOTH,
    MOD,
    MOD_DELAY,
    MOD_DUP,
    MOD_DUP_DELAY,
};


std::map<std::string, error_code> error_codes = {
        {"0000", NO_ERRORs},
        {"0001", DELAY},
        {"0010", DUP},
        {"0011", DUP_DELAY},
        {"0100", LOSS},
        {"0101", LOSS},
        {"0110", LOSS},
        {"0111", LOSS},
        {"1000", MOD},
        {"1001", MOD_DELAY},
        {"1010", MOD_DUP},
        {"1011", MOD_DUP_DELAY},
        {"1100", LOSS},
        {"1101", LOSS},
        {"1110", LOSS},
        {"1111", LOSS},
};

enum log_info_type {
    UPON_READING,
    BEFORE_TRANS,
    TIMEOUT_EVENT,
    CONTROL_FRAME,
};


class Node : public cSimpleModule {
public:
    std::pair<std::string, std::string> get_next_message();
    void start_protocol();
    void handle_frame_arrival(Message_Base *frame);
    void handle_network_layer_ready();
    void handle_checksum_err(cMessage *frame);
    void handle_timeout(int frame_seq);
    void send_data(std::string payload, std::string error, double sendingOffsetTime = 0);
    void send_control(Message_Base *msg);
    void start_timer(int frame_seq_num, double delayTime = 0);
    void stop_timer(int frame_seq_num);
    void inc(int& frame_seq_num);
    void update_window(int ack_num);
    void acknowledge_frame(int ack_num);

    void apply_error_and_send(std::string error, double time, Message_Base *msg);
    double delay_frame(double time, Message_Base *msg);
    double duplicate_frame(double time, Message_Base *msg);
    int modify_frame(double time, Message_Base *msg);

    void log_to_file(log_info_type info_type, double event_time, char nodeId,
        Message_Base* msg, bool lost, std::string error="", int modified_bit=-1,
         int duplicate_version=0,  double delayInterval=0);


    std::string frame_packet(std::string payload);
    void error_detection(Message_Base *msg);

    void tokenize(std::string const &str, const char delim,
            std::vector<std::string> &out);

    void readInputFile();

    protected:
    bool sender = false;    // flag to act upon, by default it's false
                            // set to true when the coordinator activate the node

    std::vector<std::string> messages;
    std::vector<std::string> errors;
    bool initialState = true;

    bool end_communication = false;
    bool network_layer_enabled;
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

    std::ofstream output_file;
    std::ifstream input_file;

    // GO Back N Fields
    int MAX_SEQ = 1;
    int next_frame_to_send_seq_num;
    int ack_expected;
    int frame_expected;
    int nbuffered = 0;
    int line = 0;
    int sentMessagesNumber = 0;
    char nodeId;
    int numOfLines;
    std::vector<std::pair<std::string,Message_Base *> > frames_buffer;
    std::vector<cMessage *> timeouts_buffer;
};


#endif /* NODE_H_ */
