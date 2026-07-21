#pragma once
#include <Arduino.h>
#include <vector>

struct APRSMessage {
    String from, to, text, msgID;
    uint32_t ts;
    bool outgoing;
};

namespace Messaging {
    extern std::vector<APRSMessage> history;
    void   setup();
    void   loop();
    void   receive(const String& from, const String& text, const String& msgID);
    void   markSent(const String& to, const String& text);
    String getReplyTarget();
    void   setReplyTarget(const String& call);
    int    nextMsgID();
}
