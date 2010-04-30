/*
 * Copyright (C) 2001-2010  Terence M. Welsh
 *
 * This file is part of Implicit.
 *
 * Implicit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Implicit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef IMPSURFACE_H
#define IMPSURFACE_H


#ifdef WIN32
	#include <windows.h>
#endif
#include <vector>
#include <GL/gl.h>


// set only one of these to 1 to specify draw method
#define IMM_DRAW 0  // immediate mode
#define DRAWARRAY_DRAW 1  // draw arrays
#define VBO_DRAW 0  // Vertex Buffer Objects (won't work in Windows without initializing glGenBuffersARB)


class impSurface{
private:
	unsigned int num_tristrips;
	unsigned int index_offset;
	unsigned int vertex_offset;
	std::vector<unsigned int> triStripLengths;
	std::vector<unsigned int> indices;
	std::vector<float> vertices;
	size_t vertex_data_size;

	bool mCompile;

	// display list
	GLuint mDisplayList;

	// vbo
#if VBO_DRAW
	GLuint vbo_array_id;
	GLuint vbo_index_id;
	std::vector<GLvoid*> vbo_index_offsets;
#endif

public:
	impSurface();
	~impSurface();

	// Set data counts to 0
	void reset();

	// Add data to surface
	void addTriStripLength(unsigned char length);
	void addIndex(unsigned int index);
	void addVertex(float* data);  // provide array of 6 floats (normal, position)
	
	// Compute normals from triangle data
	// This is fast, but not quite as fast as the fast normals in
	// impCubeVolume, and it also looks a lot worse.  Therefore, it
	// is not used anymore.
	void calculateNormals();

	void draw();
	void draw_wireframe();

	// convenient vector math functions
	inline void addvec(float* dest, float* a, float* b);
	inline void subvec(float* dest, float* a, float* b);
	inline void cross(float* dest, float* a, float* b);
};



#endif
