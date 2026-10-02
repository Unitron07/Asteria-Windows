#pragma once
#include <array>

namespace PyroWave {
// One producer, one presenter, externally serialized. At most one pending frame.
// A retained displayed frame is never overwritten during an overlay-only redraw.
class FrameSlots {
public:
    static constexpr int Count = 3;
    enum class State { Free, Decoding, Pending, Rendering, Displayed };
    int reserve(bool& dropped) {
        dropped = false;
        for (int i=0;i<Count;++i) if (m_State[i]==State::Free) {
            m_State[i]=State::Decoding; return i;
        }
        if (m_Pending>=0) {
            const int i=m_Pending; m_Pending=-1;
            m_State[i]=State::Decoding; dropped=true; return i;
        }
        return -1;
    }
    bool publish(int i) {
        const bool dropped=m_Pending>=0;
        if (dropped) m_State[m_Pending]=State::Free;
        m_Pending=i; m_State[i]=State::Pending;
        return dropped;
    }
    void cancel(int i) { m_State[i]=State::Free; }
    int take() {
        const int i=m_Pending; m_Pending=-1;
        if (i>=0) m_State[i]=State::Rendering;
        return i;
    }
    void displayed(int i) {
        if (m_Displayed>=0) m_State[m_Displayed]=State::Free;
        m_Displayed=i; m_State[i]=State::Displayed;
    }
    int current() const { return m_Displayed; }
    State state(int i) const { return m_State[i]; }
private:
    std::array<State,Count> m_State{};
    int m_Pending=-1,m_Displayed=-1;
};
}
