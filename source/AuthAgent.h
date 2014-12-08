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
#include "OAuth2.h"

#include <memory>

/*!	\brief	An HTTP agent with support OAuth2
	
	This is a HTTP agent that provide support for OAuth2. It will also perform retries on
	certain HTTP errors.
*/
class AuthAgent : public Agent
{
	public :
		AuthAgent( const OAuth2& auth, std::auto_ptr<Agent> real_agent ) ;

		long Put(
			const std::string&	url,
			const std::string&	data,
			Receivable			*dest,
			const Header&		hdr ) ;

		long Put(
			const std::string&	url,
			StdioFile&			file,
			Receivable			*dest,
			const Header&		hdr ) ;

		long Get(
			const std::string& 	url,
			Receivable	*dest,
			const Header&	hdr ) ;
	
		long Post(
			const std::string& 	url,
			const std::string&	data,
			Receivable	*dest,
			const Header&	hdr ) ;
	
		long Custom(
			const std::string&	method,
			const std::string&	url,
			Receivable	*dest,
			const Header&	hdr ) ;
	
		std::string RedirLocation() const ;
	
		std::string Escape( const std::string& str ) ;
		std::string Unescape( const std::string& str ) ;

	private :
		Header AppendHeader( const Header& hdr ) const ;
		bool CheckRetry( long response ) ;
		long CheckHttpResponse(
			long 				response,
			const std::string&	url,
			const Header&	hdr  ) ;
	
	private :
		OAuth2						m_auth ;
		const std::auto_ptr<Agent>	m_agent ;
} ;
