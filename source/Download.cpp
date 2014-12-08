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

#include "Download.h"
#include "log.h"

Download::Download( const std::string& filename ) :
	m_file( filename, 0600 )
{
}

Download::~Download()
{
}

void Download::Clear()
{
	// no need to do anything
}

std::size_t Download::OnData( void *data, std::size_t count )
{
	if( data == 0 ) debugPrintf("Download-OnData, no data\n");
	
	return m_file.Write( data, count ) ;
}