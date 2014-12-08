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

#include "CurlAgent.h"
#include "log.h"

// dependent libraries
#include <ppu-types.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <streambuf>
#include <iostream>

#include <signal.h>

size_t ReadStringCallback(void *ptr, size_t size, size_t nmemb, std::string *data)
{
    size_t count = std::min(size * nmemb, data->size());
    if (count > 0)
    {
        std::memcpy(ptr, &(*data)[0], count);
        data->erase(0, count);
    }

    return count;
}

size_t ReadFileCallback(void *ptr, size_t size, size_t nmemb, StdioFile *file)
{
	if( ptr == 0 ) debugPrintf("ReadFileCallback: ptr is null\n") ;
	if( file == 0 ) debugPrintf("ReadFileCallback: file ptr is null\n") ;;

	u64 count = std::min(
		static_cast<u64>(size * nmemb),
		static_cast<u64>(file->Size() - file->Tell()) ) ;
	if( count > std::numeric_limits<size_t>::max() ) debugPrintf("ReadFileCallback: numeric limit issue?\n") ; ;
	
	if ( count > 0 )
		file->Read( ptr, static_cast<size_t>(count) ) ;
	
	return count ;
}

/*int debug_cb ( CURL *handle, curl_infotype type, char *data, size_t size, void *userp) 
{
    switch ( type ) 
    {
        case CURLINFO_TEXT: {
                debugPrintf("INFO: %s\n", data);
                break;
        }
        case CURLINFO_HEADER_IN: {
                debugPrintf("RESPONSE: %s", data);
                break;
        }
        case CURLINFO_HEADER_OUT: { //There is no null-terminator on this one !
                size_t i;
                debugPrintf("REQUEST: \n");
                for ( i = 0; i < size; i ++) debugPrintf("%c", data[i]);
                break;
        }
        case CURLINFO_DATA_IN: {
                debugPrintf("RECIEVED: %d bytes\n", size);
                break;
        }
        case CURLINFO_DATA_OUT: { 
                debugPrintf("TRANSMIT: %d bytes\n", size);
                break;
        }
        case CURLINFO_SSL_DATA_IN: {
                debugPrintf("SSL RECIEVED: %d bytes\n", size);
                break;
        }
        case CURLINFO_SSL_DATA_OUT: { 
                debugPrintf("SSL TRANSMIT: %d bytes\n", size);
                break;
        }
        case CURLINFO_END: { 
                debugPrintf("This should never happen!");
                break;
        }
        default: 
            return 0;
    }
    return 0;
}*/

struct CurlAgent::Impl
{
    CURL *curl;
    std::string location;
};

CurlAgent::CurlAgent() :
m_pimpl(new Impl)
{
    m_pimpl->curl = ::curl_easy_init();
}

void CurlAgent::Init()
{
    ::curl_easy_reset(m_pimpl->curl);
    ::curl_easy_setopt(m_pimpl->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    ::curl_easy_setopt(m_pimpl->curl, CURLOPT_SSL_VERIFYHOST, 2L);
    ::curl_easy_setopt(m_pimpl->curl, CURLOPT_CAINFO, "/dev_hdd0/game/PSCD00001/USRDIR/cert.cer");
    ::curl_easy_setopt(m_pimpl->curl, CURLOPT_HEADERFUNCTION, &CurlAgent::HeaderCallback);
    ::curl_easy_setopt(m_pimpl->curl, CURLOPT_WRITEHEADER, this);
    ::curl_easy_setopt(m_pimpl->curl, CURLOPT_HEADER, 0L);
}

CurlAgent::~CurlAgent()
{
    ::curl_easy_cleanup(m_pimpl->curl);
}

std::size_t CurlAgent::HeaderCallback(void *ptr, size_t size, size_t nmemb, CurlAgent *pthis)
{
    char *str = reinterpret_cast<char*> (ptr);
    std::string line(str, str + size * nmemb);

    static const std::string loc = "Location: ";
    std::size_t pos = line.find(loc);
    if (pos != line.npos)
    {
        std::size_t end_pos = line.find("\r\n", pos);
        pthis->m_pimpl->location = line.substr(loc.size(), end_pos - loc.size());
    }

    return size*nmemb;
}

std::size_t CurlAgent::Receive(void* ptr, size_t size, size_t nmemb, Receivable *recv)
{
    if(recv == 0) debugPrintf("Receive: no data received\n") ;
    return recv->OnData(ptr, size * nmemb);
}

long CurlAgent::ExecCurl(
                         const std::string& url,
                         Receivable *dest,
                         const Header& hdr)
{
    CURL *curl = m_pimpl->curl;

    char error[CURL_ERROR_SIZE] = {};
    ::curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
    ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlAgent::Receive);
    ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, dest);
    //::curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);
    ::curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
    ::curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip,deflate");
    ::curl_easy_setopt(curl, CURLOPT_USERAGENT, "PS3 Cloud Drive (gzip)");
    ::curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST, "TLS-RSA-WITH-AES-128-GCM-SHA256");
    //::curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    //::curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_cb);

    SetHeader(hdr);

    dest->Clear();
    CURLcode curl_code = ::curl_easy_perform(curl);

    // get the HTTTP response code
    long http_code = 0;
    ::curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    debugPrintf("HTTP response %d\n", http_code ) ;

    // reset the curl buffer to prevent it from touch our "error" buffer
    ::curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, 0);

    // only throw for libcurl errors
    if (curl_code != CURLE_OK)
    {
        debugPrintf("ExecCurl %d,%s\n",curl_code,url.c_str(),error);
    }

    return http_code;
}

long CurlAgent::Put(
                    const std::string& url,
                    const std::string& data,
                    Receivable *dest,
                    const Header& hdr)
{
    debugPrintf("HTTP PUT %s\n", url.c_str() ) ;

    Init();
    CURL *curl = m_pimpl->curl;

    std::string put_data = data;

    // set common options
    ::curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    ::curl_easy_setopt(curl, CURLOPT_READFUNCTION, &ReadStringCallback);
    ::curl_easy_setopt(curl, CURLOPT_READDATA, &put_data);
    ::curl_easy_setopt(curl, CURLOPT_INFILESIZE, put_data.size());

    return ExecCurl(url, dest, hdr);
}

long CurlAgent::Put(
                    const std::string& url,
                    StdioFile& file,
                    Receivable *dest,
                    const Header& hdr)
{
	debugPrintf("HTTP PUT %s\n", url.c_str() ) ;
	
	Init() ;
	CURL *curl = m_pimpl->curl ;

	// set common options
	::curl_easy_setopt(curl, CURLOPT_UPLOAD,			1L ) ;
	::curl_easy_setopt(curl, CURLOPT_READFUNCTION,		&ReadFileCallback ) ;
	::curl_easy_setopt(curl, CURLOPT_READDATA ,			&file ) ;
	::curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, 	static_cast<curl_off_t>(file.Size()) ) ;
	
	return ExecCurl( url, dest, hdr ) ;
}

long CurlAgent::customPut(
                    const std::string& url,
                    StdioFile& file,
                    Receivable *dest,
                    curl_progress_callback func,
                    const Header& hdr)
{
	debugPrintf("HTTP PUT %s\n", url.c_str() ) ;
	
	Init() ;
	CURL *curl = m_pimpl->curl ;

	// set common options
	::curl_easy_setopt(curl, CURLOPT_UPLOAD,			1L ) ;
	::curl_easy_setopt(curl, CURLOPT_READFUNCTION,		&ReadFileCallback ) ;
	::curl_easy_setopt(curl, CURLOPT_READDATA ,			&file ) ;
	::curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, 	static_cast<curl_off_t>(file.Size()) ) ;
	::curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        ::curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, func);
	::curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, func);
                
	return ExecCurl( url, dest, hdr ) ;
}

long CurlAgent::Get(
                    const std::string& url,
                    Receivable *dest,
                    const Header& hdr)
{
    debugPrintf("HTTP GET %s\n", url.c_str() ) ;
    Init();

    // set get specific options
    ::curl_easy_setopt(m_pimpl->curl, CURLOPT_HTTPGET, 1L);

    return ExecCurl(url, dest, hdr);
}

long CurlAgent::customGet(
                    const std::string& url,
                    Receivable *dest,
                    curl_progress_callback func,
                    const Header& hdr)
{
    debugPrintf("HTTP GET %s\n", url.c_str() ) ;
    Init();

    // set get specific options
    CURL *curl = m_pimpl->curl ;
    ::curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    ::curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    ::curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, func);
    ::curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, func);

    return ExecCurl(url, dest, hdr);
}

long CurlAgent::Post(
                     const std::string& url,
                     const std::string& data,
                     Receivable *dest,
                     const Header& hdr)
{
    debugPrintf("HTTP POST %s with %s\n", url.c_str(), data.c_str() ) ;

    Init();
    CURL *curl = m_pimpl->curl;

    // make a copy because the parameter is const
    std::string post_data = data;

    // set post specific options
    ::curl_easy_setopt(curl, CURLOPT_POST, 1L);
    ::curl_easy_setopt(curl, CURLOPT_POSTFIELDS, &post_data[0]);
    ::curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data.size());

    return ExecCurl(url, dest, hdr);
}

long CurlAgent::Custom(
                       const std::string& method,
                       const std::string& url,
                       Receivable *dest,
                       const Header& hdr)
{
    debugPrintf("HTTP %s. %s\n", url.c_str(), method.c_str() ) ;

    CURL *curl = m_pimpl->curl;

    ::curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    // 	::curl_easy_setopt(curl, CURLOPT_VERBOSE,		1 );

    return ExecCurl(url, dest, hdr);
}

void CurlAgent::SetHeader(const Header& hdr)
{
    // set headers
    struct curl_slist *curl_hdr = 0;
    for (Header::iterator i = hdr.begin(); i != hdr.end(); ++i)
    {
        //debugPrintf("Set Header %s\n",i->c_str());
        curl_hdr = curl_slist_append(curl_hdr, i->c_str());
    }

    ::curl_easy_setopt(m_pimpl->curl, CURLOPT_HTTPHEADER, curl_hdr);
}

std::string CurlAgent::RedirLocation() const
{
    //debugPrintf("Returning location %s\n",m_pimpl->location.c_str());
    return m_pimpl->location;
}

std::string CurlAgent::CheckSSLSupport() const
{
    curl_version_info_data * vinfo = curl_version_info(CURLVERSION_NOW);
    if (vinfo->features & CURL_VERSION_SSL)
        return "SSL is supported";
    else
        return "Aww man, no Sweet Socks Support";
}

std::string CurlAgent::Escape(const std::string& str)
{
    CURL *curl = m_pimpl->curl;

    char *tmp = curl_easy_escape(curl, str.c_str(), str.size());
    std::string result = tmp;
    curl_free(tmp);

    return result;
}

std::string CurlAgent::Unescape(const std::string& str)
{
    CURL *curl = m_pimpl->curl;

    int r;
    char *tmp = curl_easy_unescape(curl, str.c_str(), str.size(), &r);
    std::string result = tmp;
    curl_free(tmp);

    return result;
}
