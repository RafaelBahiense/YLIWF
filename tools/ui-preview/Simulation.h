#pragma once

namespace preview {
    struct Session;
    enum class Scenario { Party, Empty, QuestLocked, Busy, Error, NoGame };
    void Reset(Scenario scenario);
    void Tick();
    void CaptureKey(unsigned int key);
    void SaveSession(Session& session);
    void RestoreSession(const Session& session);
}
