/*
 * AbstractUILayer.cpp
 *
 * Copyright (C) 2016 MegaMol Team
 * Alle Rechte vorbehalten. All rights reserved.
 */
#pragma once

#include "../../include/mmcore/view/AbstractInputScope.h"

namespace megamol {
namespace render_api {

class AbstractUILayer : public megamol::core::view::AbstractInputScope {
public:
    virtual bool Enabled() { return true; }

    virtual void OnResize(int w, int h) {}
    virtual void OnDraw() {}

protected:
    AbstractUILayer() {}
    virtual ~AbstractUILayer() = default;
};

} /* end namespace render_api */
} /* end namespace megamol */
