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


#include <util/arguments.h>


// Find the specified argument in the input stream.
// Return argument index if found.
// Otherwise, return -1.
int findArgument(int argc, char* argv[], std::string arg){
	for(int i=0; i<argc; i++){
		if(std::string(argv[i]) == arg)
			return i;
	}
	return -1;
}


// Fetch the value immediately following the specified argument.
// There are versions for std::string, int, float, and double
// Return the index of the value if one is found.
// Otherwise, return -1.
int getArgumentsValue(int argc, char* argv[], std::string arg, std::string& value){
	for(int i=0; i<argc; i++){
		if(!strcmp(argv[i], arg.c_str())){
			if(argc >= i + 2){
				value = argv[i + 1];
				return i + 1;	
			}
		}
	}
	return -1;
}

// get an int
int getArgumentsValue(int argc, char* argv[], std::string arg, int& value){
	for(int i=0; i<argc; i++){
		if(std::string(argv[i]) == arg){
			if(argc > i + 1){
				bool success(false);
				int temp;
				if((argv[i+1][0] == '0') && 
					((argv[i+1][1] == 'x') || (argv[i+1][1] == 'X')))
					success = from_string<int>(temp, argv[i+1], std::hex);
				else
					success = from_string<int>(temp, argv[i+1], std::dec);
				if(success){
					value = temp;
					return i + 1;
				}
			}
		}
	}
	return -1;
}

// get an int and clamp it
int getArgumentsValue(int argc, char* argv[], std::string arg, int& value,
	int minimum, int maximum){
	if(minimum > maximum)
		return -1;

	int retval = getArgumentsValue(argc, argv, arg, value);
	if(retval != -1){
		if(value < minimum)
			value = minimum;
		if(value > maximum)
			value = maximum;
	}
	return retval;
}

// get a float
int getArgumentsValue(int argc, char* argv[], std::string arg, float& value){
	for(int i=0; i<argc; i++){
		if(std::string(argv[i]) == arg){
			if(argc > i + 1){
				float temp;
				if(from_string<float>(temp, argv[i+1], std::dec)){
					value = temp;
					return i + 1;
				}
			}
		}
	}
	return -1;
}

// get a float and clamp it
int getArgumentsValue(int argc, char* argv[], std::string arg, float& value,
	float minimum, float maximum){
	if(minimum > maximum)
		return -1;

	int retval = getArgumentsValue(argc, argv, arg, value);
	if(retval != -1){
		if(value < minimum)
			value = minimum;
		if(value > maximum)
			value = maximum;
	}
	return retval;
}

// get a double
int getArgumentsValue(int argc, char* argv[], std::string arg, double& value){
	for(int i=0; i<argc; i++){
		if(std::string(argv[i]) == arg){
			if(argc > i + 1){
				double temp;
				if(from_string<double>(temp, argv[i+1], std::dec)){
					value = temp;
					return i + 1;
				}
			}
		}
	}
	return -1;
}

// get a double and clamp it
int getArgumentsValue(int argc, char* argv[], std::string arg, double& value,
	double minimum, double maximum){
	if(minimum > maximum)
		return -1;

	int retval = getArgumentsValue(argc, argv, arg, value);
	if(retval != -1){
		if(value < minimum)
			value = minimum;
		if(value > maximum)
			value = maximum;
	}
	return retval;
}
