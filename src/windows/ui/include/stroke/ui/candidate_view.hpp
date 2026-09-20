#pragma once

#include "stroke/engine/session.hpp"

namespace stroke {

// View owns a snapshot copy, never mutable engine state. UI thread only.
// Future mouse events carry SelectCandidate {revision, index} back to the controller.
class ICandidateView {
public:
    virtual ~ICandidateView() = default;
    virtual void present(SessionSnapshot snapshot) = 0;
    virtual void hide() noexcept = 0;
};

} // namespace stroke
