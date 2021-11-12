/*
 * ParticleDataSequenceConcatenate.h
 *
 * Copyright (C) 2015 by TU Dresden (S. Grottel)
 * Alle Rechte vorbehalten.
 */

#pragma once

#include "mmcore/Module.h"
#include "mmcore/CalleeSlot.h"
#include "mmcore/CallerSlot.h"

namespace megamol {
namespace datatools {

    class ParticleDataSequenceConcatenate : public core::Module {
    public:
        static const char *ClassName(void) {
            return "ParticleDataSequenceConcatenate";
        }
        static const char *Description(void) {
            return "Concatenates data from two MultiParticleList data source modules";
        }
        static bool IsAvailable(void) {
            return true;
        }
        ParticleDataSequenceConcatenate();
        virtual ~ParticleDataSequenceConcatenate();
    protected:
        virtual bool create(void);
        virtual void release(void);
    private:
        bool getExtend(megamol::core::Call& c);
        bool getData(megamol::core::Call& c);
        core::CalleeSlot dataOutSlot;
        core::CallerSlot dataIn1Slot;
        core::CallerSlot dataIn2Slot;
    };

}
}