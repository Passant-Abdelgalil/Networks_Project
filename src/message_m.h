//
// Generated file, do not edit! Created by nedtool 5.6 from message.msg.
//

#ifndef __MESSAGE_M_H
#define __MESSAGE_M_H

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#include <omnetpp.h>

// nedtool version check
#define MSGC_VERSION 0x0506
#if (MSGC_VERSION!=OMNETPP_VERSION)
#    error Version mismatch! Probably this file was generated by an earlier version of nedtool: 'make clean' should help.
#endif



// cplusplus {{
	#include <bitset>
	typedef std::bitset<8> bits;
	typedef unsigned char Byte;
	
	enum Msg_Type { Data, ACK, NACK };
// }}

/**
 * Class generated from <tt>message.msg:32</tt> by nedtool.
 * <pre>
 * packet Message
 * {
 *     \@customize(true);  	// see the generated C++ header for more info
 * 
 *     int Header;			// the data sequence number.
 *     string M_Payload;	// the message contents after byte stuffing (in characters).
 *     Byte Trailer;		// the parity byte.
 *     Msg_Type Frame_Type;// Data=0/ACK=1 /NACK=2.
 *     int Ack_Num;		// ACK/NACK number.
 * }
 * </pre>
 *
 * Message_Base is only useful if it gets subclassed, and Message is derived from it.
 * The minimum code to be written for Message is the following:
 *
 * <pre>
 * class Message : public Message_Base
 * {
 *   private:
 *     void copy(const Message& other) { ... }

 *   public:
 *     Message(const char *name=nullptr, short kind=0) : Message_Base(name,kind) {}
 *     Message(const Message& other) : Message_Base(other) {copy(other);}
 *     Message& operator=(const Message& other) {if (this==&other) return *this; Message_Base::operator=(other); copy(other); return *this;}
 *     virtual Message *dup() const override {return new Message(*this);}
 *     // ADD CODE HERE to redefine and implement pure virtual functions from Message_Base
 * };
 * </pre>
 *
 * The following should go into a .cc (.cpp) file:
 *
 * <pre>
 * Register_Class(Message)
 * </pre>
 */
class Message_Base : public ::omnetpp::cPacket
{
  protected:
    int Header;
    ::omnetpp::opp_string M_Payload;
    Byte Trailer;
    Msg_Type Frame_Type;
    int Ack_Num;

  private:
    void copy(const Message_Base& other);

  public:
    // protected and unimplemented operator==(), to prevent accidental usage
    bool operator==(const Message_Base&);
    // make constructors protected to avoid instantiation
    Message_Base(const char *name=nullptr, short kind=0);
    Message_Base(const Message_Base& other);
    // make assignment operator protected to force the user override it
    Message_Base& operator=(const Message_Base& other);

  public:
    virtual ~Message_Base();
    virtual Message_Base *dup() const override {
        Message_Base* msgDuplicate = new Message_Base();
        msgDuplicate->Header = this->getHeader();
        msgDuplicate->Trailer = this->Trailer;
        msgDuplicate->Ack_Num = this->Ack_Num;
        msgDuplicate->Frame_Type = this->Frame_Type;
        msgDuplicate->M_Payload = this->M_Payload;
        return msgDuplicate;
    }
    virtual void parsimPack(omnetpp::cCommBuffer *b) const override;
    virtual void parsimUnpack(omnetpp::cCommBuffer *b) override;

    // field getter/setter methods
    virtual int getHeader() const;
    virtual void setHeader(int Header);
    virtual const char * getM_Payload() const;
    virtual void setM_Payload(const char * M_Payload);
    virtual Byte& getTrailer();
    virtual const Byte& getTrailer() const {return const_cast<Message_Base*>(this)->getTrailer();}
    virtual void setTrailer(const Byte& Trailer);
    virtual Msg_Type& getFrame_Type();
    virtual const Msg_Type& getFrame_Type() const {return const_cast<Message_Base*>(this)->getFrame_Type();}
    virtual void setFrame_Type(const Msg_Type& Frame_Type);
    virtual int getAck_Num() const;
    virtual void setAck_Num(int Ack_Num);
};


#endif // ifndef __MESSAGE_M_H

