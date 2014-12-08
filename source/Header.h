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

#include <iosfwd>
#include <string>
#include <vector>

class Header
{
	private :
		typedef std::vector<std::string>	Vec ;

	public :
		typedef Vec::const_iterator	iterator ;

	public :
		Header() ;
	
		void Add( const std::string& str ) ;
	
		iterator begin() const ;
		iterator end() const ;

	private :
		Vec	m_vec ;
} ;

std::ostream& operator<<( std::ostream& os, const Header& h ) ;
Header operator+( const Header& header, const std::string& str ) ;