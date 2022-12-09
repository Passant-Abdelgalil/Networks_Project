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
    ack_expected = 0;
    next_frame_to_send_seq_num = 0;
    frame_expected = 0;
    nbuffered = 0;

    frames_buffer.resize(MAX_SEQ);
    timeouts_buffer.resize(MAX_SEQ);
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
    frame_seq_num = (frame_seq_num + 1) % MAX_SEQ;
}

void Node::handle_timeout(int frame_seq_num){
    // TODO: log "Time out event at time [.. timer off-time….. ],
    //              at Node[id] for frame with seq_num=[..]"

    // TODO implement me
    next_frame_to_send_seq_num = ack_expected;
    for (int i = 1; i <= nbuffered; i++){
        std::pair<error_code, Message_Base*> frame_info = frames_buffer[i];
        std::string payload(frame_info.second->getM_Payload());
        /*
         * all the messages in the sender’s window will be transmitted again as
         * usual with the normal calculations for errors and delays except for
         * the first frame that is the cause for the timeout,
         * it will be sent error free
         * */
        if(frame_info.second->getHeader() == frame_seq_num)
            send_data(frame_info.second, payload, NO_ERROR);
        else
            send_data(frame_info.second, payload, frame_info.first);
    }

}


void Node::handleMessage(cMessage *msg)
{
    // check if it's a self-message
    if(msg->isSelfMessage()) {
        // check if it's a timeout event
        std::string msgName(msg->getName());
        std::size_t found = msgName.find("timeout");
        if(found != std::string::npos){
            int frame_seq_num = std::stoi( msgName.substr(0, found));
            handle_timeout(frame_seq_num);
        }
        // check if it's an enable_network_layer event
        else if(strcmp(msg->getName(), "enable network") == 0)
            handle_network_layer_ready();
    }
    else {
        // try to cast the message to Message_Base
        Message_Base *message = dynamic_cast<Message_Base *>(msg);

        // if failed, this message is sent by the coordinator
        if(!message) start_protocol(); // initialize protocol parameters
        else handle_frame_arrival(message); //else it's a frame_arrival event
    }

    if(sender && nbuffered < MAX_SEQ)
    {
        msg->setName("enable network");
        scheduleAt(simTime() + static_cast<simtime_t>(getParentModule()->par("PT").doubleValue()), msg);
    }
}
void Node::update_window(){
    while(!frames_buffer.empty()){
        if(frames_buffer.front().second)
            return;
        frames_buffer.erase(frames_buffer.begin());
        nbuffered -= 1;
    }
    frames_buffer.resize(MAX_SEQ);
}

void Node:: handle_frame_arrival(Message_Base *frame)
{
    switch(frame->getFrame_Type()){
        case ACK:   // this is the sender node
            stop_timer(frame->getHeader());

            // TODO: CHECK WINDOW BOUNDS UPDATES
            break;
        case Data:  // this is the receiver node
            if (frame->getHeader() == frame_expected){
                // TODO: error detection
                frame->setFrame_Type(ACK);
                send_control(frame);
                inc(frame_expected);
            }
            break;
        // in case of NACK, nothing should be done according to TA's post
        default:
            break;
    }

}

void Node::start_timer(int frame_seq_num)
{
    // start new timer for this message
    std::string message_content = std::to_string(frame_seq_num) + "timeout";
    cMessage* timeout_message = new cMessage(message_content.c_str());
    // store the timer msg pointer to access later in case of stop_timer
    timeouts_buffer[frame_seq_num] = timeout_message;
    // set the timer value according to TO parameter
    scheduleAt(simTime()+static_cast<simtime_t>(getParentModule()->par("TO").doubleValue()), timeout_message);
}

void Node::stop_timer(int frame_seq_num)
{
    cancelAndDelete(timeouts_buffer[frame_seq_num]);
}

double Node::delay_frame(double time, Message_Base *msg){
    time += getParentModule()->par("ED").doubleValue();
    sendDelayed(msg, time, "out_port");
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
    sendDelayed(msg, time, "out_port");
    return payload;
}

void Node::send_data(Message_Base* msg, std::string payload, error_code error)
{
    std::string message_name = "message " + std::to_string(next_frame_to_send_seq_num);
    msg->setName(message_name.c_str());

    msg->setHeader(next_frame_to_send_seq_num);
    std::string framed_payload = frame_packet(payload);
    msg->setM_Payload(framed_payload.c_str());
    msg->setFrame_Type(Data);
    error_detection(msg);

// ========= TO BE REVISITED ============
    int ack_num = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
    msg->setAck_Num(ack_num);
    msg->setTrailer('0');
// ======================================

    double delayTime = getParentModule()->par("PT").doubleValue();

    /* TODO:
     log "At time [.. starting sending time after processing….. ], Node[id]
     [sent/received] frame with seq_num=[..] and
     payload=[ ….. in characters after modification….. ] and
     trailer=[…….in bits….. ] , Modified [-1 for no modification,
     otherwise the modified bit number],Lost [Yes/No],
     Duplicate [0 for none, 1 for the first version, 2 for the second version]
     , Delay [0 for no delay , otherwise the error delay interval].""
    */

    // store message to the buffer
    frames_buffer[nbuffered] = {error, msg};
    // apply errors on message according to error code
    apply_error(error, delayTime, msg);
    // start timer for that particular frame
    start_timer(next_frame_to_send_seq_num);
}

void Node:: apply_error(error_code error, double delayTime, Message_Base *msg){
    bool lose_frame = false;
    std::string modified_frame;

    switch(error)
    {
    case NO_ERROR:
        // send the message after the processing delay time only
        sendDelayed(msg, delayTime, "out_port");
        break;
    case DUP:
        delayTime = duplicate_frame(delayTime, msg);
        break;
    case DELAY:
        delayTime =  delay_frame(delayTime, msg);
        break;
    case DUP_DELAY:
        delayTime =  delay_frame(delayTime, msg);
        duplicate_frame(delayTime, msg);
        break;
    case LOSS:
        lose_frame = static_cast<bool>(bernoulli(getParentModule()->par("LP").doubleValue(), 1));
        if (!lose_frame)
            sendDelayed(msg, delayTime, "out_port");
        break;
    case MOD:
        modify_frame(delayTime, msg);
        break;
    case MOD_DELAY:
        modified_frame = modify_frame(delayTime, msg);
        delay_frame(delayTime, msg);
        break;
    case MOD_DUP:
        modify_frame(delayTime, msg);
        duplicate_frame(delayTime, msg);
        break;
    case MOD_DUP_DELAY:
        modify_frame(delayTime, msg);
        delayTime = delay_frame(delayTime, msg);
        duplicate_frame(delayTime, msg);
        break;
    default:
        break;
    }
}

void Node::handle_network_layer_ready(){
    // read from the input file the next message to send
    std::pair<error_code, std::string> message_info = get_next_message();
    std::string next_message = message_info.second;
    error_code error = message_info.first;

    Message_Base *message = new Message_Base();
    // send the data
    send_data(message, next_message, error);
    //expand the sender's window
    nbuffered = nbuffered + 1;

    inc(next_frame_to_send_seq_num);
}


// TODOs

std::pair<error_code, std::string> Node::get_next_message(){
    // TODO: Implement File Parser read function
    // TODO:  log "At time [.. starting processing time….. ],
    //              Node[id] , Introducing channel error with code
    //              =[ …code in 4 bits… ]."
return  {NO_ERROR, "Hello"};
}

std::string Node::frame_packet(std::string payload)
{
    // TODO: Implement Framing Logic
    return payload;
}

void Node::error_detection(Message_Base *msg)
{
    // TODO: Implement Framing Logic
    if(sender){

    }
    else{

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
    else{
        // send the message after the computed delay interval
        double delayTime = getParentModule()->par("PT").doubleValue();
        sendDelayed(msg, delayTime, "out_port");
    }
}
