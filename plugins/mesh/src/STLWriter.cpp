#include "stdafx.h"

#include "STLWriter.h"

#include "mesh/TriangleMeshCall.h"

#include "mmcore/param/ButtonParam.h"
#include "mmcore/param/FilePathParam.h"

#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace megamol {
namespace mesh {
    STLWriter::STLWriter()
            : mesh_lhs_slot("mesh_lhs_slot", "Separated surface meshes representing pores and throats.")
            , mesh_rhs_slot("mesh_rhs_slot", "Input surface mesh of the fluid phase.")
            , filename("filename", "Name for the output STL file.")
            , save("save", "Save to STL file.")
            , triggered(false) {

        // Connect input slot
        this->mesh_rhs_slot.SetCompatibleCall<mesh::TriangleMeshCall::triangle_mesh_description>();
        this->MakeSlotAvailable(&this->mesh_rhs_slot);

        // Connect output slots
        this->mesh_lhs_slot.SetCallback(
            mesh::TriangleMeshCall::ClassName(), "get_data", &STLWriter::getMeshDataCallback);
        this->mesh_lhs_slot.SetCallback(
            mesh::TriangleMeshCall::ClassName(), "get_extent", &STLWriter::getMeshMetaDataCallback);
        this->MakeSlotAvailable(&this->mesh_lhs_slot);

        // Initialize parameter slots
        this->filename << new core::param::FilePathParam("mesh.stl", core::param::FilePathParam::FLAG_TOBECREATED);
        this->MakeSlotAvailable(&this->filename);

        this->save << new core::param::ButtonParam();
        this->save.SetUpdateCallback(&STLWriter::setButtonPressed);
        this->MakeSlotAvailable(&this->save);
    }

    STLWriter::~STLWriter() {
        this->Release();
    }

    bool STLWriter::create() {
        return true;
    }

    void STLWriter::release() {}

    bool STLWriter::getMeshDataCallback(core::Call& _call) {
        assert(dynamic_cast<mesh::TriangleMeshCall*>(&_call) != nullptr);

        auto& call = static_cast<mesh::TriangleMeshCall&>(_call);

        // Check input connection and get data
        auto tmc_ptr = this->mesh_rhs_slot.CallAs<mesh::TriangleMeshCall>();

        if (tmc_ptr != nullptr) {
            auto& tmc = *tmc_ptr;

            if (tmc(0)) {
                call.set_vertices(tmc.get_vertices());
                call.set_normals(tmc.get_normals());
                call.set_indices(tmc.get_indices());

                call.SetDataHash(tmc.DataHash());

                if (this->triggered) {
                    this->triggered = false;

                    return write(this->filename.Param<core::param::FilePathParam>()->Value().PeekBuffer(),
                        *tmc.get_vertices().get(), *tmc.get_normals().get(), *tmc.get_indices().get());
                }

                return true;
            }
        }

        if (this->triggered) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "Unable to get data. Is it connected? [%s, %s, line %d]\n", __FILE__, __FUNCTION__,
                __LINE__);

            this->triggered = false;
        }

        return false;
    }

    bool STLWriter::getMeshMetaDataCallback(core::Call& _call) {
        assert(dynamic_cast<mesh::TriangleMeshCall*>(&_call) != nullptr);

        auto& call = static_cast<mesh::TriangleMeshCall&>(_call);

        // Get input extent
        auto tmc_ptr = this->mesh_rhs_slot.CallAs<mesh::TriangleMeshCall>();

        if (tmc_ptr == nullptr || !(*tmc_ptr)(1)) {
            return false;
        }

        call.set_dimension(tmc_ptr->get_dimension());
        call.set_bounding_box(tmc_ptr->get_bounding_box());

        return true;
    }

    bool STLWriter::setButtonPressed(core::param::ParamSlot& slot) {
        this->triggered = true;

        slot.ResetDirty();

        return true;
    }

    bool STLWriter::write(const std::string& filename, const std::vector<float>& vertices,
        const std::vector<float>& normals, const std::vector<unsigned int>& indices) const {

        // Open output file
        try {
            std::ofstream ofs(filename, std::ios_base::out | std::ios_base::binary);

            if (!ofs.good()) {
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    "Unable to open output STL file '%s'. [%s, %s, line %d]\n", filename.c_str(), __FILE__,
                    __FUNCTION__, __LINE__);

                return false;
            }

            const uint32_t num_triangles = static_cast<uint32_t>(indices.size() / 3);

            const std::array<float, 3> dummy_normal{0.0f, 0.0f, 0.0f};
            const uint16_t dummy_value = 0;

            // Store as binary STL file
            if (true) {
                // Set number of triangles in header
                for (int i = 0; i < 80; ++i)
                    ofs.put(' ');

                ofs.write(reinterpret_cast<const char*>(&num_triangles), sizeof(uint32_t));

                // Store triangles
                for (std::size_t triangle_index = 0; triangle_index < static_cast<std::size_t>(num_triangles);
                     ++triangle_index) {

                    const std::array<unsigned int, 3> vertex_indices{indices[triangle_index * 3 + 0],
                        indices[triangle_index * 3 + 1], indices[triangle_index * 3 + 2]};

                    if (normals.empty()) {
                        // TODO: calculate normal
                        ofs.write(reinterpret_cast<const char*>(dummy_normal.data()), 3 * sizeof(float));
                    } else {
                        ofs.write(reinterpret_cast<const char*>(&normals[3uLL * vertex_indices[0]]), 3 * sizeof(float));
                    }

                    ofs.write(reinterpret_cast<const char*>(&vertices[3uLL * vertex_indices[0]]), 3 * sizeof(float));
                    ofs.write(reinterpret_cast<const char*>(&vertices[3uLL * vertex_indices[1]]), 3 * sizeof(float));
                    ofs.write(reinterpret_cast<const char*>(&vertices[3uLL * vertex_indices[2]]), 3 * sizeof(float));
                    ofs.write(reinterpret_cast<const char*>(&dummy_value), sizeof(uint16_t));
                }
            }

            // Store as ASCII STL file
            else {
                const std::string identifier("solid");
                const std::string facet("  facet");
                const std::string normal("    normal");
                const std::string outer("    outer");
                const std::string loop("    loop");
                const std::string vertex("      vertex");
                const std::string end_loop("    endloop");
                const std::string end_facet("  endfacet");
                const std::string end_tag("endsolid");

                // Write identifier in header
                ofs.write(identifier.c_str(), identifier.size() * sizeof(char));
                ofs.put('\n');

                // Store triangles
                for (std::size_t triangle_index = 0; triangle_index < static_cast<std::size_t>(num_triangles);
                     ++triangle_index) {

                    const std::array<unsigned int, 3> vertex_indices{indices[triangle_index * 3 + 0],
                        indices[triangle_index * 3 + 1], indices[triangle_index * 3 + 2]};

                    // Write facet identifier
                    ofs.write(facet.c_str(), facet.size() * sizeof(char));
                    ofs.put('\n');

                    // Write normal
                    ofs.write(normal.c_str(), normal.size() * sizeof(char));
                    ofs.put('\t');

                    std::stringstream normal_s;

                    if (normals.empty()) {
                        // TODO: calculate normal
                        normal_s << dummy_normal[0] << " " << dummy_normal[1] << " " << dummy_normal[2];
                    } else {
                        normal_s << normals[3uLL * vertex_indices[0] + 0] << " "
                                 << normals[3uLL * vertex_indices[0] + 1] << " "
                                 << normals[3uLL * vertex_indices[0] + 2];
                    }

                    ofs.write(normal_s.str().c_str(), normal_s.str().size() * sizeof(char));
                    ofs.put('\n');

                    // Write vertices
                    ofs.write(outer.c_str(), outer.size() * sizeof(char));
                    ofs.put('\n');
                    ofs.write(loop.c_str(), loop.size() * sizeof(char));
                    ofs.put('\n');

                    ofs.write(vertex.c_str(), vertex.size() * sizeof(char));
                    ofs.put('\t');

                    std::stringstream vertex_1_s;
                    vertex_1_s << vertices[3uLL * vertex_indices[0] + 0] << " "
                               << vertices[3uLL * vertex_indices[0] + 1] << " "
                               << vertices[3uLL * vertex_indices[0] + 2];

                    ofs.write(vertex_1_s.str().c_str(), vertex_1_s.str().size() * sizeof(char));
                    ofs.put('\n');

                    ofs.write(vertex.c_str(), vertex.size() * sizeof(char));
                    ofs.put('\t');

                    std::stringstream vertex_2_s;
                    vertex_2_s << vertices[3uLL * vertex_indices[1] + 0] << " "
                               << vertices[3uLL * vertex_indices[1] + 1] << " "
                               << vertices[3uLL * vertex_indices[1] + 2];

                    ofs.write(vertex_2_s.str().c_str(), vertex_2_s.str().size() * sizeof(char));
                    ofs.put('\n');

                    ofs.write(vertex.c_str(), vertex.size() * sizeof(char));
                    ofs.put('\t');

                    std::stringstream vertex_3_s;
                    vertex_3_s << vertices[3uLL * vertex_indices[2] + 0] << " "
                               << vertices[3uLL * vertex_indices[2] + 1] << " "
                               << vertices[3uLL * vertex_indices[2] + 2];

                    ofs.write(vertex_3_s.str().c_str(), vertex_3_s.str().size() * sizeof(char));
                    ofs.put('\n');

                    ofs.write(end_loop.c_str(), end_loop.size() * sizeof(char));
                    ofs.put('\n');

                    // Write facet end
                    ofs.write(end_facet.c_str(), end_facet.size() * sizeof(char));
                    ofs.put('\n');
                }

                // Finalize with end tag
                ofs.write(end_tag.c_str(), end_tag.size() * sizeof(char));
                ofs.put('\n');
            }

            // Finish writing
            ofs.close();
        } catch (const std::exception& e) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "Unable to write STL file '%s': %s. [%s, %s, line %d]\n", filename.c_str(), e.what(), __FILE__,
                __FUNCTION__, __LINE__);

            return false;
        }

        megamol::core::utility::log::Log::DefaultLog.WriteInfo("Saved STL file '%s'.\n", filename.c_str());

        return true;
    }
} // namespace mesh
} // namespace megamol
