/*
 * AbstractParticleManipulator.h
 *
 * Copyright (C) 2019 MegaMol Dev Team
 * Alle Rechte vorbehalten.
 */
#pragma once

#include "geometry_calls/MultiParticleDataCall.h"
#include "mmstd_datatools/AbstractManipulator.h"

namespace megamol {
namespace stdplugin {
namespace datatools {

using AbstractParticleManipulator = AbstractManipulator<geocalls::MultiParticleDataCall>;

} /* end namespace datatools */
} /* end namespace stdplugin */
} /* end namespace megamol */
