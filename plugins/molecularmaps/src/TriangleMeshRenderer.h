/*
 * TriangleMeshRenderer.h
 * Copyright (C) 2006-2016 by MegaMol Team
 * Alle Rechte vorbehalten.
 */
#ifndef MMMOLMAPPLG_TRIANGLEMESHRENDERER_H_INCLUDED
#define MMMOLMAPPLG_TRIANGLEMESHRENDERER_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "AbstractLocalRenderer.h"

namespace megamol {
namespace molecularmaps {

	class TriangleMeshRenderer : public AbstractLocalRenderer {
	public:
		/** Ctor */
		TriangleMeshRenderer(void);

		/** Dtor */
		virtual ~TriangleMeshRenderer(void);

		/**
		 * Initializes the renderer
		 */
		virtual bool create(void);

		/**
		 * Invokes the rendering calls
		 */
		virtual bool Render(core::view::CallRender3D& call);

        /**
         * Invokes the rendering calls using wireframe rendering
         */
        virtual bool RenderWireFrame(core::view::CallRender3D& call);

		/**
		 * Update function for the local data to render
		 *
		 * @param faces Pointer to the vector containing the face vertex indices
		 * @param vertices Pointer to the vector containing the vertex positions
		 * @param vertex_colors Pointer to the vector containing the vertex colors.
		 * @param vertex_normals Pointer to the vector containing the vertex normals.
		 * @param numValuesPerColor The number of color values per vertex. (1 for intensity, 3 for RGB, 4 for RGBA. Standard: RGB)
		 * @return True on success, false otherwise.
		 */
		bool update(const std::vector<uint> * faces, const std::vector<float> * vertices, const std::vector<float> * vertex_colors, 
			const std::vector<float> * vertex_normals, unsigned int numValuesPerColor = 3);

	protected:

		/**
		 * Frees all needed resources used by this renderer
		 */
		virtual void release(void);

	private:

		/** Pointer to the vector containing the face vertex indices */
		const std::vector<uint> * faces;

		/** Number of color values per color in vertex_colors */
		unsigned int numValuesPerColor;

		/** Pointer to the vector containing the face vertex colors */
		const std::vector<float> * vertex_colors;

		/** Pointer to the vector containing the face vertex normals */
		const std::vector<float> * vertex_normals;

		/** Pointer to the vector containing the vertex positions */
		const std::vector<float> * vertices;
	};

} /* end namespace molecularmaps */
} /* end namespace megamol */

#endif /* MMMOLMAPPLG_TRIANGLEMESHRENDERER_H_INCLUDED */