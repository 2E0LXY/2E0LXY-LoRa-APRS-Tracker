#include "setup_view.h"
#include "configuration.h"
#include "display_utils.h"

namespace {
    // Indices into fields() with fixed meaning, so handleKey() knows what
    // to write back to Config on confirm without string-matching labels.
    enum FieldIndex {
        F_CALLSIGN, F_SSID, F_WIFI_SSID, F_WIFI_PASS,
        F_TX_ENABLED, F_AUDIO_ENABLED, F_AUDIO_VOLUME,
        F_COUNT
    };

    std::vector<SetupField> fieldList;
    int  selected = 0;
    bool editing = false;
    String editBuf;

    void syncFromConfig() {
        fieldList.clear();
        fieldList.push_back({"Callsign",     FieldType::TEXT,     Config.aprs.callsign});
        fieldList.push_back({"SSID (-N)",    FieldType::NUMBER,   String(Config.aprs.ssid)});
        fieldList.push_back({"WiFi Network", FieldType::TEXT,     Config.wifi.ssid});
        fieldList.push_back({"WiFi Password",FieldType::PASSWORD, Config.wifi.password});
        fieldList.push_back({"TX Enabled",   FieldType::TOGGLE,   Config.region.txConfirmed ? "ON" : "OFF"});
        fieldList.push_back({"Message Sound",FieldType::TOGGLE,   Config.audio.enabled ? "ON" : "OFF"});
        fieldList.push_back({"Sound Volume", FieldType::NUMBER,   String(Config.audio.volume)});
    }

    void writeBack(int idx, const String& val) {
        switch (idx) {
            case F_CALLSIGN:
                Config.aprs.callsign = val;
                Config.aprs.callsign.toUpperCase();
                Config.aprs.passcode = calcPasscode(Config.aprs.callsign);
                break;
            case F_SSID: {
                int n = val.toInt();
                Config.aprs.ssid = constrain(n, 0, 15);
                break;
            }
            case F_WIFI_SSID:  Config.wifi.ssid = val; break;
            case F_WIFI_PASS:  Config.wifi.password = val; break;
            case F_AUDIO_VOLUME: {
                int n = val.toInt();
                Config.audio.volume = constrain(n, 0, 100);
                break;
            }
            default: break;   // toggles handled directly in handleKey(), not via text entry
        }
        saveConfig();
        syncFromConfig();
    }
}

void Setup_View::setup() {
    syncFromConfig();
}

void Setup_View::enter() {
    syncFromConfig();
    selected = 0;
    editing = false;
    editBuf = "";
}

const std::vector<SetupField>& Setup_View::fields() { return fieldList; }
int  Setup_View::selectedIndex() { return selected; }
bool Setup_View::isEditing()     { return editing; }
String Setup_View::editBuffer()  { return editBuf; }

void Setup_View::handleKey(char key) {
    bool isUp   = (key == 'i' || key == 'I');   // reuse the map's I/O pan
    bool isDown = (key == 'o' || key == 'O') && !editing;  // 'O' only navigates outside edit mode; inside edit mode it's a letter to type
    bool isDelete = (key == '\b' || key == 0x08 || key == 0x7F);
    bool isEnter  = (key == '\r' || key == '\n');

    if (editing) {
        if (isEnter) {
            writeBack(selected, editBuf);
            editing = false;
            editBuf = "";
            return;
        }
        if (isDelete) {
            if (editBuf.length() > 0) editBuf.remove(editBuf.length() - 1);
            return;
        }
        if (key >= 0x20 && key < 0x7F && editBuf.length() < 40) {
            editBuf += key;
        }
        return;
    }

    // Not editing: navigate or toggle/enter-edit
    if (isUp)   { selected = (selected - 1 + F_COUNT) % F_COUNT; return; }
    if (isDown) { selected = (selected + 1) % F_COUNT; return; }

    if (isEnter) {
        auto& f = fieldList[selected];
        if (f.type == FieldType::TOGGLE) {
            if (selected == F_TX_ENABLED) {
                Config.region.txConfirmed = !Config.region.txConfirmed;
            } else if (selected == F_AUDIO_ENABLED) {
                Config.audio.enabled = !Config.audio.enabled;
            }
            saveConfig();
            syncFromConfig();
        } else {
            editing = true;
            editBuf = (f.type == FieldType::PASSWORD) ? "" : f.value;   // don't prefill a password for editing-in-place surprise
        }
        return;
    }
}
