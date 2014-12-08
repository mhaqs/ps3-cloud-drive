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

#include <sys/file.h>
#include <sys/stat.h>
#include <string>

#define fis_dir(entry) S_ISDIR(entry.st_mode)

class StdioFile
{
public :
	StdioFile( const std::string& path ) ;
	StdioFile( const std::string& path, int chmod ) ;
	StdioFile( const std::string& path, int flags, int chmod ) ;
	~StdioFile( ) ;

	void OpenForRead() ;
	void OpenWithFlags(int flags, int chmod );
	void OpenForWrite( int chmod = 0600 ) ;
	void Close() ;
	bool IsOpened() const ;
	
	size_t Read( void *ptr, size_t size ) ;
	size_t Write( const void *ptr, size_t size ) ;
        
        s32 fileStatus() const;

	u64 Seek( s64 offset, int whence ) ;
	u64 Tell()  ;
	u64 Size() const;
        
        bool Exists();
        s32 IsDir();
	
	void Chmod(int chmod );
	
private :
	void Open( int flags, int chmod ) ;
	
private :
	s32	m_fd ;
        s32     m_ret;
        u64     m_read;
        u64     m_written;
        u64     m_pos;
	std::string m_path;
} ;
