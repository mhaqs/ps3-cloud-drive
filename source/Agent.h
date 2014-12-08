/*
# Playstation 3 Cloud Drive
# Copyright (C) 2013-2014 Mohammad Haseeb aka MHAQS
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
*/

#pragma once

#include "Receivable.h"
#include "StdioFile.h"
#include "Header.h"
#include <string>

class Agent
{
	public :
		virtual long Put(
			const std::string&	url,
			const std::string&	data,
			Receivable			*dest,
			const Header&		hdr ) = 0 ;

		virtual long Put(
			const std::string&	url,
			StdioFile&			file,
			Receivable			*dest,
			const Header&		hdr ) = 0 ;
		
		virtual long Get(
			const std::string& 	url,
			Receivable			*dest,
			const Header&		hdr ) = 0 ;
	
		virtual long Post(
			const std::string& 	url,
			const std::string&	data,
			Receivable			*dest,
			const Header&		hdr ) = 0 ;
	
		virtual long Custom(
			const std::string&	method,
			const std::string&	url,
			Receivable			*dest,
			const Header&		hdr ) = 0 ;
	
		virtual std::string RedirLocation() const = 0 ;
	
		virtual std::string Escape( const std::string& str ) = 0 ;
		virtual std::string Unescape( const std::string& str ) = 0 ;
} ;
