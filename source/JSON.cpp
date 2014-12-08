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

#include "Json.h"
#include "log.h"
#include "StdioFile.h"

#include <json/json_tokener.h>
#include <json/linkhash.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <utility>

Json::Json( ) :
	m_json( ::json_object_new_object() )
{
	if ( m_json == 0 )
		debugPrintf("json_object_new: cannot create json object\n");
}

Json::Json( const char *str ) :
	m_json( ::json_object_new_string( str ) )
{
	if ( m_json == 0 )
		debugPrintf("json_object_new: cannot create json string %s\n", str);
}

template <>
Json::Json( const std::string& str ) :
	m_json( ::json_object_new_string( str.c_str() ) )
{
	if ( m_json == 0 )
		debugPrintf("json_object_new: cannot create json string %s\n", str.c_str());

	// paranoid check
	if( ::json_object_get_string( m_json ) != str ) debugPrintf("paranoid json check failed\n") ;
}

template <>
Json::Json( const int& l ) :
	m_json( ::json_object_new_int( l ) )
{
	if ( m_json == 0 )
		debugPrintf("cannot create json int\n");
}

template <>
Json::Json( const long& l ) :
	m_json( ::json_object_new_int( static_cast<int>(l) ) )
{
	if ( m_json == 0 )
		debugPrintf("cannot create json int\n");
}

template <>
Json::Json( const unsigned long& l ) :
	m_json( ::json_object_new_int( static_cast<int>(l) ) )
{
	if ( m_json == 0 )
		debugPrintf("cannot create json int\n");
}

template <>
Json::Json( const std::vector<Json>& arr ) :
	m_json( ::json_object_new_array( ) )
{
	if ( m_json == 0 )
		debugPrintf("cannot create json int\n");
	
	for ( std::vector<Json>::const_iterator i = arr.begin() ; i != arr.end() ; ++i )
		Add( *i ) ;
}

template <>
Json::Json( const bool& b ) :
	m_json( ::json_object_new_boolean( b ) )
{
	if ( m_json == 0 )
		debugPrintf("cannot create json bool\n");
}

template <>
Json::Json( const Object& obj ) :
	m_json( ::json_object_new_object() )
{
	if ( m_json == 0 )
		debugPrintf("cannot create json object");

	for ( Object::const_iterator i = obj.begin() ; i != obj.end() ; ++i )
		Add( i->first, i->second ) ;
}

Json::Json( struct json_object *json, NotOwned ) :
	m_json( json )
{
	if( m_json == 0 ) debugPrintf("Json m_json is null\n") ;
}

Json::Json( struct json_object *json ) :
	m_json( json )
{
	if( m_json == 0 ) debugPrintf("JsonObj m_json is null\n");
	::json_object_get( m_json ) ;
}

Json::Json( const Json& rhs ) :
	m_json( rhs.m_json )
{
	if( m_json == 0 ) debugPrintf("Jsonrhs m_json is null\n");
	::json_object_get( m_json ) ;
}

Json::~Json( )
{
	if( m_json == 0 ) debugPrintf("Json destructor m_json is null\n");
	if ( m_json != 0 )
		::json_object_put( m_json ) ;
}

Json& Json::operator=( const Json& rhs )
{
	Json tmp( rhs ) ;
	Swap( tmp ) ;
	return *this ;
}

void Json::Swap( Json& other )
{
	if( m_json == 0 ) debugPrintf("JsonSwap m_json is null\n");
	if( other.m_json == 0 ) debugPrintf("JsonSwap other.m_json is null\n");
	std::swap( m_json, other.m_json ) ;
}

Json Json::operator[]( const std::string& key ) const
{
	if( m_json == 0 ) debugPrintf("Json opkey m_json is null\n");
	
	struct json_object *j = ::json_object_object_get( m_json, key.c_str() ) ;
	if ( j == 0 )
		debugPrintf("Json Array operator key: %s is not found in object\n",key.c_str());
	
	return Json( j ) ;
}

Json Json::operator[]( const std::size_t& idx ) const
{
	if( m_json == 0 ) debugPrintf("Json opidex m_json is null\n");

	struct json_object *j = ::json_object_array_get_idx( m_json, idx ) ;
	if ( j == 0 )
	{
		std::ostringstream ss ;
		ss << "index " << idx << " is not found in array" ;
		debugPrintf("Json Array operator %s\n", ss.str().c_str());
	}
	
	return Json( j ) ;
}

bool Json::Has( const std::string& key ) const
{
	if( m_json == 0 ) debugPrintf("JsonHas m_json is null\n");
	return ::json_object_object_get( m_json, key.c_str() ) != 0 ;
}

bool Json::Get( const std::string& key, Json& json ) const
{
	if( m_json == 0 ) debugPrintf("JsonGet m_json is null\n");
	struct json_object *j = ::json_object_object_get( m_json, key.c_str() ) ;
	if ( j != 0 )
	{
		Json tmp( j ) ;
		json.Swap( tmp ) ;
		return true ;
	}
	else
		return false ;
}

void Json::Add( const std::string& key, const Json& json )
{
	if( m_json == 0 ) debugPrintf("JsonAdd m_json is null\n");
	if( json.m_json == 0 ) debugPrintf("JsonAdd json.m_json is null\n");

	::json_object_get( json.m_json ) ;
	::json_object_object_add( m_json, key.c_str(), json.m_json ) ;
}

void Json::Add( const Json& json )
{
	if( m_json == 0 ) debugPrintf("JsonAdd2 m_json is null\n");
	if( json.m_json == 0 ) debugPrintf("JsonAdd2 json.m_json is null\n");
	
	::json_object_get( json.m_json ) ;
	::json_object_array_add( m_json, json.m_json ) ;
}

bool Json::Bool() const
{
	if( m_json == 0 ) debugPrintf("JsonBool m_json is null\n");
	return ::json_object_get_boolean( m_json ) ;
}

template <>
bool Json::Is<bool>() const
{
	if( m_json == 0 ) debugPrintf("Jsonisbool m_json is null\n");
	return ::json_object_is_type( m_json, json_type_boolean ) ;
}

std::string Json::Str() const
{
	if( m_json == 0 ) debugPrintf("Jsonstr m_json is null\n");
	return ::json_object_get_string( m_json ) ;
}

template <>
bool Json::Is<std::string>() const
{
	if( m_json == 0 ) debugPrintf("Jsonisstr m_json is null\n");
	return ::json_object_is_type( m_json, json_type_string ) ;
}

int Json::Int() const
{
	if( m_json == 0 ) debugPrintf("Jsonint m_json is null\n");
        
	return ::json_object_get_int( m_json ) ;
}

template <>
bool Json::Is<int>() const
{
	if( m_json == 0 ) debugPrintf("Jsonisint m_json is null\n");
	return ::json_object_is_type( m_json, json_type_int ) ;
}

std::ostream& operator<<( std::ostream& os, const Json& json )
{
	if( json.m_json == 0 ) debugPrintf("Jsonstream m_json is null\n");
	return os << ::json_object_to_json_string( json.m_json ) ;
}

void Json::WriteFile( StdioFile& file ) const
{       
	const char *str = ::json_object_to_json_string( m_json ) ;
	file.Write( str, std::strlen(str) ) ;
}

Json::Type Json::DataType() const
{
	if( m_json == 0 ) debugPrintf("Jsontype m_json is null\n");
	return static_cast<Type>( ::json_object_get_type( m_json ) ) ;
}

Json::Object Json::AsObject() const
{
	Object result ;
	
	json_object_object_foreach( m_json, key, val )
	{
		result.insert( Object::value_type( key, Json( val ) ) ) ;
	}
	
	return result ;
}

template <>
bool Json::Is<Json::Object>() const
{
	if( m_json == 0 ) debugPrintf("Jsonisobj m_json is null\n");
	return ::json_object_is_type( m_json, json_type_object ) ;
}

Json::Array Json::AsArray() const
{
	std::size_t count = ::json_object_array_length( m_json ) ;
	Array result ;
	
	for ( std::size_t i = 0 ; i < count ; ++i )
		result.push_back( Json( ::json_object_array_get_idx( m_json, i ) ) ) ;
	
	return result ;
}

template <>
bool Json::Is<Json::Array>() const
{
	if( m_json == 0 ) debugPrintf("JsonArray m_json is null\n");
	return ::json_object_is_type( m_json, json_type_array ) ;
}

Json Json::FindInArray( const std::string& key, const std::string& value ) const
{
	std::size_t count = ::json_object_array_length( m_json ) ;
	
	for ( std::size_t i = 0 ; i < count ; ++i )
	{
		Json item( ::json_object_array_get_idx( m_json, i ) ) ;
		if ( item.Has(key) && item[key].Str() == value )
			return item ;
	}
	debugPrintf("cannot find %s = %s in array\n",key.c_str(),value.c_str());
                ;
        return Json();
}

bool Json::FindInArray( const std::string& key, const std::string& value, Json& result ) const
{
	try
	{
		result = FindInArray( key, value ) ;
		return true ;
	}
	catch ( std::exception& e)
	{
		return false ;
	}
}

Json Json::Parse( const std::string& str )
{
	struct json_object *json = ::json_tokener_parse( str.c_str() ) ;
	if ( json == 0 )
		debugPrintf("json parse error\n");
	
	return Json( json, NotOwned() ) ;
}

Json Json::ParseFile( StdioFile& file )
{
	struct json_tokener *tok = ::json_tokener_new() ;
	
	struct json_object *json = 0 ;
	
	char buf[1024] ;
	std::size_t count = 0 ;

	while ( (count = file.Read( buf, sizeof(buf) ) ) > 0 )
		json = ::json_tokener_parse_ex( tok, buf, count ) ;
	
	if ( json == 0 )
		debugPrintf("Json Parse File Errors %s\n",::json_tokener_errors[tok->err]);
	
	::json_tokener_free( tok ) ;
	
	return Json( json, NotOwned() ) ;
}
