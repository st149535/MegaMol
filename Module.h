/*
 * Module.h
 *
 * Copyright (C) 2009 by VISUS (Universitaet Stuttgart).
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOLCORE_MODULE_H_INCLUDED
#define MEGAMOLCORE_MODULE_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "api/MegaMolCore.std.h"
#include "AbstractNamedObjectContainer.h"


namespace megamol {
namespace core {

    /** forward declaration */
    class CoreInstance;
    class AbstractSlot;
    class ModuleDescription;


    /**
     * Base class of all graph modules
     */
    class MEGAMOLCORE_API Module: public AbstractNamedObjectContainer {
    public:
        friend class ::megamol::core::ModuleDescription;

        /**
         * Ctor.
         *
         * Be aware of the fact that most of your initialisation code should
         * be placed in 'create' since the ctor cannot fail and important
         * members (such as 'instance') are set after the ctor returns.
         */
        Module(void);

        /** Dtor. */
        virtual ~Module(void);

        /**
         * Tries to create this module. Do not overwrite this method!
         * Overwrite 'create'!
         *
         * @return 'true' on success, 'false' otherwise.
         */
        bool Create(void);

        /**
         * Finds the slot with the given name.
         *
         * @param name The name of the slot to find (case sensitive!)
         *
         * @return The found slot of this module, or 'NULL' if there is no
         *         slot with this name.
         */
        AbstractSlot * FindSlot(const vislib::StringA& name);

        /**
         * Gets the instance of the core owning this module.
         *
         * @return The instance of the core owning this module.
         */
        inline class ::megamol::core::CoreInstance *
        GetCoreInstance(void) const {
            ASSERT(this->coreInst != NULL);
            return this->coreInst;
        }

        /**
         * Gets the instance of the core owning this module.
         *
         * @return The instance of the core owning this module.
         */
        inline bool IsCoreInstanceAvailable(void) const {
            return (this->coreInst != NULL);
        }

        /**
         * Releases the module and all resources. Do not overwrite this method!
         * Overwrite 'release'!
         */
        void Release(void);

        /**
         * Clears the cleanup mark for this and all dependent objects.
         */
        virtual void ClearCleanupMark(void);

        /**
         * Performs the cleanup operation by removing and deleteing of all
         * marked objects.
         */
        virtual void PerformCleanup(void);

    protected:

        /**
         * Implementation of 'Create'.
         *
         * @return 'true' on success, 'false' otherwise.
         */
        virtual bool create(void) = 0;

        /**
         * Gets the instance of the core owning this module.
         *
         * @return The instance of the core owning this module.
         */
        inline class ::megamol::core::CoreInstance * instance(void) const {
            ASSERT(this->coreInst != NULL);
            return this->coreInst;
        }

        /**
         * Implementation of 'Release'.
         */
        virtual void release(void) = 0;

        /**
         * Makes the given slot for this module available.
         *
         * @param slot Slot to be made available.
         */
        void MakeSlotAvailable(AbstractSlot *slot);

    private:

        /** Sets the name of the module */
        void setModuleName(const vislib::StringA& name);

        /** The owning core instance */
        class ::megamol::core::CoreInstance *coreInst;

        /** Flag whether this module is created or not */
        bool created;

    };


} /* end namespace core */
} /* end namespace megamol */

#endif /* MEGAMOLCORE_MODULE_H_INCLUDED */
