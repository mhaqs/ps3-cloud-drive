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

#include "StdioFile.h"
#include "log.h"
#include <iostream>

StdioFile::StdioFile( const std::string& path ) : m_fd( -1 ), m_path(path)
{
	OpenForRead() ;
}

StdioFile::StdioFile( const std::string& path, int flags, int chmod ) : m_fd( -1 ), m_path(path)
{
	OpenWithFlags(flags, chmod) ;
}

StdioFile::StdioFile( const std::string& path, int chmod ) : m_fd( -1 ), m_path(path)
{
	OpenForWrite(chmod) ;
}

StdioFile::~StdioFile( )
{
	Close() ;
}

s32 StdioFile::fileStatus() const
{
    return m_ret;
}

void StdioFile::Open( int flags, int chmod )
{
	if ( IsOpened() )
		Close() ;
	
	m_ret = sysFsOpen( m_path.c_str(), flags, &m_fd,NULL,0);
        
        if(m_ret == -1)
        {
            debugPrintf("Error Opening File\n");
        }
}

void StdioFile::OpenForRead( )
{
	Open( SYS_O_RDONLY, 0644 ) ;
}

void StdioFile::OpenWithFlags(int flags,int chmod )
{
	Open( flags, chmod ) ;
}

void StdioFile::OpenForWrite( int mode )
{
	Open(SYS_O_CREAT|SYS_O_RDWR, mode ) ;
}

void StdioFile::Close()
{
	if ( IsOpened() )
	{
		sysFsClose( m_fd ) ;
                m_fd = -1;
		m_ret = -1 ;
                m_pos = 0;
                m_read = 0;
                m_written = 0;
	}
}

bool StdioFile::IsOpened() const
{
	return m_fd != -1 ;
}

size_t StdioFile::Read( void *ptr, size_t size )
{
	m_ret = sysFsRead( m_fd, ptr, size, &m_read ) ;
        return m_read;
}

size_t StdioFile::Write( const void *ptr, size_t size )
{
	m_ret =  sysFsWrite( m_fd, ptr, size, &m_written ) ;
        return m_written;
}

u64 StdioFile::Seek( s64 offset, int whence )
{
	m_ret = sysFsLseek( m_fd, offset, whence, &m_pos ) ;
        return m_pos;
}

u64 StdioFile::Tell()
{
	m_ret = sysFsLseek( m_fd, 0, SEEK_CUR, &m_pos ) ;
        return m_pos;
}

u64 StdioFile::Size() const
{	
	sysFSStat stat;
	sysFsStat(m_path.c_str(),&stat) ;
	
	return stat.st_size;
}

void StdioFile::Chmod(int mode )
{
	if ( sysFsChmod(m_path.c_str(), mode ) != 0 )
	{
		debugPrintf("Error changing file permissions\n");
	}
}

bool StdioFile::Exists()
{
        sysFSStat entry;
        return sysFsStat(m_path.c_str(), &entry) == 0;
}

s32 StdioFile::IsDir()
{
        sysFSStat stat;
        return (sysFsStat(m_path.c_str(), &stat) == 0 && fis_dir(stat));
}