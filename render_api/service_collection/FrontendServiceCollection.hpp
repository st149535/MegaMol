/*
 * FrontendServiceCollection.hpp
 *
 * Copyright (C) 2019 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#pragma once

#include "AbstractFrontendService.hpp"

#include <vector>

namespace megamol {
namespace frontend {

// provides abstraction to call AbstractFrontendService methods on a collection of services at once
class FrontendServiceCollection {
public:

    FrontendServiceCollection() = default;
    ~FrontendServiceCollection() = default;

	void add(AbstractFrontendService& service, void* service_config);

    bool init();
    void close();

	bool assignRequestedResources();
    std::vector<ModuleResource>& getProvidedResources();
	
    void updateProvidedResources();
    void digestChangedRequestedResources();
    void resetProvidedResources();

    void preGraphRender();
    void postGraphRender();

    bool shouldShutdown() const;

private:

	void sortServices();
	ModuleResource* findResource(std::string const& name);

	struct ServiceEntry {
        AbstractFrontendService* service = nullptr;
        void* service_config = nullptr;

		 AbstractFrontendService const& get() const { return *service; }
		 AbstractFrontendService& get() { return *service; }
	};

    std::vector<ServiceEntry> m_services;
    std::vector<ModuleResource> m_serviceResources;
};

} // namespace frontend
} // namespace megamol
