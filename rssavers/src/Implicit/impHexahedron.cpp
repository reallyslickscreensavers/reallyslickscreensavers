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


#include <Implicit/impHexahedron.h>



float impHexahedron::value(float* position){
	const float x(position[0]);
	const float y(position[1]);
	const float z(position[2]);

	const float tx(x * invmat[0] + y * invmat[4] + z * invmat[8] + invmat[12]);
	const float ty(x * invmat[1] + y * invmat[5] + z * invmat[9] + invmat[13]);
	const float tz(x * invmat[2] + y * invmat[6] + z * invmat[10] + invmat[14]);

	const float xx(1.0f / (tx * tx));
	const float yy(1.0f / (ty * ty));
	const float zz(1.0f / (tz * tz));
	if(xx < yy){
		if(xx < zz)
			return(xx);
		else
			return(zz);
	}
	else{
		if(yy < zz)
			return(yy);
		else
			return(zz);
	}
}
