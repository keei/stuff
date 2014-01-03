#ifndef messaging_h
#define messaging_h

#include <Arduino.h>
#include <RF24Network.h>
#include <RF24.h>
#include "pin_config.h"
#include <avr/pgmspace.h>

#define MAX_MSG (32-sizeof(RF24NetworkHeader))
#define EEPROM_NODE_ADDR 1022 //location to store node addres (2 bytes)
#define MASTER_NODE 0 //node to send all our events to. (this doenst have to be the rootnode)


uint16_t this_node=0;

// nRF24L01(+) radio using the Getting Started board

class Msg
{

  public:
  RF24 radio;
  RF24Network network;

  Msg() : radio(2, CS_RF24_PIN), network(radio)
  {

  }

  void begin()
  {
    radio.begin();
    network.begin(100, this_node);
  }

  //send a raw message-line to the master (and do serial echoing and error checking)
  bool send_line(const char * msg_buf)
  {
      RF24NetworkHeader header(MASTER_NODE, 'l');

      //if we're not the master, print sended messages on serial as well
      //this way we can use the serial api also when there is no network connection.
      if (this_node!=MASTER_NODE)
      {
        Serial.print('0');
        Serial.print(this_node,OCT);
        Serial.print(' ');
        Serial.println(msg_buf);
      }

      if (!network.write(header,msg_buf,strlen(msg_buf)+1))
      {
        //this is mostly for debugging:
        Serial.print('0');
        Serial.print(this_node,OCT);
        Serial.print(F(" net.error "));
        Serial.println(msg_buf);
        return(false);
      }
      return(true);

  }

  //send message with string parameter to master
  //NOTE: this should always be a PSTR()
  bool send(const char * event, char *par=NULL)
  {
      char msg_buf[MAX_MSG]; 

      strncpy_P(msg_buf, event, sizeof(msg_buf));
      msg_buf[MAX_MSG-1]=0;

      int left=MAX_MSG-1-strlen(msg_buf);
      if (par!=NULL && left>2)
      {
        ;
        strcat(msg_buf," ");
        strncat(msg_buf, par,left-1);
      }
      return(send_line(msg_buf));
  }

  //should be implemented in our main sketch and call other handlers and/or do stuff
  bool handle(uint16_t from, char * event,  char * par);


  //check if there are messages to receive from the network, or send from serial.
  void loop()
  {
    char msg_buf[MAX_MSG]; 
    static unsigned long last_ping;
    //Pump the network regularly
    network.update();

    //network message available?
    while ( network.available() )
    {
      // If so, take a look at it 
      RF24NetworkHeader header;
      network.peek(header);


      if (header.type=='l')
      {
        network.read(header,msg_buf, sizeof(msg_buf));
        msg_buf[sizeof(msg_buf)-1]=0; //ensure 0 termination

        char * par;
        par=strchr(msg_buf,' ');
        if (par!=NULL)
        {
          (*par)=0; //0 terminate event-name
          par++; //par starts at next byte
        }
        handle(header.from_node, msg_buf, par);
      }
    }

    //serial message available?
    if (Serial.available())
    {
      RF24NetworkHeader header;
      
      //to node?
      Serial.readBytesUntil(' ', msg_buf, sizeof(msg_buf));
      sscanf(msg_buf, "%u", &header.to_node);
      header.type='l';

      //the rest will become one 0 terminated string:
      byte len=Serial.readBytesUntil('\n', msg_buf, sizeof(msg_buf)-1);
      msg_buf[len]=0;
      network.write(header,&msg_buf,len+1);
    }

    //ping master node
    if ( millis()-last_ping > 60000)
    {
      send(PSTR("node.ping"));
      last_ping=millis();
    }

  }

};

Msg msg;

#endif