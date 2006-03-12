/*
 * Copyright (C) 2006  Terence M. Welsh
 *
 * This file is part of rsUtility.
 *
 * rsUtility is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * rsUtility is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef ARGUMENTS_H
#define ARGUMENTS_H



#include <string>
#include <sstream>
#include <iostream>
#include <map>



// Handy template for converting std::string to numeric types.
template <class T>
bool from_string(T& t, const std::string& str, std::ios_base& (*f)(std::ios_base&)){
	std::istringstream iss(str);
	return !(iss >> f >> t).fail();
}



/*typedef union{
	int i;
	float f;
	bool b;
} arg_union;*/

typedef struct{
	int arg;
	int min;
	int max;
} argument_int_t;

// Map argument name to its value, min value, and max value.
//typedef std::map<std::string, argument_t> argument_int_map_t;



// Find the specified argument in the input stream.
// Return argument index if found.
// Otherwise, return -1.
int findArgument(int argc, char* argv[], std::string arg);


// Fetch the value immediately following the specified argument.
// There are versions for std::string, int, float, and double
// Return the index of the value if one is found.
// Otherwise, return -1.
int getArgumentsValue(int argc, char* argv[], std::string arg, std::string& value);

int getArgumentsValue(int argc, char* argv[], std::string arg, int& value);
int getArgumentsValue(int argc, char* argv[], std::string arg, int& value, int minimum, int maximum);

int getArgumentsValue(int argc, char* argv[], std::string arg, float& value);
int getArgumentsValue(int argc, char* argv[], std::string arg, float& value, float minimum, float maximum);

int getArgumentsValue(int argc, char* argv[], std::string arg, double& value);
int getArgumentsValue(int argc, char* argv[], std::string arg, double& value, double minimum, double maximum);



#endif
