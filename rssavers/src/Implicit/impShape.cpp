/*
 * Copyright (C) 2001-2005  Terence M. Welsh
 *
 * This file is part of Implicit.
 *
 * Implicit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
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


#include <Implicit/impShape.h>



impShape::impShape(){
	mat[0] = mat[5] = mat[10] = mat[15]
		= invmat[0] = invmat[5] = invmat[10] = invmat[15] = 1.0f;
	mat[1] = mat[2] = mat[3] = mat[4]
		= mat[6] = mat[7] = mat[8] = mat[9]
		= mat[11] = mat[12] = mat[13] = mat[14]
		= invmat[1] = invmat[2] = invmat[3] = invmat[4]
		= invmat[6] = invmat[7] = invmat[8] = invmat[9]
		= invmat[11] = invmat[12] = invmat[13] = invmat[14] = 0.0f;

	thickness = 1.0f;
	thicknessSquared = thickness * thickness;
}


void impShape::setPosition(float x, float y, float z){
	mat[12] = x;
	mat[13] = y;
	mat[14] = z;
	invmat[12] = -x;
	invmat[13] = -y;
	invmat[14] = -z;
}


void impShape::setPosition(float* position){
	mat[12] = position[0];
	mat[13] = position[1];
	mat[14] = position[2];
	invmat[12] = -position[0];
	invmat[13] = -position[1];
	invmat[14] = -position[2];
}


// Don't need to set this for simple spheres.
// A whole matrix is only necessary for weird asymmetric objects.
void impShape::setMatrix(float* m){
	for(unsigned int i=0; i<16; ++i)
		mat[i] = m[i];
	
	invertMatrix();
}


float impShape::determinant3(const float a1, const float a2, const float a3,
	const float b1, const float b2, const float b3,
	const float c1, const float c2, const float c3){
	return (a1 * b2 * c3) + (a2 * b3 * c1) + (a3 * b1 * c2)
		- (a1 * b3 * c2) - (a2 * b1 * c3) - (a3 * b2 * c1); 
}


bool impShape::invertMatrix(){
	const float a1(mat[0]);
	const float b1(mat[1]);
	const float c1(mat[2]);
	const float d1(mat[3]);
	const float a2(mat[4]);
	const float b2(mat[5]);
	const float c2(mat[6]);
	const float d2(mat[7]);
	const float a3(mat[8]);
	const float b3(mat[9]);
	const float c3(mat[10]);
	const float d3(mat[11]);
	const float a4(mat[12]);
	const float b4(mat[13]);
	const float c4(mat[14]);
	const float d4(mat[15]);

	// calculate determinant
	const float d3_1(determinant3(b2, b3, b4, c2, c3, c4, d2, d3, d4));
	const float d3_2(-determinant3(a2, a3, a4, c2, c3, c4, d2, d3, d4));
	const float d3_3(determinant3(a2, a3, a4, b2, b3, b4, d2, d3, d4));
	const float d3_4(-determinant3(a2, a3, a4, b2, b3, b4, c2, c3, c4));
    const float det(a1 * d3_1 + b1 * d3_2 + c1 * d3_3 + d1 * d3_4);

	if(fabs(det) < 0.000001f)
		return false;  // matrix is singular, cannot be inverted

	// reciprocal of determinant
    const float rec_det(1.0f / det);

	// calculate inverted matrix
	invmat[0]  =   d3_1 * rec_det;
	invmat[4]  =   d3_2 * rec_det;
	invmat[8]  =   d3_3 * rec_det;
	invmat[12] =   d3_4 * rec_det;
	invmat[1]  = - determinant3(b1, b3, b4, c1, c3, c4, d1, d3, d4) * rec_det;
	invmat[5]  =   determinant3(a1, a3, a4, c1, c3, c4, d1, d3, d4) * rec_det;
	invmat[9]  = - determinant3(a1, a3, a4, b1, b3, b4, d1, d3, d4) * rec_det;
	invmat[13] =   determinant3(a1, a3, a4, b1, b3, b4, c1, c3, c4) * rec_det;
	invmat[2]  =   determinant3(b1, b2, b4, c1, c2, c4, d1, d2, d4) * rec_det;
	invmat[6]  = - determinant3(a1, a2, a4, c1, c2, c4, d1, d2, d4) * rec_det;
	invmat[10] =   determinant3(a1, a2, a4, b1, b2, b4, d1, d2, d4) * rec_det;
	invmat[14] = - determinant3(a1, a2, a4, b1, b2, b4, c1, c2, c4) * rec_det;
	invmat[3]  = - determinant3(b1, b2, b3, c1, c2, c3, d1, d2, d3) * rec_det;
	invmat[7]  =   determinant3(a1, a2, a3, c1, c2, c3, d1, d2, d3) * rec_det;
	invmat[11] = - determinant3(a1, a2, a3, b1, b2, b3, d1, d2, d3) * rec_det;
	invmat[15] =   determinant3(a1, a2, a3, b1, b2, b3, c1, c2, c3) * rec_det;

	return true; 
}


float impShape::value(float* position){
	return(0.0f);
}


void impShape::center(float* position){
	position[0] = mat[12];
	position[1] = mat[13];
	position[2] = mat[14];
}


void impShape::addCrawlPoint(impCrawlPointVector &cpv){
	cpv.push_back(impCrawlPoint(&(mat[12])));
}
