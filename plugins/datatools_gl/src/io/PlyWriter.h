/*
 * PlyWriter.h
 *
 * Copyright (C) 2017 by MegaMol Team
 * Alle Rechte vorbehalten.
 */
#ifndef MEGAMOL_DATATOOLS_IO_PLYWRITER_H_INCLUDED
#define MEGAMOL_DATATOOLS_IO_PLYWRITER_H_INCLUDED
#pragma once

#include "geometry_calls_gl/CallTriMeshDataGL.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/param/ParamSlot.h"
#include "mmstd/data/AbstractDataWriter.h"
#include "vislib/sys/FastFile.h"

namespace megamol {
namespace datatools_gl {
namespace io {

class PlyWriter : public core::AbstractDataWriter {
public:
    /**
     * Answer the name of this module.
     *
     * @return The name of this module.
     */
    static const char* ClassName(void) {
        return "PlyWriter";
    }

    /**
     * Answer a human readable description of this module.
     *
     * @return A human readable description of this module.
     */
    static const char* Description(void) {
        return "Polygon file format (.ply) file writer";
    }

    /**
     * Answers whether this module is available on the current system.
     *
     * @return 'true' if the module is available, 'false' otherwise.
     */
    static bool IsAvailable(void) {
        return true;
    }

    /** Ctor. */
    PlyWriter(void);

    /** Dtor. */
    ~PlyWriter(void) override;

protected:
    /**
     * Implementation of 'Create'.
     *
     * @return 'true' on success, 'false' otherwise.
     */
    bool create(void) override;

    /**
     * Implementation of 'Release'.
     */
    void release(void) override;

    /**
     * The main function
     *
     * @return True on success
     */
    bool run(void) override;

    /**
     * Function querying the writers capabilities
     *
     * @param call The call to receive the capabilities
     *
     * @return True on success
     */
    bool getCapabilities(core::DataWriterCtrlCall& call) override;

private:
    /** The file name of the file to be written */
    core::param::ParamSlot filenameSlot;

    /** The frame ID of the frame to be written */
    core::param::ParamSlot frameIDSlot;

    /** The slot asking for data. */
    core::CallerSlot meshDataSlot;
};

} /* end namespace io */
} // namespace datatools_gl
} /* end namespace megamol */

#endif // !MEGAMOL_DATATOOLS_IO_PLYWRITER_H_INCLUDED
