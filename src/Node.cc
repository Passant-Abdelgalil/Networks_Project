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

    // buffers to store pointers to messages
    frames_buffer.resize(MAX_SEQ);
    timeouts_buffer.resize(MAX_SEQ);
}

void Node::handleMessage(cMessage *msg)
{
    // check if this message is from the coordinator
    if(msg->arrivedOn("in_ports", 0)) {

        EV << "Node Id From Node = " << msg->par(msg->findPar("nodeId")).longValue() << endl;
//        EV << "Starting Time From Node = " << msg->par(msg->findPar("startTime")).longValue() << endl;
//        EV << "simTime Time From Node = " << simTime() << endl;

        nodeId = msg->par(msg->findPar("nodeId")).longValue();
        long startTime = msg->par(msg->findPar("startTime")).longValue();

        start_protocol();
        delete msg;
        cMessage * self_msg = new cMessage("enable network");
        scheduleAt(simTime() + static_cast<simtime_t>(getParentModule()->par("PT").doubleValue()) + static_cast<simtime_t>(startTime), self_msg);
    }
    else if(strcmp(msg->getName(), "enable network") == 0) {
        if(initialState){
            readInputFile();
            initialState = false;
        }

        if(network_layer_enabled)
            handle_network_layer_ready();
        delete msg;
        if(!end_communication){
            cMessage * self_msg = new cMessage("enable network");
            scheduleAt(simTime() + static_cast<simtime_t>(getParentModule()->par("PT").doubleValue()), self_msg);
        }
    }
    else if(msg->isSelfMessage()&& msg->hasPar("frame_seq")) { // timeout event
        // get sequence number of the timed out frame
        int frame_seq_num = msg->par("frame_seq");
        handle_timeout(frame_seq_num);

        delete msg;
    }
    else  // frame_arrival event
        handle_frame_arrival(dynamic_cast<Message_Base *>(msg));

    if(sender && nbuffered < MAX_SEQ)
        network_layer_enabled = true;
    else
        network_layer_enabled = false;

}

void Node::start_protocol() {
    // so initialize this node as the sender
    sender = true;
    // set the WS for this sender protocol instance
    MAX_SEQ = getParentModule()->par("WS").intValue();

    frames_buffer.resize(MAX_SEQ);
    timeouts_buffer.resize(MAX_SEQ);
}
void Node::inc(int& frame_seq_num)
{
    frame_seq_num = (frame_seq_num + 1) % getParentModule()->par("WS").intValue();;
}

void Node::handle_timeout(int frame_seq_num){
    next_frame_to_send_seq_num = ack_expected;
    int size = nbuffered;
    nbuffered = 0;
    // re-transmit all frames in the buffer
    for (int i = 0; i < size; i++){
        std::pair<error_code, Message_Base*> frame_info = frames_buffer[i];
        std::string payload(frame_info.second->getM_Payload());

        /*
         * all the messages in the sender’s window will be transmitted again as
         * usual with the normal calculations for errors and delays except for
         * the first frame that is the cause for the timeout,
         * it will be sent error free
         * */

        if(frame_info.second->getHeader() == frame_seq_num)
            send_data(payload, NO_ERRORs);
        else {
            // remove the timeout for this frame if any
            stop_timer(frame_info.second->getHeader());
            send_data(payload, frame_info.first);
        }
    }

}


void Node::update_window(int ack_num){

    // don't update the window if the received acknowledge isn't the expected one
    if (ack_num != frames_buffer.front().second->getHeader())
        return;

    // advance the expected acknowledge sequence number
    inc(ack_expected);

    // shift frames within the buffer to the left
    frames_buffer.erase(frames_buffer.begin());
    nbuffered--;
    EV << "\nnbuffered is: " << nbuffered;

    while(!frames_buffer.empty() && nbuffered > 0)
    {
        auto frame_it = frames_buffer.begin();
        if (!frame_it->second)
            break;
        // if a timer is set for this frame, it's not acknowledged yet
        if (timeouts_buffer[frame_it->second->getHeader()])
            break;
        frames_buffer.erase(frames_buffer.begin());
        nbuffered--;
        EV << "\nnbuffered is: " << nbuffered;
    }
    frames_buffer.resize(MAX_SEQ);
}

void Node:: handle_frame_arrival(Message_Base *frame)
{
    switch(frame->getFrame_Type()){
        case ACK:   // this is the sender node
            stop_timer(frame->getHeader());
            update_window(frame->getHeader());
            delete frame;
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

void Node::start_timer(int frame_seq_num)
{
    // start new timer for this message
    cMessage* timeout_message = new cMessage("timeout");
    timeout_message->addPar("frame_seq");
    timeout_message->par("frame_seq").setLongValue(frame_seq_num);
    // set the timer value according to TO parameter
    scheduleAt(simTime()+static_cast<simtime_t>(getParentModule()->par("TO").doubleValue()), timeout_message);
    // store the timer msg pointer to access later in case of stop_timer
    timeouts_buffer[frame_seq_num] = timeout_message;
}

void Node::stop_timer(int frame_seq_num)
{
    EV << "stopping timer for " << std::to_string(frame_seq_num);
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

std::string Node::modify_frame(double time, Message_Base *msg){
    std::string payload(msg->getM_Payload());
    int bit_index =  intuniform(0,static_cast<int>(payload.length())-1, 0);
    payload[bit_index] = '1' - payload[bit_index];
    msg->setM_Payload(payload.c_str());
    return payload;
}

void Node::send_data(std::string payload, error_code error)
{
    // set message name according to frame sequence number
    std::string message_name = "message " + std::to_string(next_frame_to_send_seq_num);
    Message_Base* msg = new Message_Base(message_name.c_str());

    // set sequence number
    msg->setHeader(next_frame_to_send_seq_num);

    // apply framing algorithm
    std::string framed_payload = frame_packet(payload);
    // update the frame payload
    msg->setM_Payload(framed_payload.c_str());

    // set frame type to data
    msg->setFrame_Type(Data);

    // apply error detection algorithm and fill corresponding data
    error_detection(msg);

    // The ACK/NACK number are set as the sequence number of the next expected frame
    int ack_num = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
    msg->setAck_Num(ack_num);

    // set delayTime to transmission  delay parameter
    double delayTime = getParentModule()->par("TD").doubleValue();

    // store message to the buffer
    frames_buffer[nbuffered] = {error, msg};

    // apply errors on message according to error code
    apply_error(error, delayTime, msg);

    // start timer for that particular frame
    start_timer(next_frame_to_send_seq_num);

    //expand the sender's window
    nbuffered = nbuffered + 1;
    EV << "\nsend_data, nbuffered is: " << nbuffered;


    inc(next_frame_to_send_seq_num);
}

void Node:: apply_error(error_code error, double delayTime, Message_Base *msg){
    bool lose_frame = false;
    std::string modified_frame;
    double loss_prob = getParentModule()->par("LP").doubleValue();

    switch(error)
    {
    case NO_ERRORs:
        // send the message after the processing delay time only
        sendDelayed(msg, delayTime, "out_port");
        break;
    case DUP:
        duplicate_frame(delayTime, msg);
        break;
    case DELAY:
        delayTime = delay_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        break;
    case DUP_DELAY:
        delayTime = delay_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        duplicate_frame(delayTime, msg);
        break;
    case LOSS:
        lose_frame = static_cast<bool>(bernoulli(loss_prob, 0));
        if (!lose_frame)
            sendDelayed(msg, delayTime, "out_port");
        break;
    case MOD:
        modify_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        break;
    case MOD_DELAY:
        modify_frame(delayTime, msg);
        delayTime =  delay_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        break;
    case MOD_DUP:
        modify_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
//        EV << "From Node Before Sending Message " << msg->M_Payload <<endl;
        duplicate_frame(delayTime, msg);
        break;
    case MOD_DUP_DELAY:
        modify_frame(delayTime, msg);
        delayTime = delay_frame(delayTime, msg);
        sendDelayed(msg, delayTime, "out_port");
        duplicate_frame(delayTime, msg);
        break;
    default:
        break;
    }
}

void Node::handle_network_layer_ready() {
    // read from the input file the next message to send
    std::pair<error_code, std::string> message_info = get_next_message();
    std::string payload = message_info.second;
    error_code error = message_info.first;

    // send the data
    if(nbuffered < MAX_SEQ)
        send_data(payload, error);
}



std::pair<error_code, std::string> Node::get_next_message(){
    // TODO: Implement File Parser read function

//    EV << "From Node Messages = " << messages[line]<< endl;
//    EV << "From Node Messages = " << errors[line]<< endl;

    std::string error = "";
    std::string message = "";

    if(line < numOfLines){
        error = errors[line];
        message = messages[line];
        line++;
    }

//    EV << "From Node Line = " <<line <<endl;
//    EV << "From Node Error = " <<error;
//    EV << "From Node Error = " <<error.size() << endl;

    if(numOfLines == line){
        end_communication = true;
        EV << "From Node Line = " <<line <<endl;
        EV << "From Node Error = " <<error;
        EV << "From Node Error = " <<error.size() << endl;
    }

    if(error == "0000 ")
        return  {NO_ERRORs, message};
    else if(error == "0001 ")
        return  {DELAY, message};
    else if(error == "0010 ")
        return  {DUP, message};
    else if(error == "0011 ")
        return  {DUP_DELAY, message};
    else if(error == "0100 ")
        return  {LOSS, message};
    else if(error == "0101 ")
        return  {LOSS, message};
    else if(error== "0110 ")
        return  {LOSS_BOTH, message};
    else if(error == "0111 ")
        return  {LOSS_BOTH, message};
    else if(error == "1000 ")
        return  {MOD, message};
    else if(error == "1001 ")
        return  {MOD_DELAY, message};
    else if(error == "1010 ")
        return  {MOD_DUP, message};
    else if(error == "1011 ")
        return  {MOD_DUP_DELAY, message};
    else if(error == "1100 ")
        return  {LOSS, message};
    else if(error == "1101 ")
        return  {LOSS, message};
    else if(error == "1110 ")
        return  {LOSS_BOTH, message};
    else if(error == "1111 ")
        return  {LOSS_BOTH, message};

    return  {NO_ERRORs,"Hello"};
}

std::string Node::frame_packet(std::string payload)
{
    // TODO: Implement Framing Logic

//    EV << "From Packet Payload = " << payload;

    // Framing
    std::string frame = "";
    frame += '$';
    for(int i = 0; i < payload.size() - 1; i++){
        if(payload[i] == '$' || payload[i] == '/')
            frame+='/';

        frame += payload[i];
    }
    frame+='$';

//    EV << "From Packet frame = " << frame;

    return frame;
}

void Node::error_detection(Message_Base *msg)
{

    // TODO: Implement Framing Logic
    if(sender){
//       EV << "From Error detection = " << msg->M_Payload;
        std::bitset<8> parityByte(0);

//        EV << "From Error detection = " << msg->M_Payload.size() << endl;

        for(int i = 0; i < msg->M_Payload.size(); i++)
            parityByte ^= std::bitset<8>( msg->M_Payload.c_str()[i]);

        EV << "From Error detection Sender parityByte = " << char(parityByte.to_ulong()) <<endl;
        msg->setTrailer(parityByte.to_ulong());
    }
    else{

        EV << "From Error detection Receiver = " << msg->M_Payload<<endl;
//        EV << "From Error detection Receiver = " << msg->getTrailer()<<endl;
        std::bitset<8> parityByte(0);
        for(int i = 0; i < msg->M_Payload.size(); i++)
                    parityByte ^= std::bitset<8>( msg->M_Payload.c_str()[i]);
//        EV << "From Error detection Receiver parityByte = " << char(parityByte.to_ulong()) <<endl;

//           if(parityByte.to_ulong() == msg->getTrailer())
//               EV << "From Error detection Receiver parityByte Condition= " << true <<endl;
//           else
//               EV << "From Error detection Receiver parityByte Condition= " << false <<endl;

        std::string message_name = "ack frame " + std::to_string(frame_expected);
        Message_Base *control_msg = new Message_Base(message_name.c_str());


        if(parityByte.to_ulong() == msg->getTrailer())
            control_msg->setFrame_Type(ACK);
        else
            control_msg->setFrame_Type(NACK);

        control_msg->setHeader(frame_expected);
        inc(frame_expected);
        control_msg->setAck_Num(frame_expected);
        send_control(control_msg);
    }
}
void Node::send_control(Message_Base *msg){

    /* TODO:
     * log next message
     * At time[.. starting sending time after processing….. ], Node[id] Sending [ACK/NACK] with
        number […] , loss [Yes/No ]
     * */

    // Lose the frame with probability LP
    bool lose_frame = static_cast<bool>(bernoulli(getParentModule()->par("LP").doubleValue(), 0));
    if (lose_frame)
        delete msg;
    else {
        // send the message after the computed delay interval
        double delayTime = getParentModule()->par("PT").doubleValue();
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
    std::ifstream filestream("C:\\Users\\CMP\\OneDrive\\Documents\\GitHub\\Networks_Project\\src\\input0.txt");
    EV << " File is open = " << filestream.is_open() <<endl;

    if(filestream.is_open()) {

        const char delim = ' ';
        int k = 0;

        while (getline(filestream, line)) {

            std::string error = "";
            std::string message = "";
            std::vector<std::string> result;

            tokenize(line, delim, result);

            error = result[0];
            for (int i = 1; i < result.size(); i++)
                    message += result[i];

            messages.push_back(message);
            errors.push_back(error);
            k++;


//            EV << "Error From Node Base = " << typeid(error).name() << endl;
//            EV << "Message From Node  = " << message << endl;


        }

        numOfLines = k;
        filestream.close();
    }

}
