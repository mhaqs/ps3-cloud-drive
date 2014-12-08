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

#include "AuthAgent.h"
#include "Header.h"

#include <cassert>
#include <iostream>

AuthAgent::AuthAgent( const OAuth2& auth, std::auto_ptr<Agent> real_agent ) :
	m_auth	( auth ),
	m_agent	( real_agent )
{
	assert( m_agent.get() != 0 ) ;
}

Header AuthAgent::AppendHeader( const Header& hdr ) const
{
	Header h(hdr) ;
	h.Add( "Authorization: Bearer " + m_auth.AccessToken() ) ;
	h.Add( "GData-Version: 3.0" ) ;
	return h ;
}

long AuthAgent::Put(
	const std::string&	url,
	const std::string&	data,
	Receivable			*dest,
	const Header&		hdr )
{
	Header auth = AppendHeader(hdr) ;

	long response ;
	while ( CheckRetry(
		response = m_agent->Put(url, data, dest, auth) ) ) ;
	
	return CheckHttpResponse(response, url, auth) ;
}

long AuthAgent::Put(
	const std::string&	url,
	StdioFile&			file,
	Receivable			*dest,
	const Header&		hdr )
{
	Header auth = AppendHeader(hdr) ;
	
	long response ;
	while ( CheckRetry(
		response = m_agent->Put( url, file, dest, AppendHeader(hdr) ) ) ) ;
	
	return CheckHttpResponse(response, url, auth) ;
}

long AuthAgent::Get(
	const std::string& 	url,
	Receivable			*dest,
	const Header&		hdr )
{
	Header auth = AppendHeader(hdr) ;

	long response ;
	while ( CheckRetry(
		response = m_agent->Get( url, dest, AppendHeader(hdr) ) ) ) ;
	
	return CheckHttpResponse(response, url, auth) ;
}

long AuthAgent::Post(
	const std::string& 	url,
	const std::string&	data,
	Receivable			*dest,
	const Header&		hdr )
{
	Header auth = AppendHeader(hdr) ;
	
	long response ;
	while ( CheckRetry(
		response = m_agent->Post( url, data, dest, AppendHeader(hdr) ) ) ) ;
	
	return CheckHttpResponse(response, url, auth) ;
}

long AuthAgent::Custom(
	const std::string&	method,
	const std::string&	url,
	Receivable			*dest,
	const Header&		hdr )
{
	Header auth = AppendHeader(hdr) ;

	long response ;
	while ( CheckRetry(
		response = m_agent->Custom( method, url, dest, AppendHeader(hdr) ) ) ) ;
	
	return CheckHttpResponse(response, url, auth) ;
}

std::string AuthAgent::RedirLocation() const
{
	return m_agent->RedirLocation() ;
}

std::string AuthAgent::Escape( const std::string& str )
{
	return m_agent->Escape( str ) ;
}

std::string AuthAgent::Unescape( const std::string& str )
{
	return m_agent->Unescape( str ) ;
}

bool AuthAgent::CheckRetry( long response )
{
	// HTTP 500 and 503 should be temperory. just wait a bit and retry
	if ( response == 500 || response == 503 )
	{
		std::cout<<"request failed due to temporary error: %1%. retrying in 5 seconds"<<response;
			
		//os::Sleep( 5 ) ;
		return true ;
	}
	
	// HTTP 401 Unauthorized. the auth token has been expired. refresh it
	else if ( response == 401 )
	{
		std::cout<<"request failed due to auth token expired: %1%. refreshing token"<<response;
			
		m_auth.Refresh() ;
		return true ;
	}
	else
		return false ;
}

long AuthAgent::CheckHttpResponse(
		long 				response,
		const std::string&	url,
		const Header&	hdr  )
{
	// throw for other HTTP errors
	if ( response >= 400 && response < 500 )
        {
            std::cout<<"HTTP Server Error...";
        }
	return response ;
}
