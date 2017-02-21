/*
* CallOSPRayLight.cpp
*
* Copyright (C) 2017 by Universitaet Stuttgart (VISUS).
* Alle Rechte vorbehalten.
*/

#include "stdafx.h"
#include "CallOSPRayLight.h"
#include "vislib/sys/Log.h"

using namespace megamol::ospray;

OSPRayLightContainer::OSPRayLightContainer() :
    // General light parameters
    lightType(lightenum::NONE),
    lightColor(NULL),
    lightIntensity(0.0f),
    // Distant light parameters
    dl_direction(NULL),
    dl_angularDiameter(0.0f),
    dl_eye_direction(false),
    // point light paramenters
    pl_position(NULL),
    pl_radius(0.0f),
    // spot light parameters
    sl_position(NULL),
    sl_direction(NULL),
    sl_openingAngle(0.0f),
    sl_penumbraAngle(0.0f),
    sl_radius(0.0f),
    // quad light parameters
    ql_position(NULL),
    ql_edgeOne(NULL),
    ql_edgeTwo(NULL),
    // hdri light parameters
    hdri_up(NULL),
    hdri_direction(NULL),
    hdri_evnfile(""),
    // tracks the existence of the light module
    isValid(false) { }

OSPRayLightContainer::~OSPRayLightContainer() {
    this->release();
};

void OSPRayLightContainer::release() {
    // General light parameters
    lightColor.clear();
    // Distant light parameters
    dl_direction.clear();
    // point light paramenters
    pl_position.clear();
    // spot light parameters
    sl_position.clear();
    sl_direction.clear();
    // quad light parameters
    ql_position.clear();
    ql_edgeOne.clear();
    ql_edgeTwo.clear();
    // hdri light parameters
    hdri_up.clear();
    hdri_direction.clear();
    hdri_evnfile.Clear();
}


// #############################
// ###### CallOSPRayLight ######
// #############################
/*
* megamol::ospray::CallOSPRayLight::CallOSPRayLight
*/
CallOSPRayLight::CallOSPRayLight() { 
// intentionally empty
}



void CallOSPRayLight::setLightMap(std::map<CallOSPRayLight*, OSPRayLightContainer> *lm) {
    this->lightMap = lm;
}

/*
* megamol::ospray::CallOSPRayLight::~CallOSPRayLight
*/
CallOSPRayLight::~CallOSPRayLight(void) {
    // intentionally empty
}

/*
* megamol::ospray::CallOSPRayLight::operator=
*/
CallOSPRayLight& CallOSPRayLight::operator=(const CallOSPRayLight& rhs) {
    return *this;
}

void CallOSPRayLight::addLight(OSPRayLightContainer &lc) {
    if (lc.isValid) {
        this->lightMap->insert_or_assign(this, lc);
    } 
}

void CallOSPRayLight::fillLightMap() {
    if (!(*this)(0)) {
        vislib::sys::Log::DefaultLog.WriteError("Error in fillLightMap");
    }
}

void CallOSPRayLight::setDirtyObj(bool *md) {
    this->ModuleIsDirty = md;
}
