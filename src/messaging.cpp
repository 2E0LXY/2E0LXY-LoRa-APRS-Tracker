#include "messaging.h"
#include <Arduino.h>

namespace Messaging {

std::vector<APRSMessage> history;
static String  replyTarget;
static int     msgCounter = 0;

void setup() { history.reserve(50); }
void loop()  {}

void receive(const String& from, const String& text, const String& msgID) {
    for (auto& m : history)
        if (m.from == from && m.msgID == msgID) return;
    APRSMessage msg { from, "", text, msgID, millis(), false };
    if (history.size() >= 100) history.erase(history.begin());
    history.push_back(msg);
    replyTarget = from;
}

void markSent(const String& to, const String& text) {
    APRSMessage msg { "", to, text, String(msgCounter), millis(), true };
    if (history.size() >= 100) history.erase(history.begin());
    history.push_back(msg);
}

String getReplyTarget()               { return replyTarget; }
void   setReplyTarget(const String& c){ replyTarget = c; }
int    nextMsgID()                    { return ++msgCounter; }

} // namespace Messaging
