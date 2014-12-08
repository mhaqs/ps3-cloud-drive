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

#include "Agent.h"

#include <curl/curl.h>

#include <memory>
#include <string>

/*!	\brief	agent to provide HTTP access
	
	This class provides functions to send HTTP request in many methods (e.g. get, post and put).
	Normally the HTTP response is returned in a Receivable.
*/
class CurlAgent : public Agent
{
	public :
		CurlAgent() ;
		~CurlAgent() ;
	
		long Put(
			const std::string&	url,
			const std::string&	data,
			Receivable		*dest,
			const Header&		hdr ) ;
		
		long Put(
			const std::string&	url,
			StdioFile&		file,
			Receivable		*dest,
			const Header&		hdr ) ;
                
                long customPut(
			const std::string&	url,
			StdioFile&		file,
			Receivable		*dest,
                        curl_progress_callback  func,
			const Header&		hdr ) ;

		long Get(
			const std::string& 	url,
			Receivable		*dest,
			const Header&		hdr ) ;
                
                long customGet(
			const std::string& 	url,
			Receivable		*dest,
                        curl_progress_callback  func,
			const Header&		hdr ) ;
	
		long Post(
			const std::string& 	url,
			const std::string&	data,
			Receivable		*dest,
			const Header&		hdr ) ;
	
		long Custom(
			const std::string&	method,
			const std::string&	url,
			Receivable			*dest,
			const Header&		hdr ) ;
	
		std::string RedirLocation() const ;
		std::string CheckSSLSupport() const;
	
		std::string Escape( const std::string& str ) ;
		std::string Unescape( const std::string& str ) ;

	private :
		static std::size_t HeaderCallback( void *ptr, size_t size, size_t nmemb, CurlAgent *pthis ) ;
		static std::size_t Receive( void* ptr, size_t size, size_t nmemb, Receivable *recv ) ;
	
		void SetHeader( const Header& hdr ) ;
		long ExecCurl(
			const std::string&	url,
			Receivable			*dest,
			const Header&		hdr ) ;

		void Init() ;
	
	private :
		struct Impl ;
		std::auto_ptr<Impl>	m_pimpl ;
} ;
