#include "messaging.h"
#include "aprs_utils.h"
#include "display_utils.h"
#include <vector>

namespace Messaging {

std::vector<APRSMessage> history;
static String  replyTarget;
static int     msgIDCounter = 1;

void setup() {
    history.reserve(50);
}

void loop() {
    // Prune history beyond 100 messages
    if (history.size() > 100)
        history.erase(history.begin(), history.begin() + (history.size() - 100));
}

void receive(const String& from, const String& text, const String& msgID) {
    APRSMessage m;
    m.from     = from;
    m.text     = text;
    m.msgID    = msgID;
    m.ts       = millis();
    m.outgoing = false;
    history.push_back(m);
    replyTarget = from; // auto-set reply target to last sender
}

void markSent(const String& to, const String& text) {
    APRSMessage m;
    m.to       = to;
    m.text     = text;
    m.ts       = millis();
    m.outgoing = true;
    history.push_back(m);
}

String getReplyTarget()              { return replyTarget; }
void   setReplyTarget(const String& call) { replyTarget = call; }
int    nextMsgID()                   { return msgIDCounter++; }

} // namespace Messaging
