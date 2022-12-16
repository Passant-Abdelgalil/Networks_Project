/*
 * Node.cc
 *
 *  Created on: Dec 8, 2022
 *      Author: passant-abdelgalil
 */


#include "Node.h"
#include "message_m.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#include<fstream>

Define_Module(Node);

void Node::initialize() {
    // sender's parameters
    next_frame_to_send_seq_num = 0;
    ack_expected = 0;
    // receiver's parameter
    frame_expected = 0;
    // number of buffered frames
    nbuffered = 0;
    // enable network layer
    network_layer_enabled = true;

    // set node id
    std::string node_name(getName());
    nodeId = node_name.back();

    // prepare output file for writing logs
    std::string output_file_name = "../src/output";
    output_file_name.push_back(nodeId);
    output_file_name.append(".txt");
    output_file.open(output_file_name);
    EV << "Output File is open = " << output_file.is_open() <<endl;

    // buffers to store pointers to messages
    frames_buffer.resize(MAX_SEQ);
    timeouts_buffer.resize(MAX_SEQ);
}

void Node::handleMessage(cMessage *msg)
{
    // check if this message is from the coordinator
    if(msg->arrivedOn("in_ports", 0)) {

        long startTime = msg->par("startTime").longValue();

        start_protocol();
        delete msg;

        cMessage * self_msg = new cMessage("enable network");
//        scheduleAt(simTime() + static_cast<simtime_t>(getParentModule()->par("PT").doubleValue()) + static_cast<simtime_t>(startTime), self_msg);
        scheduleAt(simTime() + static_cast<simtime_t>(startTime), self_msg);
    }
    else if(strcmp(msg->getName(), "enable network") == 0) {
        if(network_layer_enabled)
            handle_network_layer_ready();
        delete msg;

        if(sentMessagesNumber < numOfLines){
            cMessage * self_msg = new cMessage("enable network");
            scheduleAt(simTime() + static_cast<simtime_t>(getParentModule()->par("PT").doubleValue()), self_msg);
        }
    }
    else if(msg->isSelfMessage()&& msg->hasPar("frame_seq")) { // timeout event
        // get sequence number of the timed out frame
        int frame_seq_num = msg->par("frame_seq");
        handle_timeout(frame_seq_num);
    }
    else  // frame_arrival event
        handle_frame_arrival(dynamic_cast<Message_Base *>(msg));

    if(sender && nbuffered < MAX_SEQ && line < numOfLines)
        network_layer_enabled = true;
    else
        network_layer_enabled = false;

}

void Node::start_protocol() {
    // so initialize this node as the sender
    sender = true;
    // set the WS for this sender protocol instance
    MAX_SEQ = getParentModule()->par("WS").intValue();

    std::string input_file_name = "../src/input";
    input_file_name.push_back(nodeId);
    input_file_name.append(".txt");

    // prepare input file for reading messages
    input_file.open(input_file_name);

    EV << "Input File is open = " << input_file.is_open() <<endl;

    frames_buffer.resize(MAX_SEQ);
    timeouts_buffer.resize(MAX_SEQ);

    readInputFile();
}
void Node::inc(int& frame_seq_num)
{
    frame_seq_num = (frame_seq_num + 1) % (getParentModule()->par("WS").intValue() + 1);
}

void Node::handle_timeout(int frame_seq_num){

    next_frame_to_send_seq_num = frames_buffer[0].second->getHeader();
    int size = nbuffered;
    nbuffered = 0;
    double sendingTime = 0;
    // re-transmit all frames in the buffer
    for (int i = 0; i < size; i++){
        std::pair<std::string, Message_Base*> frame_info = frames_buffer[i];
        std::string payload(frame_info.second->getM_Payload());
        /*
         * all the messages in the sender’s window will be transmitted again as
         * usual with the normal calculations for errors and delays except for
         * the first frame that is the cause for the timeout,
         * it will be sent error free
         * */

        sendingTime = getParentModule()->par("PT").doubleValue() + sendingTime;
        // remove the timeout for this frame if any
        stop_timer(frame_info.second->getHeader());

        if(frame_info.second->getHeader() == frame_seq_num){
            log_to_file(TIMEOUT_EVENT, simTime().dbl(), nodeId, frame_info.second, false);
            send_data(payload, "0000", sendingTime);
        }
        else {
            send_data(payload, frame_info.first,sendingTime);
        }
    }

}


void Node::update_window(int ack_num){
    int index = -1;
    for(int i = 0; i < nbuffered; i++){
        std::pair<std::string, Message_Base*> frame = frames_buffer[i];
        if(!frame.second) continue;
        if(frame.second->getHeader() == ack_num) {
            index = i;
            break;
        }
    }
    // all frames in the window is acknowledged
    if(index == -1){
        for(int i = 0; i < nbuffered; i++){
            sentMessagesNumber++;
            stop_timer(frames_buffer[i].second->getHeader());
            delete frames_buffer[i].second;
        }
        nbuffered = 0;
        return;
    }

    for(int i = 0; i < nbuffered -index; i++){
        if(i < index){
            sentMessagesNumber++;
            stop_timer(frames_buffer[i].second->getHeader());
            delete frames_buffer[i].second;
        }

        frames_buffer[i] = frames_buffer[i+index];
    }

    nbuffered -= index;
}

void Node:: handle_frame_arrival(Message_Base *frame)
{
    switch(frame->getFrame_Type()){
        case ACK:   // this is the sender node
            update_window(frame->getAck_Num());
            break;
        case Data:  // this is the receiver node
            if (frame->getHeader() == frame_expected){
                // send either ACK or NACK
                error_detection(frame);
            }
            // don't delete the msg, because the sender is
            // responsible for deleting msgs
            break;
        // in case of NACK, nothing should be done according to TA's post
        default:
            break;
    }
}

void Node::start_timer(int frame_seq_num, double delayTime)
{
    // start new timer for this message
    cMessage* timeout_message = new cMessage("timeout");
    timeout_message->addPar("frame_seq");
    timeout_message->par("frame_seq").setLongValue(frame_seq_num);
    // set the timer value according to TO parameter
    scheduleAt(simTime()+static_cast<simtime_t>(getParentModule()->par("TO").doubleValue() + delayTime), timeout_message);
    // store the timer msg pointer to access later in case of stop_timer
    timeouts_buffer[frame_seq_num] = timeout_message;
}

void Node::stop_timer(int frame_seq_num)
{
//    EV << "stopping timer for " << std::to_string(frame_seq_num);
    cancelAndDelete(timeouts_buffer[frame_seq_num]);
    timeouts_buffer[frame_seq_num] = nullptr;
}

double Node::delay_frame(double time, Message_Base *msg){
    time += getParentModule()->par("ED").doubleValue();
    return time;
}
double Node::duplicate_frame(double time, Message_Base *msg){
    Message_Base *dup_msg = msg->dup();
    time += getParentModule()->par("DD").doubleValue();
    sendDelayed(dup_msg, time, "out_port");
    return time;
}

int Node::modify_frame(double time, Message_Base *msg){
    std::string payload(msg->getM_Payload());
    // select random bit index
    int bit_index =  intuniform(0,static_cast<int>(payload.length())*8-1, 0);
    // get the char that contains that bit
    char selected_char =  payload[bit_index/8];
    // construct bitset of the char to flip the selected bit
    std::bitset<8> char_bits(selected_char);
    // flip the selected bit
    char_bits[bit_index%8].flip();
    // update the payload after the modification
    payload[bit_index/8] = static_cast<char>(char_bits.to_ulong());

    msg->setM_Payload(payload.c_str());

    return bit_index;
}

void Node::send_data(std::string payload, std::string error, double sendingOffsetTime)
{
    // set message name according to frame sequence number
    std::string message_name = "message " + std::to_string(next_frame_to_send_seq_num);
    Message_Base* msg = new Message_Base(message_name.c_str());

    // fill message information
    msg->setHeader(next_frame_to_send_seq_num);
    msg->setM_Payload(payload.c_str());
    msg->setFrame_Type(Data);
    int ack_num = (next_frame_to_send_seq_num + 1) % (MAX_SEQ + 1);
    msg->setAck_Num(ack_num);

    // clone the message to store a copy of it
    Message_Base *msg_dup = msg->dup();

    // apply framing algorithm
    std::string framed_payload = frame_packet(payload);
    // update the frame payload
    msg->setM_Payload(framed_payload.c_str());

    // apply error detection algorithm and fill corresponding data
    error_detection(msg);

    // store message to the buffer
    frames_buffer[nbuffered] = {error, msg_dup};

    // apply errors on message according to error code
    apply_error_and_send(error, sendingOffsetTime, msg);

    // start timer for that particular frame
    start_timer(next_frame_to_send_seq_num, sendingOffsetTime);

    //expand the sender's window
    nbuffered = nbuffered + 1;

    inc(next_frame_to_send_seq_num);
}

void Node:: apply_error_and_send(std::string error, double sendingOffsetTime, Message_Base *msg){
    bool lose_frame = false;
    std::string modified_frame;
    double loss_prob = getParentModule()->par("LP").doubleValue();
    int modified_bit = -1;
    int delay_interval = 0;
    int duplicate_version = 0;

    double delayTime = sendingOffsetTime + getParentModule()->par("TD").doubleValue();
    switch(error_codes[error])
    {
    case NO_ERRORs:
        // delayTime = PT + TD
        sendDelayed(msg, delayTime, "out_port");
        break;
    case DUP:
        sendDelayed(msg, delayTime, "out_port");
        duplicate_version = 1;
        log_to_file(BEFORE_TRANS, simTime().dbl() + sendingOffsetTime, nodeId, msg, lose_frame,
                    error, modified_bit, duplicate_version, delay_interval);

        duplicate_version = 2;
        delay_interval += duplicate_frame(delayTime, msg) - delayTime;
        break;
    case DELAY:
        delay_interval = delay_frame(delayTime, msg) - delayTime;
        delayTime = delay_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        break;
    case DUP_DELAY:
        delay_interval = delay_frame(delayTime, msg) - delayTime;
        delayTime = delay_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        duplicate_version = 1;
        log_to_file(BEFORE_TRANS, simTime().dbl() + sendingOffsetTime, nodeId, msg, lose_frame,
                    error, modified_bit, duplicate_version, delay_interval);
        duplicate_version = 2;
        delay_interval += duplicate_frame(delayTime, msg) - delayTime;
        break;
    case LOSS:
        lose_frame = true;
        break;
    case MOD:
        modified_bit = modify_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        break;
    case MOD_DELAY:
        modified_bit =  modify_frame(delayTime, msg);
        delay_interval = delay_frame(delayTime, msg) - delayTime;
        delayTime =  delay_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        break;
    case MOD_DUP:
        modified_bit =  modify_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        duplicate_version = 1;
        log_to_file(BEFORE_TRANS, simTime().dbl() + sendingOffsetTime, nodeId, msg, lose_frame,
                    error, modified_bit, duplicate_version, delay_interval);

        duplicate_version = 2;
        delay_interval += duplicate_frame(delayTime, msg) - delayTime;
        break;
    case MOD_DUP_DELAY:
        modified_bit = modify_frame(delayTime, msg);
        delay_interval = delay_frame(delayTime, msg) - delayTime;
        delayTime = delay_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        duplicate_version = 1;
        log_to_file(BEFORE_TRANS, simTime().dbl() + sendingOffsetTime, nodeId, msg, lose_frame,
                    error, modified_bit, duplicate_version, delay_interval);

        duplicate_version = 2;
        delay_interval += duplicate_frame(delayTime, msg) - delayTime;
        break;
    default:
        lose_frame = true;
        break;
    }

    log_to_file(BEFORE_TRANS, simTime().dbl() + sendingOffsetTime, nodeId, msg, lose_frame,
            error, modified_bit, duplicate_version, delay_interval);
}

void Node::handle_network_layer_ready() {
    // read from the input file the next message to send
    std::pair<std::string, std::string> message_info = get_next_message();
    std::string payload = message_info.second;
    std::string error = message_info.first;

    // send the data
    if(nbuffered < MAX_SEQ)
        send_data(payload, error);
}



std::pair<std::string, std::string> Node::get_next_message(){

    std::string error = "";
    std::string message = "";

    if(line < numOfLines){
        error = errors[line];
        message = messages[line];
        line++;
    }

    log_to_file(UPON_READING, simTime().dbl(), nodeId, nullptr, false, error);

    return {error, message};

}

std::string Node::frame_packet(std::string payload)
{
    // Framing
    std::string frame = "";
    frame += '$';
    for(int i = 0; i < payload.size(); i++){
        if(payload[i] == '$' || payload[i] == '/')
            frame+='/';

        frame += payload[i];
    }
    frame+='$';

    return frame;
}

void Node::error_detection(Message_Base *msg)
{
    if(sender){
        std::bitset<8> parityByte(0);

        for(int i = 0; i < msg->M_Payload.size(); i++)
            parityByte ^= std::bitset<8>( msg->M_Payload.c_str()[i]);

        msg->setTrailer(parityByte.to_ulong());
    }
    else{

        std::bitset<8> parityByte(0);
        for(int i = 0; i < msg->M_Payload.size(); i++)
                    parityByte ^= std::bitset<8>( msg->M_Payload.c_str()[i]);

        if(parityByte.to_ulong() == msg->getTrailer() && frame_expected == msg->getHeader()){
            inc(frame_expected);
            msg->setFrame_Type(ACK);
        }

        else{
            msg->setHeader(frame_expected);
            msg->setFrame_Type(NACK);
        }

        std::string message_name = "ack frame " + std::to_string(frame_expected);
        msg->setAck_Num(frame_expected);

        send_control(msg);
    }
}
void Node::send_control(Message_Base *msg){

    // Lose the frame with probability LP
    bool lose_frame = static_cast<bool>(bernoulli(getParentModule()->par("LP").doubleValue(), 0));
    // send the message after the computed delay interval
    double delayTime = getParentModule()->par("PT").doubleValue();

    log_to_file(CONTROL_FRAME, simTime().dbl() + delayTime, nodeId, msg, lose_frame);

    if (lose_frame)
        delete msg;
    else {
        // increase the delay by the transmission delay
        delayTime += getParentModule()->par("TD").doubleValue();
        sendDelayed(msg, delayTime, "out_port");
    }
}

void Node::tokenize(std::string const &str, const char delim,
            std::vector<std::string> &out)
{
    // construct a stream from the string
    std::stringstream ss(str);

    std::string s;
    while (std::getline(ss, s, delim)) {
        out.push_back(s + " ");
    }
}

void Node::readInputFile(){
    std::string line;
    if(input_file.is_open()) {

        const char delim = ' ';
        int k = 0;

        while (getline(input_file, line)) {
            std::string error = "";
            std::string message = "";
            std::vector<std::string> result;

            std::size_t split_index = line.find(' ');
            error = line.substr(0, split_index);
            message = line.substr(split_index+1);
//            tokenize(line, delim, result);

//            error = result[0];
//            for (int i = 1; i < result.size(); i++)
//                    message += result[i];

            messages.push_back(message);
            errors.push_back(error);
            k++;

        }

        numOfLines = k;
        input_file.close();
    }
}

void Node::log_to_file(log_info_type info_type, double event_time, char nodeId,
        Message_Base* msg, bool lost, std::string error, int modified_bit,
         int duplicate_version, double delayInterval) {

//    output_file.open("output.txt");
    EV << "Output File is open = " << output_file.is_open() <<endl;

    switch(info_type){
    case UPON_READING:
        output_file << "At time[" << event_time << "], Node[" << nodeId <<
        "], Introducing channel error with code =[" << error <<  "]\n";
        output_file << std::flush;
        break;
    case BEFORE_TRANS:
        output_file << "At time [" << event_time << "], Node["<< nodeId <<
        "] sent frame with seq_num = [" << std::to_string(msg->getHeader())
        << "] and payload = [" << msg->getM_Payload() << "] and trailer =[" << std::bitset<8>(msg->getTrailer()).to_string()
                << "], Modified [" << modified_bit << "], Lost[" << (lost?"Yes":"No") <<
                "], Duplicate[" << duplicate_version << "], Delay [" << delayInterval <<  "]\n";
        output_file << std::flush;
        break;
    case TIMEOUT_EVENT:
        output_file << "Time out event at time [" << event_time << "], at Node["
        << nodeId <<"], for frame with seq_num=[" << msg->getHeader() <<"]\n";
        break;
    case CONTROL_FRAME:
        std::string ack_type = (msg->getFrame_Type() == ACK ? "ACK": "NACK");
        int seq_num = msg->getAck_Num();
        output_file << "At time[" << event_time << "], Node[" << nodeId <<
                    "] sending [" << ack_type << "] with number["
                    << seq_num << "], loss[" << (lost?"Yes":"No")<<"]\n";
        output_file << std::flush;
        break;
    }
//    output_file.close();
}
