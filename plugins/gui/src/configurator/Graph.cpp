/*
 * Graph.cpp
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "Graph.h"


using namespace megamol;
using namespace megamol::gui;
using namespace megamol::gui::configurator;


ImGuiID megamol::gui::configurator::Graph::generated_uid = 0; // must be greater than or equal to zero


megamol::gui::configurator::Graph::Graph(const std::string& graph_name)
    : uid(this->generate_unique_id())
    , name(graph_name)
    , group_name_uid(0)
    , modules()
    , calls()
    , groups()
    , dirty_flag(true)
    , present() {}


megamol::gui::configurator::Graph::~Graph(void) {}


ImGuiID megamol::gui::configurator::Graph::AddModule(
    const ModuleStockVectorType& stock_modules, const std::string& module_class_name) {

    try {
        for (auto& mod : stock_modules) {
            if (module_class_name == mod.class_name) {
                ImGuiID mod_uid = this->generate_unique_id();
                auto mod_ptr = std::make_shared<Module>(mod_uid);
                mod_ptr->class_name = mod.class_name;
                mod_ptr->description = mod.description;
                mod_ptr->plugin_name = mod.plugin_name;
                mod_ptr->is_view = mod.is_view;
                mod_ptr->name = this->generate_unique_module_name(mod.class_name);
                mod_ptr->is_view_instance = false;
                mod_ptr->GUI_SetLabelVisibility(this->present.GetModuleLabelVisibility());

                for (auto& p : mod.parameters) {
                    Parameter param_slot(this->generate_unique_id(), p.type, p.storage, p.minval, p.maxval);
                    param_slot.full_name = p.full_name;
                    param_slot.description = p.description;
                    param_slot.SetValueString(p.default_value, true);
                    param_slot.GUI_SetVisibility(p.gui_visibility);
                    param_slot.GUI_SetReadOnly(p.gui_read_only);
                    param_slot.GUI_SetPresentation(p.gui_presentation);
                    // Apply current global configurator parameter gui settings
                    // Do not apply global read-only and visibility.
                    param_slot.GUI_SetExpert(this->present.param_expert_mode);

                    mod_ptr->parameters.emplace_back(param_slot);
                }

                for (auto& callslots_type : mod.callslots) {
                    for (auto& c : callslots_type.second) {
                        CallSlot callslot(this->generate_unique_id());
                        callslot.name = c.name;
                        callslot.description = c.description;
                        callslot.compatible_call_idxs = c.compatible_call_idxs;
                        callslot.type = c.type;
                        callslot.GUI_SetLabelVisibility(this->present.GetCallSlotLabelVisibility());

                        mod_ptr->AddCallSlot(std::make_shared<CallSlot>(callslot));
                    }
                }

                for (auto& callslot_type_list : mod_ptr->GetCallSlots()) {
                    for (auto& callslot : callslot_type_list.second) {
                        callslot->ConnectParentModule(mod_ptr);
                    }
                }

                this->modules.emplace_back(mod_ptr);
                vislib::sys::Log::DefaultLog.WriteInfo(
                    "[Configurator] Added module '%s' (uid %i) to project '%s'.\n", mod_ptr->class_name.c_str(), mod_ptr->uid, this->name.c_str());

                this->dirty_flag = true;

                return mod_uid;
            }
        }
    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return GUI_INVALID_ID;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return GUI_INVALID_ID;
    }

    vislib::sys::Log::DefaultLog.WriteError("Unable to find module in stock: %s [%s, %s, line %d]\n",
        module_class_name.c_str(), __FILE__, __FUNCTION__, __LINE__);
    return GUI_INVALID_ID;
}


bool megamol::gui::configurator::Graph::DeleteModule(ImGuiID module_uid) {

    try {
        for (auto iter = this->modules.begin(); iter != this->modules.end(); iter++) {
            if ((*iter)->uid == module_uid) {

                // First reset module and call slot pointers in groups
                for (auto& group_ptr : this->groups) {
                    if (group_ptr->ContainsModule(module_uid)) {
                        group_ptr->RemoveModule(module_uid);
                    }
                    if (group_ptr->Empty()) {
                        this->DeleteGroup(group_ptr->uid);
                    }
                }

                // Second remove call slots
                (*iter)->RemoveAllCallSlots();
                
                // Delete calls which are no longer connected
                this->delete_disconnected_calls();
                
                if ((*iter).use_count() > 1) {
                    vislib::sys::Log::DefaultLog.WriteError(
                        "Unclean deletion. Found %i references pointing to module. [%s, %s, line %d]\n",
                        (*iter).use_count(), __FILE__, __FUNCTION__, __LINE__);
                }

                vislib::sys::Log::DefaultLog.WriteInfo(
                    "[Configurator] Deleted module '%s' (uid %i) from  project '%s'.\n", (*iter)->class_name.c_str(), (*iter)->uid, this->name.c_str());
                (*iter).reset();
                this->modules.erase(iter);

                this->dirty_flag = true;
                return true;
            }
        }

    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    vislib::sys::Log::DefaultLog.WriteWarn("Invalid module uid. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
    return false;
}


bool megamol::gui::configurator::Graph::GetModule(
    ImGuiID module_uid, megamol::gui::configurator::ModulePtrType& out_module_ptr) {

    if (module_uid != GUI_INVALID_ID) {
        for (auto& module_ptr : this->modules) {
            if (module_ptr->uid == module_uid) {
                out_module_ptr = module_ptr;
                return true;
            }
        }
    }
    return false;
}


bool megamol::gui::configurator::Graph::AddCall(
    const CallStockVectorType& stock_calls, CallSlotPtrType callslot_1, CallSlotPtrType callslot_2) {

    try {
        auto compat_idx = CallSlot::GetCompatibleCallIndex(callslot_1, callslot_2);
        if (compat_idx == GUI_INVALID_ID) {
            vislib::sys::Log::DefaultLog.WriteWarn(
                "Unable to find compatible call. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }
        Call::StockCall call_stock_data = stock_calls[compat_idx];

        auto call_ptr = std::make_shared<Call>(this->generate_unique_id());
        call_ptr->class_name = call_stock_data.class_name;
        call_ptr->description = call_stock_data.description;
        call_ptr->plugin_name = call_stock_data.plugin_name;
        call_ptr->functions = call_stock_data.functions;
        call_ptr->GUI_SetLabelVisibility(this->present.GetCallLabelVisibility());

        // Delete calls when CALLERs should be connected to new call slot
        if ((callslot_1->type == CallSlotType::CALLER) && (callslot_1->CallsConnected())) {
            std::vector<ImGuiID> calls_uids;
            for (auto& call : callslot_1->GetConnectedCalls()) {
                calls_uids.emplace_back(call->uid);
            }
            for (auto& call_uid : calls_uids) {
                this->DeleteCall(call_uid);
            }
        }
        if ((callslot_2->type == CallSlotType::CALLER) && (callslot_2->CallsConnected())) {
            std::vector<ImGuiID> calls_uids;
            for (auto& call : callslot_2->GetConnectedCalls()) {
                calls_uids.emplace_back(call->uid);
            }
            for (auto& call_uid : calls_uids) {
                this->DeleteCall(call_uid);
            }
        }

        if (call_ptr->ConnectCallSlots(callslot_1, callslot_2) && callslot_1->ConnectCall(call_ptr) &&
            callslot_2->ConnectCall(call_ptr)) {

            this->calls.emplace_back(call_ptr);
            vislib::sys::Log::DefaultLog.WriteInfo(
                "[Configurator] Added call '%s' (uid %i) to project '%s'.\n", call_ptr->class_name.c_str(), call_ptr->uid, this->name.c_str());

            // Add connected call slots to interface of group of the parent module
            if (callslot_1->IsParentModuleConnected() && callslot_2->IsParentModuleConnected()) {
                ImGuiID slot_1_parent_group_uid = callslot_1->GetParentModule()->GUI_GetGroupUID();
                ImGuiID slot_2_parent_group_uid = callslot_2->GetParentModule()->GUI_GetGroupUID();
                if (slot_1_parent_group_uid != slot_2_parent_group_uid) {
                    if ((slot_1_parent_group_uid != GUI_INVALID_ID) && !(callslot_1->GUI_IsGroupInterface())) {
                        for (auto& group_ptr : this->groups) {
                            if (group_ptr->uid == slot_1_parent_group_uid) {
                                group_ptr->InterfaceSlot_AddCallSlot(callslot_1, this->generate_unique_id());
                            }
                        }
                    }
                    if ((slot_2_parent_group_uid != GUI_INVALID_ID) && !(callslot_2->GUI_IsGroupInterface())) {
                        for (auto& group_ptr : this->groups) {
                            if (group_ptr->uid == slot_2_parent_group_uid) {
                                group_ptr->InterfaceSlot_AddCallSlot(callslot_2, this->generate_unique_id());
                            }
                        }
                    }
                }
            }

            this->dirty_flag = true;
        } else {
            this->DeleteCall(call_ptr->uid);
            vislib::sys::Log::DefaultLog.WriteWarn("Unable to connect call: %s [%s, %s, line %d]\n",
                call_ptr->class_name.c_str(), __FILE__, __FUNCTION__, __LINE__);
            return false;
        }
    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


bool megamol::gui::configurator::Graph::AddCall(const CallStockVectorType& stock_calls, ImGuiID slot_1_uid, ImGuiID slot_2_uid) {
    
    try {
        if ((slot_1_uid == GUI_INVALID_ID) || (slot_2_uid == GUI_INVALID_ID)) {
            vislib::sys::Log::DefaultLog.WriteError("Invalid slot uid given. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }
        
        CallSlotPtrType drag_callslot_ptr;
        CallSlotPtrType drop_callslot_ptr;
        for (auto& module_ptr : this->modules) {
            CallSlotPtrType callslot_ptr;
            if (module_ptr->GetCallSlot(slot_1_uid, callslot_ptr)) {
                drag_callslot_ptr = callslot_ptr;
            }
            if (module_ptr->GetCallSlot(slot_2_uid, callslot_ptr)) {

                drop_callslot_ptr = callslot_ptr;
            }
        }
        
        InterfaceSlotPtrType drag_interfaceslot_ptr;
        InterfaceSlotPtrType drop_interfaceslot_ptr;
        for (auto& group_ptr : this->groups) {
            InterfaceSlotPtrType interfaceslot_ptr;
            if (group_ptr->GetInterfaceSlot(slot_1_uid, interfaceslot_ptr)) {
                drag_interfaceslot_ptr = interfaceslot_ptr;
            }
            if (group_ptr->GetInterfaceSlot(slot_2_uid, interfaceslot_ptr)) {
                drop_interfaceslot_ptr = interfaceslot_ptr;
            }
        }
            
        // CallSlot <-> CallSlot
        if ((drag_callslot_ptr != nullptr) && (drop_callslot_ptr != nullptr)) {
            this->AddCall(stock_calls, drag_callslot_ptr, drop_callslot_ptr);
        }
        // InterfaceSlot <-> CallSlot
        else if (((drag_interfaceslot_ptr != nullptr) && (drop_callslot_ptr != nullptr)) || 
            ((drag_callslot_ptr != nullptr) && (drop_interfaceslot_ptr != nullptr))) {
                
             InterfaceSlotPtrType interface_ptr = (drag_interfaceslot_ptr != nullptr) ? (drag_interfaceslot_ptr) : (drop_interfaceslot_ptr);
             CallSlotPtrType callslot_ptr = (drop_callslot_ptr != nullptr) ? (drop_callslot_ptr) : (drag_callslot_ptr);
             
            ImGuiID interfaceslot_group_uid = interface_ptr->GUI_GetGroupUID();                         
            ImGuiID callslot_group_uid = GUI_INVALID_ID;
            if (callslot_ptr->IsParentModuleConnected()) {
                callslot_group_uid = callslot_ptr->GetParentModule()->GUI_GetGroupUID();
            }

            if (!callslot_ptr->GUI_IsGroupInterface()) {
        
                if ((interfaceslot_group_uid == callslot_group_uid) && interface_ptr->IsCallSlotCompatible((*callslot_ptr))) {
                    if (interface_ptr->AddCallSlot(callslot_ptr, interface_ptr)) {
                        CallSlotType compatible_callslot_type = (interface_ptr->GetCallSlotType() == CallSlotType::CALLEE) ? (CallSlotType::CALLER): (CallSlotType::CALLEE);
                        // Add calls to all call slots the call slots of the interface are connected to.
                        /// XXX ?! One is sufficient since all call slots are connected to the same call slot.
                        CallSlotPtrType connect_callslot_ptr;
                        for (auto& interface_callslots_ptr : interface_ptr->GetCallSlots()) {
                            if (interface_callslots_ptr->uid != callslot_ptr->uid) {
                                for (auto& call_ptr : interface_callslots_ptr->GetConnectedCalls()) {
                                    connect_callslot_ptr = call_ptr->GetCallSlot(compatible_callslot_type);
                                }
                            }
                        }
                        if (connect_callslot_ptr != nullptr) {
                            if (!this->AddCall(stock_calls, callslot_ptr, connect_callslot_ptr)) {
                                interface_ptr->RemoveCallSlot(callslot_ptr->uid);
                            }
                        }
                    }
                }
                else if (interfaceslot_group_uid != callslot_group_uid) {
                    for (auto& interface_callslots_ptr : interface_ptr->GetCallSlots()) {
                        this->AddCall(stock_calls, callslot_ptr, interface_callslots_ptr);
                    }
                }
            }
        }
        // InterfaceSlot <-> InterfaceSlot
        else if ((drag_interfaceslot_ptr != nullptr) && (drop_interfaceslot_ptr != nullptr)) {
            
            ImGuiID drag_interfaceslot_group_uid = drag_interfaceslot_ptr->GUI_GetGroupUID(); 
            ImGuiID drop_interfaceslot_group_uid = drop_interfaceslot_ptr->GUI_GetGroupUID(); 
                                  
            if (drag_interfaceslot_group_uid != drop_interfaceslot_group_uid) {
                for (auto& drag_interface_callslots_ptr : drag_interfaceslot_ptr->GetCallSlots()) {
                    for (auto& drop_interface_callslots_ptr : drop_interfaceslot_ptr->GetCallSlots()) {
                        this->AddCall(stock_calls, drag_interface_callslots_ptr, drop_interface_callslots_ptr);
                    }
                }
            }
        }
    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;    
}


bool megamol::gui::configurator::Graph::DeleteCall(ImGuiID call_uid) {

    try {
        std::vector<ImGuiID> delete_calls_uids;
        delete_calls_uids.emplace_back(call_uid);
                
        // Also delete other calls, which are connceted to same interface slot and call slot
        ImGuiID caller_uid = GUI_INVALID_ID;
        ImGuiID callee_uid = GUI_INVALID_ID;
        for (auto& call_ptr : this->calls) {
            if (call_ptr->uid == call_uid) {             
                auto caller = call_ptr->GetCallSlot(CallSlotType::CALLER);
                auto callee = call_ptr->GetCallSlot(CallSlotType::CALLEE);                
                if (caller != nullptr) {
                    caller_uid = caller->uid;
                    if (caller->GUI_IsGroupInterface()) {
                        caller_uid = caller->GUI_GetGroupInterface()->uid;
                    }
                }
                if (callee != nullptr) {
                    if (callee->GUI_IsGroupInterface()) {
                        callee_uid = callee->uid;
                        if (callee->GUI_IsGroupInterface()) {
                            callee_uid = callee->GUI_GetGroupInterface()->uid;
                        }
                    }
                }            
            }
        }
        for (auto& call_ptr : this->calls) {
            if (call_ptr->uid != call_uid) { 
                bool caller_fits = false;
                bool callee_fits = false;
                auto caller = call_ptr->GetCallSlot(CallSlotType::CALLER);
                auto callee = call_ptr->GetCallSlot(CallSlotType::CALLEE);     
                if (caller != nullptr) {
                    if (caller->GUI_IsGroupInterface()) {
                        caller_fits = (caller_uid == caller->GUI_GetGroupInterface()->uid);
                    }
                    else {
                        caller_fits = (caller_uid == caller->uid);
                    }
                }
                if (callee != nullptr) {
                    if (callee->GUI_IsGroupInterface()) {
                        callee_fits = (callee_uid == callee->GUI_GetGroupInterface()->uid);
                    }
                    else {
                        callee_fits = (callee_uid == callee->uid);
                    }
                } 
                if (caller_fits && callee_fits) {
                    delete_calls_uids.emplace_back(call_ptr->uid);
                }
            }
        }
        
        // Actual deletion of calls
        for (auto& delete_call_uid : delete_calls_uids) {
            for (auto iter = this->calls.begin(); iter != this->calls.end(); iter++) {
                if ((*iter)->uid == delete_call_uid) {
                    
                    (*iter)->DisconnectCallSlots();

                    if ((*iter).use_count() > 1) {
                        vislib::sys::Log::DefaultLog.WriteError(
                            "Unclean deletion. Found %i references pointing to call. [%s, %s, line %d]\n",
                            (*iter).use_count(), __FILE__, __FUNCTION__, __LINE__);
                    }

                    vislib::sys::Log::DefaultLog.WriteInfo("[Configurator] Deleted call '%s' (uid %i) from  project '%s'.\n",
                        (*iter)->class_name.c_str(), (*iter)->uid, this->name.c_str());
                    (*iter).reset();
                    this->calls.erase(iter);

                    this->dirty_flag = true;
                    break;
                }
            }
        }
    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    return true;
}


ImGuiID megamol::gui::configurator::Graph::AddGroup(const std::string& group_name) {

    try {
        ImGuiID group_id = this->generate_unique_id();
        auto group_ptr = std::make_shared<Group>(group_id);
        group_ptr->name = (group_name.empty()) ? (this->generate_unique_group_name()) : (group_name);
        this->groups.emplace_back(group_ptr);

        vislib::sys::Log::DefaultLog.WriteInfo(
            "[Configurator] Added group '%s' (uid %i) to project '%s'.\n", group_ptr->name.c_str(), group_ptr->uid, this->name.c_str());
        return group_id;

    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return GUI_INVALID_ID;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return GUI_INVALID_ID;
    }

    return GUI_INVALID_ID;
}


bool megamol::gui::configurator::Graph::DeleteGroup(ImGuiID group_uid) {

    try {
        for (auto iter = this->groups.begin(); iter != this->groups.end(); iter++) {
            if ((*iter)->uid == group_uid) {

                if ((*iter).use_count() > 1) {
                    vislib::sys::Log::DefaultLog.WriteError(
                        "Unclean deletion. Found %i references pointing to group. [%s, %s, line %d]\n",
                        (*iter).use_count(), __FILE__, __FUNCTION__, __LINE__);
                }

                vislib::sys::Log::DefaultLog.WriteInfo("[Configurator] Deleted group '%s' (uid %i) from  project '%s'.\n",
                    (*iter)->name.c_str(), (*iter)->uid, this->name.c_str());
                (*iter).reset();
                this->groups.erase(iter);

                this->present.ForceUpdate();
                return true;
            }
        }
    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    vislib::sys::Log::DefaultLog.WriteWarn("Invalid group uid. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
    return false;
}


ImGuiID megamol::gui::configurator::Graph::AddGroupModule(
    const std::string& group_name, const ModulePtrType& module_ptr) {

    try {
        // Only create new group if given name is not empty
        if (!group_name.empty()) {
            // Check if group with given name already exists
            ImGuiID existing_group_uid = GUI_INVALID_ID;
            for (auto& group_ptr : this->groups) {
                if (group_ptr->name == group_name) {
                    existing_group_uid = group_ptr->uid;
                }
            }
            // Create new group if there is no one with given name
            if (existing_group_uid == GUI_INVALID_ID) {
                existing_group_uid = this->AddGroup(group_name);
            }
            // Add module to group
            for (auto& group_ptr : this->groups) {
                if (group_ptr->uid == existing_group_uid) {
                    if (group_ptr->AddModule(module_ptr)) {
                        this->restore_callslots_interfaceslot_state(group_ptr->uid);
                        return existing_group_uid;
                    }
                }
            }
        }
    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return GUI_INVALID_ID;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return GUI_INVALID_ID;
    }

    return GUI_INVALID_ID;
}


bool megamol::gui::configurator::Graph::AddGroupInterfaceCallSlot(
    ImGuiID group_uid, const CallSlotPtrType& callslot_ptr) {

    GroupPtrType group_ptr;
    if (this->get_group(group_uid, group_ptr)) {
        return group_ptr->InterfaceSlot_AddCallSlot(callslot_ptr, this->generate_unique_id());
    }
    return false;
}


bool megamol::gui::configurator::Graph::UniqueModuleRename(const std::string& module_name) {

    for (auto& mod : this->modules) {
        if (module_name == mod->name) {
            mod->name = this->generate_unique_module_name(module_name);
            this->present.ForceUpdate();
            return true;
        }
    }
    return false;
}


bool megamol::gui::configurator::Graph::IsMainViewSet(void) {

    for (auto& mod : this->modules) {
        if (mod->is_view_instance) {
            return true;
        }
    }
    return false;
}


void megamol::gui::configurator::Graph::restore_callslots_interfaceslot_state(ImGuiID group_uid) {

    GroupPtrType group_ptr;
    if (this->get_group(group_uid, group_ptr)) {
        for (auto& module_ptr : group_ptr->GetModules()) {
            // Add connected call slots to group interface if connected module is not part of same group
            // CALLER
            for (auto& callerslot_ptr : module_ptr->GetCallSlots(CallSlotType::CALLER)) {
                if (callerslot_ptr->CallsConnected()) {
                    for (auto& call : callerslot_ptr->GetConnectedCalls()) {
                        auto calleeslot_ptr = call->GetCallSlot(CallSlotType::CALLEE);
                        if (calleeslot_ptr->IsParentModuleConnected()) {
                            ImGuiID parent_module_group_uid = calleeslot_ptr->GetParentModule()->GUI_GetGroupUID();
                            if (parent_module_group_uid != group_ptr->uid) {
                                group_ptr->InterfaceSlot_AddCallSlot(callerslot_ptr, this->generate_unique_id());
                            }
                        }
                    }
                }
            }
            // CALLEE
            for (auto& calleeslot_ptr : module_ptr->GetCallSlots(CallSlotType::CALLEE)) {
                if (calleeslot_ptr->CallsConnected()) {
                    for (auto& call : calleeslot_ptr->GetConnectedCalls()) {
                        auto callerslot_ptr = call->GetCallSlot(CallSlotType::CALLER);
                        if (callerslot_ptr->IsParentModuleConnected()) {
                            ImGuiID parent_module_group_uid = callerslot_ptr->GetParentModule()->GUI_GetGroupUID();
                            if (parent_module_group_uid != group_ptr->uid) {
                                group_ptr->InterfaceSlot_AddCallSlot(calleeslot_ptr, this->generate_unique_id());
                            }
                        }
                    }
                }
            }

            // Remove connected call slots of group interface if connected module is part of same group
            // CALLER
            for (auto& callerslot_ptr : module_ptr->GetCallSlots(CallSlotType::CALLER)) {
                if (callerslot_ptr->CallsConnected()) {
                    for (auto& call : callerslot_ptr->GetConnectedCalls()) {
                        auto calleeslot_ptr = call->GetCallSlot(CallSlotType::CALLEE);
                        if (calleeslot_ptr->IsParentModuleConnected()) {
                            ImGuiID parent_module_group_uid = calleeslot_ptr->GetParentModule()->GUI_GetGroupUID();
                            if (parent_module_group_uid == group_ptr->uid) {
                                group_ptr->InterfaceSlot_RemoveCallSlot(calleeslot_ptr->uid);
                            }
                        }
                    }
                }
            }
            // CALLEE
            for (auto& calleeslot_ptr : module_ptr->GetCallSlots(CallSlotType::CALLEE)) {
                if (calleeslot_ptr->CallsConnected()) {
                    for (auto& call : calleeslot_ptr->GetConnectedCalls()) {
                        auto callerslot_ptr = call->GetCallSlot(CallSlotType::CALLER);
                        if (callerslot_ptr->IsParentModuleConnected()) {
                            ImGuiID parent_module_group_uid = callerslot_ptr->GetParentModule()->GUI_GetGroupUID();
                            if (parent_module_group_uid == group_ptr->uid) {
                                group_ptr->InterfaceSlot_RemoveCallSlot(callerslot_ptr->uid);
                            }
                        }
                    }
                }
            }
        }
    }
}


bool megamol::gui::configurator::Graph::get_group(
    ImGuiID group_uid, megamol::gui::configurator::GroupPtrType& out_group_ptr) {

    if (group_uid != GUI_INVALID_ID) {
        for (auto& group_ptr : this->groups) {
            if (group_ptr->uid == group_uid) {
                out_group_ptr = group_ptr;
                return true;
            }
        }
    }
    return false;
}


bool megamol::gui::configurator::Graph::delete_disconnected_calls(void) {

    try {
        // Create separate uid list to avoid iterator conflict when operating on calls list while deleting.
        UIDVectorType call_uids;
        for (auto& call : this->calls) {
            if (!call->IsConnected()) {
                call_uids.emplace_back(call->uid);
            }
        }
        for (auto& id : call_uids) {
            this->DeleteCall(id);
        }
    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


const std::string megamol::gui::configurator::Graph::generate_unique_group_name(void) {

    int new_name_id = 0;
    std::string new_name_prefix = "Group_";
    for (auto& group : this->groups) {
        if (group->name.find(new_name_prefix) == 0) {
            std::string int_postfix = group->name.substr(new_name_prefix.length());
            try {
                int last_id = std::stoi(int_postfix);
                new_name_id = std::max(new_name_id, last_id);
            } catch (...) {
            }
        }
    }
    return std::string(new_name_prefix + std::to_string(new_name_id + 1));
}


const std::string megamol::gui::configurator::Graph::generate_unique_module_name(const std::string& module_name) {

    int new_name_id = 0;
    std::string new_name_prefix = module_name + "_";
    for (auto& mod : this->modules) {
        if (mod->name.find(new_name_prefix) == 0) {
            std::string int_postfix = mod->name.substr(new_name_prefix.length());
            try {
                int last_id = std::stoi(int_postfix);
                new_name_id = std::max(new_name_id, last_id);
            } catch (...) {
            }
        }
    }
    return std::string(new_name_prefix + std::to_string(new_name_id + 1));
}


// GRAPH PRESENTATION ####################################################

megamol::gui::configurator::Graph::Presentation::Presentation(void)
    : params_visible(true)
    , params_readonly(false)
    , param_expert_mode(false)
    , utils()
    , update(true)
    , show_grid(false)
    , show_call_names(true)
    , show_slot_names(false)
    , show_module_names(true)
    , show_parameter_sidebar(false)
    , change_show_parameter_sidebar(false)
    , layout_current_graph(false)
    , child_split_width(300.0f)
    , reset_zooming(true)
    , param_name_space()
    , multiselect_start_pos()
    , multiselect_end_pos()
    , multiselect_done(false)
    , canvas_hovered(false)
    , current_font_scaling(1.0f)
    , graph_state() {

    this->graph_state.canvas.position = ImVec2(0.0f, 0.0f);
    this->graph_state.canvas.size = ImVec2(1.0f, 1.0f);
    this->graph_state.canvas.scrolling = ImVec2(0.0f, 0.0f);
    this->graph_state.canvas.zooming = 1.0f;
    this->graph_state.canvas.offset = ImVec2(0.0f, 0.0f);

    this->graph_state.interact.button_active_uid = GUI_INVALID_ID;         
    this->graph_state.interact.button_hovered_uid = GUI_INVALID_ID;  
          
    this->graph_state.interact.group_selected_uid = GUI_INVALID_ID;        
    this->graph_state.interact.group_hovered_uid = GUI_INVALID_ID;         
           
    this->graph_state.interact.modules_selected_uids.clear();
    this->graph_state.interact.module_hovered_uid = GUI_INVALID_ID;        
    this->graph_state.interact.module_mainview_uid = GUI_INVALID_ID; 
    this->graph_state.interact.modules_add_group_uids.clear();
    this->graph_state.interact.modules_remove_group_uids.clear();
    
    this->graph_state.interact.call_selected_uid = GUI_INVALID_ID;         
    this->graph_state.interact.call_hovered_uid = GUI_INVALID_ID;          
    
    this->graph_state.interact.slot_dropped_uid = GUI_INVALID_ID;    
              
    this->graph_state.interact.callslot_selected_uid = GUI_INVALID_ID;     
    this->graph_state.interact.callslot_hovered_uid = GUI_INVALID_ID;      
    this->graph_state.interact.callslot_add_group_uid = UIDPairType(GUI_INVALID_ID, GUI_INVALID_ID);
    this->graph_state.interact.callslot_compat_ptr.reset(); 
    
    this->graph_state.interact.interfaceslot_selected_uid = GUI_INVALID_ID;
    this->graph_state.interact.interfaceslot_hovered_uid = GUI_INVALID_ID; 
    this->graph_state.interact.interfaceslot_compat_ptr.reset();
        
    this->graph_state.groups.clear();      
    // this->graph_state.hotkeys are already initialzed
}


megamol::gui::configurator::Graph::Presentation::~Presentation(void) {}


void megamol::gui::configurator::Graph::Presentation::Present(
    megamol::gui::configurator::Graph& inout_graph, GraphStateType& state) {

    try {
        if (ImGui::GetCurrentContext() == nullptr) {
            vislib::sys::Log::DefaultLog.WriteError(
                "No ImGui context available. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return;
        }
        ImGuiIO& io = ImGui::GetIO();

        ImGuiID graph_uid = inout_graph.uid;
        ImGui::PushID(graph_uid);

        // State Init/Reset ----------------------
        this->show_parameter_sidebar = state.show_parameter_sidebar;
        if (this->change_show_parameter_sidebar) {
            this->show_parameter_sidebar = !this->show_parameter_sidebar;
            state.show_parameter_sidebar = this->show_parameter_sidebar;
            this->change_show_parameter_sidebar = false;
        }
        this->graph_state.hotkeys = state.hotkeys;
        this->graph_state.groups.clear();
        for (auto& group : inout_graph.GetGroups()) {
            std::pair<ImGuiID, std::string> group_pair(group->uid, group->name);
            this->graph_state.groups.emplace_back(group_pair);
        }
        this->graph_state.interact.slot_dropped_uid = GUI_INVALID_ID;        
        // Compatible call slot ptr
        this->graph_state.interact.callslot_compat_ptr.reset();
        //  Prioritise hovered slots but only if there is no drag and drop
        bool found_compatible_callslot = false;        
        bool callslot_hovered = (this->graph_state.interact.callslot_hovered_uid != GUI_INVALID_ID);
        bool interfaceslot_hovered = (this->graph_state.interact.interfaceslot_hovered_uid != GUI_INVALID_ID);
        if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
            if (payload->IsDataType(GUI_DND_CALLSLOT_UID_TYPE)) {
                callslot_hovered = false;
                interfaceslot_hovered = false;
            }
        }
        ImGuiID slot_uid = (callslot_hovered) ? (this->graph_state.interact.callslot_hovered_uid) : 
            ((interfaceslot_hovered) ? (GUI_INVALID_ID) : (this->graph_state.interact.callslot_selected_uid));
        if (slot_uid != GUI_INVALID_ID) {
            for (auto& mods : inout_graph.GetModules()) {
                CallSlotPtrType callslot_ptr;
                if (mods->GetCallSlot(slot_uid, callslot_ptr)) {
                    this->graph_state.interact.callslot_compat_ptr = callslot_ptr;
                    found_compatible_callslot = true;
                }
            }
        }
        // Compatible call slot and/or interface slot ptr
        this->graph_state.interact.interfaceslot_compat_ptr.reset();        
        slot_uid = (interfaceslot_hovered) ? (this->graph_state.interact.interfaceslot_hovered_uid) : (this->graph_state.interact.interfaceslot_selected_uid);
        if (!found_compatible_callslot && (slot_uid != GUI_INVALID_ID)) {
            for (auto& group_ptr : inout_graph.GetGroups()) {
                InterfaceSlotPtrType interfaceslot_ptr;
                if (group_ptr->GetInterfaceSlot(slot_uid, interfaceslot_ptr)) {
                    this->graph_state.interact.interfaceslot_compat_ptr = interfaceslot_ptr;                        
                    CallSlotPtrType callslot_ptr;
                    if (interfaceslot_ptr->GetCompatibleCallSlot(callslot_ptr)) {
                        this->graph_state.interact.callslot_compat_ptr = callslot_ptr;
                    }
                }
            }
        }

        // Tab showing this graph ---------------
        bool popup_rename = false;
        ImGuiTabItemFlags tab_flags = ImGuiTabItemFlags_None;
        if (inout_graph.IsDirty()) {
            tab_flags |= ImGuiTabItemFlags_UnsavedDocument;
        }
        std::string graph_label = "    " + inout_graph.name + "  ###graph" + std::to_string(graph_uid);
        bool open = true;
        if (ImGui::BeginTabItem(graph_label.c_str(), &open, tab_flags)) {
            // Context menu
            if (ImGui::BeginPopupContextItem()) {
                ImGui::TextUnformatted("Project");
                ImGui::Separator();

                if (ImGui::MenuItem("Rename")) {
                    popup_rename = true;
                }

                if (!inout_graph.GetFilename().empty()) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Filename");
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 13.0f);
                    ImGui::TextUnformatted(inout_graph.GetFilename().c_str());
                    ImGui::PopTextWrapPos();
                }

                ImGui::EndPopup();
            }

            // Draw -----------------------------
            this->present_menu(inout_graph);

            float child_width_auto = 0.0f;
            if (this->show_parameter_sidebar) {
                this->utils.VerticalSplitter(
                    GUIUtils::FixedSplitterSide::RIGHT, child_width_auto, this->child_split_width);
            }

            // Load font for canvas
            ImFont* font_ptr = nullptr;
            unsigned int scalings_count = static_cast<unsigned int>(state.font_scalings.size());
            if (scalings_count == 0) {
                throw std::invalid_argument("Array for graph fonts is empty.");
            } else if (scalings_count == 1) {
                font_ptr = io.Fonts->Fonts[0];
                this->current_font_scaling = state.font_scalings[0];
            } else {
                for (unsigned int i = 0; i < scalings_count; i++) {
                    bool apply = false;
                    if (i == 0) {
                        if (this->graph_state.canvas.zooming <= state.font_scalings[i]) {
                            apply = true;
                        }
                    } else if (i == (scalings_count - 1)) {
                        if (this->graph_state.canvas.zooming >= state.font_scalings[i]) {
                            apply = true;
                        }
                    } else {
                        if ((state.font_scalings[i - 1] < this->graph_state.canvas.zooming) &&
                            (this->graph_state.canvas.zooming < state.font_scalings[i + 1])) {
                            apply = true;
                        }
                    }
                    if (apply) {
                        font_ptr = io.Fonts->Fonts[i];
                        this->current_font_scaling = state.font_scalings[i];
                        break;
                    }
                }
            }
            if (font_ptr != nullptr) {
                ImGui::PushFont(font_ptr);
                this->present_canvas(inout_graph, child_width_auto);
                ImGui::PopFont();
            } else {
                throw std::invalid_argument("Pointer to font is nullptr.");
            }

            if (this->show_parameter_sidebar) {
                ImGui::SameLine();
                this->present_parameters(inout_graph, this->child_split_width);
            }

            // Apply graph layout
            if (this->layout_current_graph) {
                this->layout_graph(inout_graph);
                this->layout_current_graph = false;
                this->update = true;
            }

            state.graph_selected_uid = inout_graph.uid;
            ImGui::EndTabItem();
        }

        // State processing ---------------------
        // Add module to group
        if (!this->graph_state.interact.modules_add_group_uids.empty()) {
            ModulePtrType module_ptr;
            ImGuiID new_group_uid = GUI_INVALID_ID;
            for (auto& uid_pair : this->graph_state.interact.modules_add_group_uids) {
                module_ptr.reset();
                for (auto& mod : inout_graph.GetModules()) {
                    if (mod->uid == uid_pair.first) {
                        module_ptr = mod;
                    }
                }
                if (module_ptr != nullptr) {

                    // Add module to new or already existing group
                    // Create new group for multiple selected modules only once!
                    ImGuiID group_uid = GUI_INVALID_ID;
                    if ((uid_pair.second == GUI_INVALID_ID) && (new_group_uid == GUI_INVALID_ID)) {
                        new_group_uid = inout_graph.AddGroup();
                    }
                    if (uid_pair.second == GUI_INVALID_ID) {
                        group_uid = new_group_uid;
                    } else {
                        group_uid = uid_pair.second;
                    }

                    GroupPtrType add_group_ptr;
                    if (inout_graph.get_group(group_uid, add_group_ptr)) {
                        // Remove module from previous associated group
                        ImGuiID module_group_uid = module_ptr->GUI_GetGroupUID();
                        GroupPtrType remove_group_ptr;
                        if (inout_graph.get_group(module_group_uid, remove_group_ptr)) {
                            if (remove_group_ptr->uid != add_group_ptr->uid) {
                                remove_group_ptr->RemoveModule(module_ptr->uid);
                                inout_graph.restore_callslots_interfaceslot_state(remove_group_ptr->uid);                                 
                            }
                        }
                        // Add module to group
                        add_group_ptr->AddModule(module_ptr);
                        inout_graph.restore_callslots_interfaceslot_state(add_group_ptr->uid);
                    }
                }
            }
            this->graph_state.interact.modules_add_group_uids.clear();
        }
        // Remove module from group
        if (!this->graph_state.interact.modules_remove_group_uids.empty()) {
            for (auto& module_uid : this->graph_state.interact.modules_remove_group_uids) {
                for (auto& remove_group_ptr : inout_graph.GetGroups()) {
                    if (remove_group_ptr->ContainsModule(module_uid)) {
                        remove_group_ptr->RemoveModule(module_uid);
                    }
                }
            }
            this->graph_state.interact.modules_remove_group_uids.clear();
        }
        // Create new interface slot for call slot
        ImGuiID callslot_uid = this->graph_state.interact.callslot_add_group_uid.first;
        if (callslot_uid != GUI_INVALID_ID) {
            CallSlotPtrType callslot_ptr = nullptr;
            for (auto& mod : inout_graph.GetModules()) {
                for (auto& callslot_map : mod->GetCallSlots()) {
                    for (auto& callslot : callslot_map.second) {
                        if (callslot->uid == callslot_uid) {
                            callslot_ptr = callslot;
                        }
                    }
                }
            }
            if (callslot_ptr != nullptr) {
                ImGuiID module_uid = this->graph_state.interact.callslot_add_group_uid.second;
                if (module_uid != GUI_INVALID_ID) {
                    for (auto& group : inout_graph.GetGroups()) {
                        if (group->ContainsModule(module_uid)) {
                            group->InterfaceSlot_AddCallSlot(callslot_ptr, inout_graph.generate_unique_id());
                        }
                    }
                }
            }
            this->graph_state.interact.callslot_add_group_uid.first = GUI_INVALID_ID;
            this->graph_state.interact.callslot_add_group_uid.second = GUI_INVALID_ID;
        }
        // Process module/call/group deletion
        if (std::get<1>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::DELETE_GRAPH_ITEM])) {
            // Reset pointer before deleting
            this->graph_state.interact.callslot_compat_ptr = nullptr;
            this->graph_state.interact.interfaceslot_compat_ptr = nullptr; 
            
            if (!this->graph_state.interact.modules_selected_uids.empty()) {
                for (auto& module_uid : this->graph_state.interact.modules_selected_uids) {
                    inout_graph.DeleteModule(module_uid);
                }
            }
            if (this->graph_state.interact.call_selected_uid != GUI_INVALID_ID) {                                
                inout_graph.DeleteCall(this->graph_state.interact.call_selected_uid);
            }
            if (this->graph_state.interact.group_selected_uid != GUI_INVALID_ID) {
                inout_graph.DeleteGroup(this->graph_state.interact.group_selected_uid);
            }
            if (this->graph_state.interact.interfaceslot_selected_uid != GUI_INVALID_ID) {   
                for (auto& group_ptr : inout_graph.groups) {
                    InterfaceSlotPtrType interfaceslot_ptr;
                    if (group_ptr->GetInterfaceSlot(this->graph_state.interact.interfaceslot_selected_uid, interfaceslot_ptr)) {
                        // Allow deletion only if interface slot has no call slots with connceted calls.
                        if (!interfaceslot_ptr->IsConnected()) {
                            interfaceslot_ptr.reset();
                            group_ptr->DeleteInterfaceSlot(this->graph_state.interact.interfaceslot_selected_uid);
                        }
                    }
                }                  
            }                        
            // Reset interact state for modules and call slots
            this->graph_state.interact.group_selected_uid = GUI_INVALID_ID;
            this->graph_state.interact.group_hovered_uid = GUI_INVALID_ID;
            this->graph_state.interact.interfaceslot_selected_uid = GUI_INVALID_ID;
            this->graph_state.interact.interfaceslot_hovered_uid = GUI_INVALID_ID;
            this->graph_state.interact.modules_selected_uids.clear();
            this->graph_state.interact.module_hovered_uid = GUI_INVALID_ID;
            this->graph_state.interact.module_mainview_uid = GUI_INVALID_ID;
            this->graph_state.interact.modules_add_group_uids.clear();
            this->graph_state.interact.modules_remove_group_uids.clear();
            this->graph_state.interact.call_selected_uid = GUI_INVALID_ID;
            this->graph_state.interact.call_hovered_uid = GUI_INVALID_ID;    
            this->graph_state.interact.callslot_selected_uid = GUI_INVALID_ID;
            this->graph_state.interact.callslot_hovered_uid = GUI_INVALID_ID;
            this->graph_state.interact.callslot_add_group_uid = UIDPairType(GUI_INVALID_ID, GUI_INVALID_ID);
            this->graph_state.interact.slot_dropped_uid = GUI_INVALID_ID;    
        }
        // Delete empty group(s)
        std::vector<ImGuiID> delete_empty_groups_uids;
        for (auto& group_ptr : inout_graph.GetGroups()) {
            if (group_ptr->GetModules().empty()) {
                delete_empty_groups_uids.emplace_back(group_ptr->uid);
            }
        }
        for (auto& group_uid : delete_empty_groups_uids) {
            inout_graph.DeleteGroup(group_uid);
        }
        // Set delete flag if tab was closed
        if (!open) {
            state.graph_delete = true;
        }
        // Propoagate unhandeled hotkeys back to configurator state
        state.hotkeys = this->graph_state.hotkeys;

        // Rename pop-up
        this->utils.RenamePopUp("Rename Project", popup_rename, inout_graph.name);

        ImGui::PopID();

    } catch (std::exception e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Error: %s [%s, %s, line %d]\n", e.what(), __FILE__, __FUNCTION__, __LINE__);
        return;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError("Unknown Error. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return;
    }
}



bool megamol::gui::configurator::Graph::Presentation::StateFromJsonString(Graph& inout_graph, const std::string& in_json_string) {

    try {
        if (in_json_string.empty()) {
            return false;
        }
                
        bool found = false;
        bool valid = true;

        nlohmann::json json;
        json = nlohmann::json::parse(in_json_string);

        if (!json.is_object()) {
            ///vislib::sys::Log::DefaultLog.WriteError("State is no valid JSON object. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }

        for (auto& header_item : json.items()) {
            if (header_item.key() == GUI_JSON_TAG_GRAPHS) {
                for (auto& content_item : header_item.value().items()) {
                    std::string json_graph_id = content_item.key();
                    if (json_graph_id == inout_graph.GetFilename()) { /// = graph filename
                        found = true;
                        auto config_state = content_item.value();      

                        // show_parameter_sidebar
                        bool tmp_show_parameter_sidebar;
                        this->change_show_parameter_sidebar = false;
                        if (config_state.at("show_parameter_sidebar").is_boolean()) {
                            config_state.at("show_parameter_sidebar").get_to(tmp_show_parameter_sidebar);
                            if (this->show_parameter_sidebar != tmp_show_parameter_sidebar) {
                                this->change_show_parameter_sidebar = true;
                            }
                            
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'show_parameter_sidebar' as boolean. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }
                        // show_grid
                        if (config_state.at("show_grid").is_boolean()) {
                            config_state.at("show_grid").get_to(this->show_grid);                          
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'show_grid' as boolean. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }  
                                                
                        // show_call_names
                        if (config_state.at("show_call_names").is_boolean()) {
                            config_state.at("show_call_names").get_to(this->show_call_names);
                            for (auto& call : inout_graph.get_calls()) {
                                call->GUI_SetLabelVisibility(this->show_call_names);
                            }                            
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'show_call_names' as boolean. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }     
                        // show_slot_names
                        if (config_state.at("show_slot_names").is_boolean()) {
                            config_state.at("show_slot_names").get_to(this->show_slot_names);
                            for (auto& mod : inout_graph.GetModules()) {
                                for (auto& callslot_types : mod->GetCallSlots()) {
                                    for (auto& callslots : callslot_types.second) {
                                        callslots->GUI_SetLabelVisibility(this->show_slot_names);
                                    }
                                }
                            }
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'show_slot_names' as boolean. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }
                        // show_module_names
                        if (config_state.at("show_module_names").is_boolean()) {
                            config_state.at("show_module_names").get_to(this->show_module_names);
                            for (auto& mod : inout_graph.GetModules()) {
                                mod->GUI_SetLabelVisibility(this->show_module_names);
                            }
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'show_module_names' as boolean. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }   
                        // params_visible
                        if (config_state.at("params_visible").is_boolean()) {
                            config_state.at("params_visible").get_to(this->params_visible);
                            /// Do not apply. Already refelcted in parameter gui state.
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'params_visible' as boolean. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }   
                        // params_readonly
                        if (config_state.at("params_readonly").is_boolean()) {
                            config_state.at("params_readonly").get_to(this->params_readonly);
                            /// Do not apply. Already refelcted in parameter gui state.
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'params_readonly' as boolean. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }       
                        // param_expert_mode
                        if (config_state.at("param_expert_mode").is_boolean()) {
                            config_state.at("param_expert_mode").get_to(this->param_expert_mode);
                            for (auto& module_ptr : inout_graph.GetModules()) {
                                for (auto& parameter : module_ptr->parameters) {
                                    parameter.GUI_SetExpert(this->param_expert_mode);
                                }
                            }
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'param_expert_mode' as boolean. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }
                        // canvas_scrolling
                        if (config_state.at("canvas_scrolling").is_array() && (config_state.at("canvas_scrolling").size() == 2)) {
                            if (config_state.at("canvas_scrolling")[0].is_number_float()) {
                                config_state.at("canvas_scrolling")[0].get_to(this->graph_state.canvas.scrolling.x);
                            } else {
                                vislib::sys::Log::DefaultLog.WriteError(
                                    "JSON state: Failed to read first value of 'canvas_scrolling' as float. [%s, %s, line %d]\n",
                                    __FILE__, __FUNCTION__, __LINE__);
                            }
                            if (config_state.at("canvas_scrolling")[1].is_number_float()) {
                                config_state.at("canvas_scrolling")[1].get_to(this->graph_state.canvas.scrolling.y);
                            } else {
                                vislib::sys::Log::DefaultLog.WriteError(
                                    "JSON state: Failed to read second value of 'canvas_scrolling' as float. [%s, %s, line %d]\n",
                                    __FILE__, __FUNCTION__, __LINE__);
                            }
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read 'canvas_scrolling' as array of size two. [%s, %s, line %d]\n", __FILE__,
                                __FUNCTION__, __LINE__);
                        }
                        // canvas_zooming
                        if (config_state.at("canvas_zooming").is_number_float()) {
                            config_state.at("canvas_zooming").get_to(this->graph_state.canvas.zooming);
                            this->reset_zooming = false;
                        } else {
                            vislib::sys::Log::DefaultLog.WriteError(
                                "JSON state: Failed to read first value of 'canvas_zooming' as float. [%s, %s, line %d]\n",
                                __FILE__, __FUNCTION__, __LINE__);
                        }                    
                        
                        // modules
                        for (auto& module_item : content_item.value().items()) {
                            if (module_item.key() == "modules") {
                                for (auto& module_state : module_item.value().items()) {
                                    std::string module_fullname = module_state.key();
                                    auto position_item = module_state.value();
                                    valid = true;
                                                                
                                    // graph_position
                                    ImVec2 module_position;
                                    if (position_item.at("graph_position").is_array() && (position_item.at("graph_position").size() == 2)) {
                                        if (position_item.at("graph_position")[0].is_number_float()) {
                                            position_item.at("graph_position")[0].get_to(module_position.x);
                                        } else {
                                            vislib::sys::Log::DefaultLog.WriteError(
                                                "JSON state: Failed to read first value of 'graph_position' as float. [%s, %s, line %d]\n",
                                                __FILE__, __FUNCTION__, __LINE__);
                                            valid = false;
                                        }
                                        if (position_item.at("graph_position")[1].is_number_float()) {
                                            position_item.at("graph_position")[1].get_to(module_position.y);
                                        } else {
                                            vislib::sys::Log::DefaultLog.WriteError(
                                                "JSON state: Failed to read second value of 'graph_position' as float. [%s, %s, line %d]\n",
                                                __FILE__, __FUNCTION__, __LINE__);
                                            valid = false;
                                        }
                                    } else {
                                        vislib::sys::Log::DefaultLog.WriteError(
                                            "JSON state: Failed to read 'graph_position' as array of size two. [%s, %s, line %d]\n", __FILE__,
                                            __FUNCTION__, __LINE__);
                                        valid = false;
                                    }
                                    
                                    // Apply graph position to module
                                    if (valid) {
                                        bool module_found = false;
                                        for (auto& module_ptr : inout_graph.modules) {
                                            if (module_ptr->FullName() == module_fullname) {
                                                module_ptr->GUI_SetPosition(module_position);
                                                module_found = true;
                                            }
                                        }
                                        if (!module_found) {
                                            vislib::sys::Log::DefaultLog.WriteError(
                                                "JSON state: Unable to find module '%s' to apply graph position in configurator. [%s, %s, line %d]\n", module_fullname.c_str(), __FILE__,
                                                __FUNCTION__, __LINE__);
                                        }   
                                    }                         
                                }
                            }
                        }
                    
                        // interfaces
                        /*
                        for (auto& interfaces_item : content_item.value().items()) {
                            if (interfaces_item.key() == "interfaces") {
                                for (auto& interface_state : interfaces_item.value().items()) {
                                    std::string group_name = interface_state.key();
                                    auto interfaceslot_items = interface_state.value();
                                                
                                    // interfaces
                                    for (auto& interfaceslot_item : interfaceslot_items.items()) {
                                        valid = true;
                                        
                                        std::vector<std::string> calleslot_fullnames;
                                        for (auto& callslot_item : interfaceslot_item.value().items()) {
                                            if (callslot_item.value().is_string()) {
                                                calleslot_fullnames.emplace_back(callslot_item.value().get<std::string>());
                                            } else {
                                                vislib::sys::Log::DefaultLog.WriteError(
                                                    "JSON state: Failed to read value of call slot as string. [%s, %s, line %d]\n",
                                                    __FILE__, __FUNCTION__, __LINE__);
                                                valid = false;
                                            }
                                        }
                                    
                                        // Add interface slot containing found calls slots to group
                                        if (valid ) {

                                            // Find pointers to call slots by name
                                            CallSlotPtrVectorType callslot_ptr_vector;
                                            for (auto& callsslot_fullname : calleslot_fullnames) {
                                                auto split_pos = callsslot_fullname.rfind("::");
                                                if (split_pos != std::string::npos) {
                                                    std::string callslot_name = callsslot_fullname.substr(split_pos);
                                                    std::string module_fullname = callsslot_fullname.substr(0, (split_pos-2));
                                                    for (auto& module_ptr : inout_graph.modules) {
                                                        if (module_ptr->FullName() == module_fullname) {
                                                            for (auto& callslot_map : module_ptr->GetCallSlots()) {
                                                                for (auto& callslot_ptr : callslot_map.second) {
                                                                    if (callslot_ptr->name == callslot_name) {
                                                                        callslot_ptr_vector.emplace_back(callslot_ptr);
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                                    
                                                    
                                            bool group_found = false;
                                            for (auto& group_ptr : inout_graph.groups) {
                                                if (group_ptr->name == group_name) {
                                                    for (auto& callslot_ptr : callslot_ptr_vector) {
                                                        std::cout << ">>>>>> GROUP: " << group_name << " Interface - CallSlots: " <<  callslot_ptr->name << std::endl;
                                                    }
                                                    
                                                    
                                                    
                                                    
                                                    //ImGuiID new_interfaceslot_uid = inout_graph.generate_unique_id();
                                                    //group_ptr->InterfaceSlot_AddCallSlot( , new_interfaceslot_uid);


                                                    group_found = true;
                                                }
                                            }
                                            if (!group_found) {
                                                vislib::sys::Log::DefaultLog.WriteError(
                                                    "JSON state: Unable to find group '%s' to add interface slot. [%s, %s, line %d]\n", group_name.c_str(), __FILE__,
                                                    __FUNCTION__, __LINE__);
                                            }   
                                        }                                                                            
                                    }
                                }
                            }   
                        } 
                        */
                    }
                }
            }
        }

        if (found) {
            this->update = true;
            vislib::sys::Log::DefaultLog.WriteInfo("[Configurator] Read graph state for '%s' from JSON string.", inout_graph.name.c_str());  
        }
        else {
            /// vislib::sys::Log::DefaultLog.WriteWarn("Could not find graph state in JSON. [%s, %s, line
            /// %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }

    } catch (nlohmann::json::type_error& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::invalid_iterator& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::out_of_range& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::other_error& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unknown Error - Unable to parse JSON string. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


bool megamol::gui::configurator::Graph::Presentation::StateToJSON(Graph& inout_graph, nlohmann::json& out_json) {

    try {
        out_json.clear();
        std::string json_graph_id = inout_graph.GetFilename(); /// = graph filename
        
        // ! State of graph is only stored if project was saved to file previously. Otherwise the project could not be loaded again.
        if (!json_graph_id.empty()) {

            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["show_parameter_sidebar"] = this->show_parameter_sidebar;            
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["show_grid"] = this->show_grid;
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["show_call_names"] = this->show_call_names;
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["show_slot_names"] = this->show_slot_names;
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["show_module_names"] = this->show_module_names;
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["params_visible"] = this->params_visible;
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["params_readonly"] = this->params_readonly;
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["param_expert_mode"] = this->param_expert_mode;
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["canvas_scrolling"] = { this->graph_state.canvas.scrolling.x, this->graph_state.canvas.scrolling.y };
            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["canvas_zooming"] = this->graph_state.canvas.zooming;
            
            // Module positions
            for (auto& module_ptr : inout_graph.modules) {
                out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["modules"][module_ptr->FullName()]["graph_position"] = {module_ptr->GUI_GetPosition().x, module_ptr->GUI_GetPosition().y};  
            }
            // Group interface slots
            size_t interface_number = 0;
            for (auto& group_ptr : inout_graph.groups) {
                for (auto& interfaceslots_map : group_ptr->GetInterfaceSlots()) {
                    for (auto& interface_ptr : interfaceslots_map.second) {
                        std::string interface_label = "interface_slot_" + std::to_string(interface_number);
                        for (auto& callslot_ptr : interface_ptr->GetCallSlots()) {
                            std::string callslot_fullname;
                            if (callslot_ptr->IsParentModuleConnected()) {
                                callslot_fullname = callslot_ptr->GetParentModule()->FullName() + "::" + callslot_ptr->name;
                            }
                            
                            out_json[GUI_JSON_TAG_GRAPHS][json_graph_id]["interfaces"][group_ptr->name][interface_label] += callslot_fullname;
                        }
                        interface_number++;
                    }
                }
            } 
            
            ///vislib::sys::Log::DefaultLog.WriteInfo("[Configurator] Wrote graph state to JSON.");  
        }
        else {
            ///vislib::sys::Log::DefaultLog.WriteWarn("State of project '%s' is not being saved. Save project to file in order to get its state saved. [%s, %s, line %d]\n", inout_graph.name.c_str(), __FILE__, __FUNCTION__, __LINE__);
            return false;
        }

    } catch (nlohmann::json::type_error& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::invalid_iterator& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::out_of_range& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::other_error& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unknown Error - Unable to write JSON of state. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


void megamol::gui::configurator::Graph::Presentation::present_menu(megamol::gui::configurator::Graph& inout_graph) {

    const float child_height = ImGui::GetFrameHeightWithSpacing() * 1.0f;
    auto child_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NavFlattened;
    ImGui::BeginChild("graph_menu", ImVec2(0.0f, child_height), false, child_flags);

    // Main View Checkbox
    ModulePtrType selected_mod_ptr;
    if (inout_graph.GetModule(this->graph_state.interact.module_mainview_uid, selected_mod_ptr)) {
        this->graph_state.interact.module_mainview_uid = GUI_INVALID_ID;
    } else if (this->graph_state.interact.modules_selected_uids.size() == 1) {
        for (auto& mod : inout_graph.GetModules()) {
            if ((this->graph_state.interact.modules_selected_uids[0] == mod->uid) && (mod->is_view)) {
                selected_mod_ptr = mod;
            }
        }
    }
    if (selected_mod_ptr == nullptr) {
        GUIUtils::ReadOnlyWigetStyle(true);
        bool checked = false;
        ImGui::Checkbox("Main View", &checked);
        GUIUtils::ReadOnlyWigetStyle(false);
    } else {
        ImGui::Checkbox("Main View", &selected_mod_ptr->is_view_instance);
        // Set all other (view) modules to non main views
        if (selected_mod_ptr->is_view_instance) {
            for (auto& mod : inout_graph.GetModules()) {
                if (selected_mod_ptr->uid != mod->uid) {
                    mod->is_view_instance = false;
                }
            }
        }
    }
    ImGui::SameLine();

    ImGui::Text("Scrolling: %.2f,%.2f", this->graph_state.canvas.scrolling.x, this->graph_state.canvas.scrolling.y);
    ImGui::SameLine();
    if (ImGui::Button("Reset###reset_scrolling")) {
        this->graph_state.canvas.scrolling = ImVec2(0.0f, 0.0f);
        this->update = true;
    }
    ImGui::SameLine();

    ImGui::Text("Zooming: %.2f", this->graph_state.canvas.zooming);
    ImGui::SameLine();
    if (ImGui::Button("Reset###reset_zooming")) {
        this->reset_zooming = true;
    }
    ImGui::SameLine();

    ImGui::Checkbox("Grid", &this->show_grid);

    ImGui::SameLine();

    if (ImGui::Checkbox("Call Names", &this->show_call_names)) {
        for (auto& call : inout_graph.get_calls()) {
            call->GUI_SetLabelVisibility(this->show_call_names);
        }
        this->update = true;
    }
    ImGui::SameLine();

    if (ImGui::Checkbox("Module Names", &this->show_module_names)) {
        for (auto& mod : inout_graph.GetModules()) {
            mod->GUI_SetLabelVisibility(this->show_module_names);
        }
        this->update = true;
    }
    ImGui::SameLine();

    if (ImGui::Checkbox("Slot Names", &this->show_slot_names)) {
        for (auto& mod : inout_graph.GetModules()) {
            for (auto& callslot_types : mod->GetCallSlots()) {
                for (auto& callslots : callslot_types.second) {
                    callslots->GUI_SetLabelVisibility(this->show_slot_names);
                }
            }
        }
        this->update = true;
    }
    ImGui::SameLine();

    if (ImGui::Button("Layout Graph")) {
        this->layout_current_graph = true;
    }

    ImGui::EndChild();
}


void megamol::gui::configurator::Graph::Presentation::present_canvas(
    megamol::gui::configurator::Graph& inout_graph, float child_width) {

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // Colors
    const ImU32 COLOR_CANVAS_BACKGROUND = ImGui::ColorConvertFloat4ToU32(
        style.Colors[ImGuiCol_ChildBg]); // ImGuiCol_ScrollbarBg ImGuiCol_ScrollbarGrab ImGuiCol_Border

    ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_CANVAS_BACKGROUND);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    auto child_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove;
    ImGui::BeginChild("region", ImVec2(child_width, 0.0f), true, child_flags);

    this->canvas_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    // UPDATE CANVAS -----------------------------------------------------------

    // Update canvas position
    ImVec2 new_position = ImGui::GetWindowPos();
    if ((this->graph_state.canvas.position.x != new_position.x) ||
        (this->graph_state.canvas.position.y != new_position.y)) {
        this->update = true;
    }
    this->graph_state.canvas.position = new_position;
    // Update canvas size
    ImVec2 new_size = ImGui::GetWindowSize();
    if ((this->graph_state.canvas.size.x != new_size.x) || (this->graph_state.canvas.size.y != new_size.y)) {
        this->update = true;
    }
    this->graph_state.canvas.size = new_size;
    // Update canvas offset
    ImVec2 new_offset =
        this->graph_state.canvas.position + (this->graph_state.canvas.scrolling * this->graph_state.canvas.zooming);
    if ((this->graph_state.canvas.offset.x != new_offset.x) || (this->graph_state.canvas.offset.y != new_offset.y)) {
        this->update = true;
    }
    this->graph_state.canvas.offset = new_offset;

    // Update position and size of modules (and  call slots) and groups.
    if (this->update) {
        for (auto& mod : inout_graph.GetModules()) {
            mod->GUI_Update(this->graph_state.canvas);
        }
        for (auto& group : inout_graph.GetGroups()) {
            group->GUI_Update(this->graph_state.canvas);
        }
        this->update = false;
    }


    ImGui::PushClipRect(
        this->graph_state.canvas.position, this->graph_state.canvas.position + this->graph_state.canvas.size, true);

    // GRID --------------------------------------
    if (this->show_grid) {
        this->present_canvas_grid();
    }
    ImGui::PopStyleVar(2);

    /// Phase 1: Interaction ---------------------------------------------------
    // Update button states of all graph elements
    /// Phase 2: Rendering -----------------------------------------------------
    // Render graph elements using collected button state
    this->graph_state.interact.button_active_uid = GUI_INVALID_ID;
    this->graph_state.interact.button_hovered_uid = GUI_INVALID_ID;
    for (size_t p = 0; p < 2; p++) {
        PresentPhase phase = static_cast<PresentPhase>(p);

        // 1] GROUPS and INTERFACE SLOTS --------------
        for (auto& group_ptr : inout_graph.GetGroups()) {
            
            group_ptr->GUI_Present(phase, this->graph_state);
            
            // 2] MODULES and CALL SLOTS ----------------
            for (auto& module_ptr : inout_graph.GetModules()) {
                if (module_ptr->GUI_GetGroupUID() == group_ptr->uid){
                    
                    module_ptr->GUI_Present(phase, this->graph_state);
                    
                    // 3] CALLS ---------------------------------;
                    /// Check only for calls of caller slots for considering each call only once
                    for (auto& callslots_ptr : module_ptr->GetCallSlots(CallSlotType::CALLER)) {
                        for (auto& call_ptr : callslots_ptr->GetConnectedCalls()) {
                        
                            bool caller_group = false;
                            auto caller_ptr = call_ptr->GetCallSlot(CallSlotType::CALLER);
                            if (caller_ptr->IsParentModuleConnected()) {
                                if (caller_ptr->GetParentModule()->GUI_GetGroupUID() == group_ptr->uid) {
                                    caller_group = true;
                                }
                            }
                            bool callee_group = false;
                            auto callee_ptr = call_ptr->GetCallSlot(CallSlotType::CALLER);
                            if (callee_ptr->IsParentModuleConnected()) {
                                if (callee_ptr->GetParentModule()->GUI_GetGroupUID() == group_ptr->uid) {
                                    callee_group = true;
                                }
                            }                    
                            if (caller_group || callee_group) {
                                
                                call_ptr->GUI_Present(phase, this->graph_state);
                            }
                        }
                    }                
                }
            }
        }
        // MODULES (no group member)
        for (auto& module_ptr : inout_graph.GetModules()) {
            if (!module_ptr->GUI_IsGroupMember()) {
                module_ptr->GUI_Present(phase, this->graph_state);
            }
        }
        // CALLS (call slots are not part of module which is group member)
        for (auto& call_ptr : inout_graph.get_calls()) {
            bool caller_group = false;
            auto caller_ptr = call_ptr->GetCallSlot(CallSlotType::CALLER);
            if (caller_ptr->IsParentModuleConnected()) {
                if (caller_ptr->GetParentModule()->GUI_IsGroupMember()) {
                    caller_group = true;
                }
            }
            bool callee_group = false;
            auto callee_ptr = call_ptr->GetCallSlot(CallSlotType::CALLER);
            if (callee_ptr->IsParentModuleConnected()) {
                if (callee_ptr->GetParentModule()->GUI_IsGroupMember()) {
                    callee_group = true;
                }
            } 
            if ((!caller_group) && (!callee_group)) {
                call_ptr->GUI_Present(phase, this->graph_state);
            }
        }  
    }
    
    // Multiselection ----------------------------
    this->present_canvas_multiselection(inout_graph);

    // Dragged CALL ------------------------------
    this->present_canvas_dragged_call(inout_graph);

    ImGui::PopClipRect();

    // Zooming and Scaling ----------------------
    // Must be checked inside this canvas child window!
    // Check at the end of drawing for being applied in next frame when font scaling matches zooming.
    if ((ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()) || this->reset_zooming) {

        // Scrolling (2 = Middle Mouse Button)
        if (ImGui::IsMouseDragging(2, 0.0f)) {
            this->graph_state.canvas.scrolling =
                this->graph_state.canvas.scrolling + ImGui::GetIO().MouseDelta / this->graph_state.canvas.zooming;
            this->update = true;
        }

        // Zooming (Mouse Wheel) + Reset
        if ((io.MouseWheel != 0) || this->reset_zooming) {
            float last_zooming = this->graph_state.canvas.zooming;
            ImVec2 current_mouse_pos;
            if (this->reset_zooming) {
                this->graph_state.canvas.zooming = 1.0f;
                current_mouse_pos = this->graph_state.canvas.offset -
                                    (this->graph_state.canvas.position + this->graph_state.canvas.size * 0.5f);
                this->reset_zooming = false;
            } else {
                const float factor = this->graph_state.canvas.zooming / 10.0f;
                this->graph_state.canvas.zooming = this->graph_state.canvas.zooming + (io.MouseWheel * factor);
                current_mouse_pos = this->graph_state.canvas.offset - ImGui::GetMousePos();
            }
            // Limit zooming
            this->graph_state.canvas.zooming =
                (this->graph_state.canvas.zooming <= 0.0f) ? 0.000001f : (this->graph_state.canvas.zooming);
            // Compensate zooming shift of origin
            ImVec2 scrolling_diff = (this->graph_state.canvas.scrolling * last_zooming) -
                                    (this->graph_state.canvas.scrolling * this->graph_state.canvas.zooming);
            this->graph_state.canvas.scrolling += (scrolling_diff / this->graph_state.canvas.zooming);
            // Move origin away from mouse position
            ImVec2 new_mouse_position = (current_mouse_pos / last_zooming) * this->graph_state.canvas.zooming;
            this->graph_state.canvas.scrolling +=
                ((new_mouse_position - current_mouse_pos) / this->graph_state.canvas.zooming);

            this->update = true;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // FONT scaling
    float font_scaling = this->graph_state.canvas.zooming / this->current_font_scaling;
    // Update when scaling of font has changed due to project tab switching
    if (ImGui::GetFont()->Scale != font_scaling) {
        this->update = true;
    }
    // Font scaling is applied next frame after ImGui::Begin()
    // Font for graph should not be the currently used font of the gui.
    ImGui::GetFont()->Scale = font_scaling;
}


void megamol::gui::configurator::Graph::Presentation::present_parameters(
    megamol::gui::configurator::Graph& inout_graph, float child_width) {

    ImGui::BeginGroup();

    float search_child_height = ImGui::GetFrameHeightWithSpacing() * 3.5f;
    auto child_flags =
        ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NavFlattened;
    ImGui::BeginChild("parameter_search_child", ImVec2(child_width, search_child_height), false, child_flags);

    ImGui::TextUnformatted("Parameters");
    ImGui::Separator();

    if (std::get<1>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::PARAMETER_SEARCH])) {
        this->utils.SetSearchFocus(true);
    }
    std::string help_text =
        "[" + std::get<0>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::PARAMETER_SEARCH]).ToString() +
        "] Set keyboard focus to search input field.\n"
        "Case insensitive substring search in parameter names.";
    this->utils.StringSearch("graph_parameter_search", help_text);
    auto search_string = this->utils.GetSearchString();

    // Mode
    ImGui::BeginGroup();
    this->utils.PointCircleButton("Mode");
    if (ImGui::BeginPopupContextItem("graph_param_mode_button_context", 0)) { // 0 = left mouse button
        bool changed = false;
        if (ImGui::MenuItem("Basic###graph_basic_mode", nullptr, !this->param_expert_mode, true)) {
            this->param_expert_mode = false;
            changed = true;
        }
        if (ImGui::MenuItem("Expert###graph_expert_mode", nullptr, this->param_expert_mode, true)) {
            this->param_expert_mode = true;
            changed = true;
        }
        if (changed) {
            for (auto& module_ptr : inout_graph.GetModules()) {
                for (auto& parameter : module_ptr->parameters) {
                    parameter.GUI_SetExpert(this->param_expert_mode);
                }
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndGroup();

    if (this->param_expert_mode) {
        ImGui::SameLine();

        // Visibility
        if (ImGui::Checkbox("Visibility", &this->params_visible)) {
            for (auto& module_ptr : inout_graph.GetModules()) {
                for (auto& parameter : module_ptr->parameters) {
                    parameter.GUI_SetVisibility(this->params_visible);
                }
            }
        }
        ImGui::SameLine();

        // Read-only option
        if (ImGui::Checkbox("Read-Only", &this->params_readonly)) {
            for (auto& module_ptr : inout_graph.GetModules()) {
                for (auto& parameter : module_ptr->parameters) {
                    parameter.GUI_SetReadOnly(this->params_readonly);
                }
            }
        }
    }
    ImGui::Separator();

    ImGui::EndChild();

    child_flags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened |
                  ImGuiWindowFlags_AlwaysUseWindowPadding;
    ImGui::BeginChild("parameter_list_frame_child", ImVec2(child_width, 0.0f), false, child_flags);

    if (!this->graph_state.interact.modules_selected_uids.empty()) {
        // Loop over all selected modules
        for (auto& module_uid : this->graph_state.interact.modules_selected_uids) {
            ModulePtrType module_ptr;
            // Get pointer to currently selected module(s)
            if (inout_graph.GetModule(module_uid, module_ptr)) {
                if (module_ptr->parameters.size() > 0) {

                    ImGui::PushID(module_ptr->uid);

                    // Set default state of header
                    auto headerId = ImGui::GetID(module_ptr->name.c_str());
                    auto headerState = ImGui::GetStateStorage()->GetInt(headerId, 1); // 0=close 1=open
                    ImGui::GetStateStorage()->SetInt(headerId, headerState);

                    if (ImGui::CollapsingHeader(module_ptr->name.c_str(), nullptr, ImGuiTreeNodeFlags_None)) {
                        this->utils.HoverToolTip(
                            module_ptr->description, ImGui::GetID(module_ptr->name.c_str()), 0.75f, 5.0f);

                        bool param_name_space_open = true;
                        unsigned int param_indent_stack = 0;
                        for (auto& parameter : module_ptr->parameters) {
                            // Filter module by given search string
                            bool search_filter = true;
                            if (!search_string.empty()) {
                                search_filter =
                                    this->utils.FindCaseInsensitiveSubstring(parameter.full_name, search_string);
                            }

                            // Add Collapsing header depending on parameter namespace
                            std::string current_param_namespace = parameter.GetNameSpace();
                            if (current_param_namespace != this->param_name_space) {
                                this->param_name_space = current_param_namespace;
                                while (param_indent_stack > 0) {
                                    param_indent_stack--;
                                    ImGui::Unindent();
                                }
                                /// ImGui::Separator();
                                if (!this->param_name_space.empty()) {
                                    ImGui::Indent();
                                    std::string label = this->param_name_space + "###" + parameter.full_name;
                                    // Open all namespace headers when parameter search is active
                                    if (!search_string.empty()) {
                                        auto headerId = ImGui::GetID(label.c_str());
                                        ImGui::GetStateStorage()->SetInt(headerId, 1);
                                    }
                                    param_name_space_open =
                                        ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                                    param_indent_stack++;
                                } else {
                                    param_name_space_open = true;
                                }
                            }

                            // Draw parameter
                            if (search_filter && param_name_space_open) {
                                parameter.GUI_Present();
                            }
                        }
                        // Vertical spacing
                        ImGui::Dummy(ImVec2(1.0f, ImGui::GetFrameHeightWithSpacing()));
                    }
                    ImGui::PopID();
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}


void megamol::gui::configurator::Graph::Presentation::present_canvas_grid(void) {

    ImGuiStyle& style = ImGui::GetStyle();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    assert(draw_list != nullptr);

    // Color
    const ImU32 COLOR_GRID = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border]);

    const float GRID_SIZE = 64.0f * this->graph_state.canvas.zooming;
    ImVec2 relative_offset = this->graph_state.canvas.offset - this->graph_state.canvas.position;

    for (float x = fmodf(relative_offset.x, GRID_SIZE); x < this->graph_state.canvas.size.x; x += GRID_SIZE) {
        draw_list->AddLine(ImVec2(x, 0.0f) + this->graph_state.canvas.position,
            ImVec2(x, this->graph_state.canvas.size.y) + this->graph_state.canvas.position, COLOR_GRID);
    }

    for (float y = fmodf(relative_offset.y, GRID_SIZE); y < this->graph_state.canvas.size.y; y += GRID_SIZE) {
        draw_list->AddLine(ImVec2(0.0f, y) + this->graph_state.canvas.position,
            ImVec2(this->graph_state.canvas.size.x, y) + this->graph_state.canvas.position, COLOR_GRID);
    }
}


void megamol::gui::configurator::Graph::Presentation::present_canvas_dragged_call(
    megamol::gui::configurator::Graph& inout_graph) {

    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
        if (payload->IsDataType(GUI_DND_CALLSLOT_UID_TYPE)) {
            ImGuiID* selected_slot_uid_ptr = (ImGuiID*)payload->Data;
            if (selected_slot_uid_ptr == nullptr) {
                vislib::sys::Log::DefaultLog.WriteError("Pointer to drag and drop payload data is nullptr. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
                return;
            }

            ImGuiStyle& style = ImGui::GetStyle();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            // Color
            const auto COLOR_CALL_CURVE = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Button]);

            ImVec2 current_pos = ImGui::GetMousePos();
            bool mouse_inside_canvas = false;
            if ((current_pos.x >= this->graph_state.canvas.position.x) &&
                (current_pos.x <= (this->graph_state.canvas.position.x + this->graph_state.canvas.size.x)) &&
                (current_pos.y >= this->graph_state.canvas.position.y) &&
                (current_pos.y <= (this->graph_state.canvas.position.y + this->graph_state.canvas.size.y))) {
                mouse_inside_canvas = true;
            }
            if (mouse_inside_canvas) {

                bool found_valid_slot = false;
                ImVec2 p1;

                CallSlotPtrType selected_callslot_ptr;
                for (auto& module_ptr : inout_graph.GetModules()) {
                    CallSlotPtrType callslot_ptr;
                    if (module_ptr->GetCallSlot((*selected_slot_uid_ptr), callslot_ptr)) {
                        selected_callslot_ptr = callslot_ptr;
                    }
                }
                if (selected_callslot_ptr != nullptr) {
                    p1 = selected_callslot_ptr->GUI_GetPosition();
                    found_valid_slot = true;
                }
                if (!found_valid_slot) {
                    InterfaceSlotPtrType selected_interfaceslot_ptr;
                    for (auto& group_ptr : inout_graph.GetGroups()) {
                        InterfaceSlotPtrType interfaceslot_ptr;
                        if (group_ptr->GetInterfaceSlot((*selected_slot_uid_ptr), interfaceslot_ptr)) {
                            selected_interfaceslot_ptr = interfaceslot_ptr;
                        }
                    }
                    if (selected_interfaceslot_ptr != nullptr) {
                        p1 = selected_interfaceslot_ptr->GUI_GetPosition();
                        found_valid_slot = true;
                    }
                }
                
                if (found_valid_slot) {
                    ImVec2 p2 = ImGui::GetMousePos();
                    if (p2.x < p1.x) {
                        ImVec2 tmp = p1;
                        p1 = p2;
                        p2 = tmp;
                    }
                    if (glm::length(glm::vec2(p1.x, p1.y) - glm::vec2(p2.x, p2.y)) > GUI_SLOT_RADIUS) {
                        draw_list->AddBezierCurve(p1, p1 + ImVec2(+50, 0), p2 + ImVec2(-50, 0), p2, COLOR_CALL_CURVE,
                            GUI_LINE_THICKNESS * this->graph_state.canvas.zooming);
                    }
                }
            }
        }
    }
}


void megamol::gui::configurator::Graph::Presentation::present_canvas_multiselection(Graph& inout_graph) {

    bool no_graph_item_selected = ((this->graph_state.interact.callslot_selected_uid == GUI_INVALID_ID) &&
                                   (this->graph_state.interact.call_selected_uid == GUI_INVALID_ID) &&
                                   (this->graph_state.interact.modules_selected_uids.empty()) &&
                                   (this->graph_state.interact.interfaceslot_selected_uid == GUI_INVALID_ID) &&
                                   (this->graph_state.interact.group_selected_uid == GUI_INVALID_ID));

    if (no_graph_item_selected && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {

        this->multiselect_end_pos = ImGui::GetMousePos();
        this->multiselect_done = true;

        ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        assert(draw_list != nullptr);

        ImVec4 tmpcol = style.Colors[ImGuiCol_FrameBg];
        tmpcol.w = 0.2f; // alpha
        const ImU32 COLOR_MULTISELECT_BACKGROUND = ImGui::ColorConvertFloat4ToU32(tmpcol);
        const ImU32 COLOR_MULTISELECT_BORDER = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border]);

        draw_list->AddRectFilled(multiselect_start_pos, multiselect_end_pos, COLOR_MULTISELECT_BACKGROUND,
            GUI_RECT_CORNER_RADIUS, ImDrawCornerFlags_All);

        float border = 1.0f;
        draw_list->AddRect(multiselect_start_pos, multiselect_end_pos, COLOR_MULTISELECT_BORDER, GUI_RECT_CORNER_RADIUS,
            ImDrawCornerFlags_All, border);
    } else if (this->multiselect_done && ImGui::IsWindowHovered() && ImGui::IsMouseReleased(0)) {
        ImVec2 outer_rect_min = ImVec2(std::min(this->multiselect_start_pos.x, this->multiselect_end_pos.x),
            std::min(this->multiselect_start_pos.y, this->multiselect_end_pos.y));
        ImVec2 outer_rect_max = ImVec2(std::max(this->multiselect_start_pos.x, this->multiselect_end_pos.x),
            std::max(this->multiselect_start_pos.y, this->multiselect_end_pos.y));
        ImVec2 inner_rect_min, inner_rect_max;
        ImVec2 module_size;
        this->graph_state.interact.modules_selected_uids.clear();
        for (auto& module_ptr : inout_graph.modules) {
            bool group_member = (module_ptr->GUI_GetGroupUID() != GUI_INVALID_ID);
            if (!group_member || (group_member && module_ptr->GUI_IsVisibleInGroup())) {
                module_size = module_ptr->GUI_GetSize() * this->graph_state.canvas.zooming;
                inner_rect_min =
                    this->graph_state.canvas.offset + module_ptr->GUI_GetPosition() * this->graph_state.canvas.zooming;
                inner_rect_max = inner_rect_min + module_size;
                if (((outer_rect_min.x < inner_rect_max.x) && (outer_rect_max.x > inner_rect_min.x) &&
                        (outer_rect_min.y < inner_rect_max.y) && (outer_rect_max.y > inner_rect_min.y))) {
                    this->graph_state.interact.modules_selected_uids.emplace_back(module_ptr->uid);
                }
            }
        }
        this->multiselect_done = false;
    } else {
        this->multiselect_start_pos = ImGui::GetMousePos();
    }
}


bool megamol::gui::configurator::Graph::Presentation::layout_graph(megamol::gui::configurator::Graph& inout_graph) {

    std::vector<std::vector<ModulePtrType>> layers;
    layers.clear();

    // Fill first layer with modules having no connected callee
    layers.emplace_back();
    for (auto& mod : inout_graph.GetModules()) {
        bool any_connected_callee = false;
        for (auto& callee_slot : mod->GetCallSlots(CallSlotType::CALLEE)) {
            if (callee_slot->CallsConnected()) {
                any_connected_callee = true;
            }
        }
        if (!any_connected_callee) {
            layers.back().emplace_back(mod);
        }
    }

    // Loop while modules are added to new layer.
    bool added_module = true;
    while (added_module) {
        added_module = false;
        // Add new layer
        layers.emplace_back();
        // Loop through last filled layer
        for (auto& layer_mod : layers[layers.size() - 2]) {
            for (auto& caller_slot : layer_mod->GetCallSlots(CallSlotType::CALLER)) {
                if (caller_slot->CallsConnected()) {
                    for (auto& call : caller_slot->GetConnectedCalls()) {
                        auto add_mod = call->GetCallSlot(CallSlotType::CALLEE)->GetParentModule();

                        // Add module only if not already present in current layer
                        bool module_already_added = false;
                        for (auto& last_layer_mod : layers.back()) {
                            if (last_layer_mod == add_mod) {
                                module_already_added = true;
                            }
                        }
                        if (!module_already_added) {
                            layers.back().emplace_back(add_mod);
                            added_module = true;
                        }
                    }
                }
            }
        }
    }

    // Deleting duplicate modules from back to front
    int layer_size = static_cast<int>(layers.size());
    for (int i = (layer_size - 1); i >= 0; i--) {
        for (auto& layer_module : layers[i]) {
            for (int j = (i - 1); j >= 0; j--) {
                for (auto module_iter = layers[j].begin(); module_iter != layers[j].end(); module_iter++) {
                    if ((*module_iter) == layer_module) {
                        layers[j].erase(module_iter);
                        break;
                    }
                }
            }
        }
    }

    // Calculate new positions of modules
    ImVec2 init_position = megamol::gui::configurator::Module::GUI_GetInitModulePosition(this->graph_state.canvas);
    ImVec2 pos = init_position;
    float max_call_width = 25.0f;
    float max_module_width = 0.0f;
    size_t layer_mod_cnt = 0;
    for (auto& layer : layers) {
        if (this->show_call_names) {
            max_call_width = 0.0f;
        }
        max_module_width = 0.0f;
        layer_mod_cnt = layer.size();
        pos.y = init_position.y;
        for (size_t i = 0; i < layer_mod_cnt; i++) {
            auto mod = layer[i];
            if (this->show_call_names) {
                for (auto& caller_slot : mod->GetCallSlots(CallSlotType::CALLER)) {
                    if (caller_slot->CallsConnected()) {
                        for (auto& call : caller_slot->GetConnectedCalls()) {
                            auto call_name_length = GUIUtils::TextWidgetWidth(call->class_name);
                            max_call_width =
                                (call_name_length > max_call_width) ? (call_name_length) : (max_call_width);
                        }
                    }
                }
            }
            mod->GUI_SetPosition(pos);
            auto mod_size = mod->GUI_GetSize();
            pos.y += mod_size.y + 2.0f * GUI_GRAPH_BORDER;
            max_module_width = (mod_size.x > max_module_width) ? (mod_size.x) : (max_module_width);
        }
        pos.x += (max_module_width + max_call_width + 2.0f * GUI_GRAPH_BORDER);
    }

    return true;
}
