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

#include <iostream>
#include <sstream>
#include <string.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>

#include <lv2/syscalls.h>
#include <sys/systime.h>
#include <sys/file.h>
#include <io/pad.h>

#include <sysutil/msg.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <sysutil/save.h>

#include <net/net.h>
#include <net/netctl.h>
#include <netinet/in.h>
#include <algorithm>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_thread.h>

#include <polarssl/md5.h>

#include "OAuth2.h"
#include "CurlAgent.h"
#include "JsonResponse.h"
#include "Download.h"
#include "log.h"

const std::string jsonMime = "application/json";
const std::string folderMime = "application/vnd.google-apps.folder";
const std::string octetMime = "application/octet-stream";
const std::string appPath = "/dev_hdd0/game/PSCD00001/USRDIR/";
const std::string appFile = "/dev_hdd0/game/PSCD00001/USRDIR/ps3clouddrive.json";
const std::string appFileBackup = "/dev_hdd0/game/PSCD00001/USRDIR/ps3clouddrive.backup";
const std::string localResourceFile = "/dev_hdd0/game/PSCD00001/USRDIR/local.json";
const std::string remoteResourceFile = "/dev_hdd0/game/PSCD00001/USRDIR/remote.json";
const std::string remoteBackup = "/dev_hdd0/game/PSCD00001/USRDIR/remote.backup";
const std::string queryFields = "items(fileSize,id,md5Checksum,mimeType,modifiedDate,parents/id,title)";
const std::string app_title = "ps3clouddrive";
const std::string client_id = "google_drive_app_client_id";
const std::string client_secret = "google_drive_app_client_secret";

OAuth2 authToken(client_id, client_secret);

std::string rootFolderId = "root";
std::string deviceId     = "nill";
std::string remoteJsonId = "root";
std::string last_sync = "0";

Json localResourceRoot;
Json remoteResourceRoot;

SDL_Event event;
SDL_Surface* screen = NULL;
SDL_Surface *image = NULL;
TTF_Font *Sans;
SDL_Color GREY = { 57, 57, 57};

msgType dialog_type;
static int dialog_action = 0;

int exitapp = 0;
int xmbopen = 0;
int syncing = 0;

double progFuncLastProgress = 0.0;

int appWidth = 1280;
int appHeight = 720;

std::string userType = "one";

CurlAgent http;

typedef struct {
	uint32_t total;
	uint32_t avail;
} _meminfo;
_meminfo meminfo;

void get_free_memory()
{
	lv2syscall1(SYSCALL_MEMORY_GET_USER_MEMORY_SIZE, (uint64_t) &meminfo);
}

bool pathExists(std::string pathToCheck)
{
    sysFSStat entry;
    return sysFsStat(pathToCheck.c_str(), &entry) == 0;
}

void sysevent_callback(u64 status, u64 param, void * userdata)
{
    switch(status)
    {
        case SYSUTIL_EXIT_GAME: exitapp = 1;	break;
        case SYSUTIL_MENU_OPEN: xmbopen = 1;	break;
        case SYSUTIL_MENU_CLOSE: xmbopen = 0;	break;
    }
}

static void dialog_handler(msgButton button, void *usrData)
{
    switch (button)
    {
        case MSG_DIALOG_BTN_OK:
                dialog_action = 1;
                break;
        case MSG_DIALOG_BTN_NO:
        case MSG_DIALOG_BTN_ESCAPE:
                dialog_action = 2;
                break;
        case MSG_DIALOG_BTN_NONE:
                dialog_action = -1;
                break;
        default:
                break;
    }
}

SDL_Surface *Load_Image(std::string filePath)
{
	SDL_Surface* loadedImage = NULL;
	SDL_Surface* optimizedImage = NULL;
	loadedImage = IMG_Load(filePath.c_str());
	if(loadedImage!=NULL){
		optimizedImage = SDL_DisplayFormatAlpha(loadedImage);
		SDL_FreeSurface(loadedImage);
	}
	return optimizedImage;
}

void draw_surface(SDL_Surface* destination, SDL_Surface* source, int x, int y)
{
	SDL_Rect offset;
	offset.x = x;
	offset.y = y;
	SDL_BlitSurface( source, NULL, destination, &offset );
}

bool CheckRetry( long response )
{
	// HTTP 500 and 503 should be temperory. just wait a bit and retry
	if ( response == 500 || response == 503 )
	{
		debugPrintf("Request failed due to temporary error: %d, retrying in 5 seconds\n",response);
			
		sysUsleep(5);
		return true ;
	}
	
	// HTTP 401 Unauthorized. the auth token has been expired. refresh it
	else if ( response == 401 )
	{
		debugPrintf("Request failed due to Auth token expired: %d. refreshing token\n",response);
			
		authToken.Refresh() ;
		return true ;
	}
	else
        {
            debugPrintf("All is fine with GDrive, continue... %l\n",response);
            return false ;
        }
}

/**
 * Retrieves a Resource tree of the resources under a folder in google drive
 * @param parentId the parentid for whomm the resources are to be listed
 * @return the resource array containing all children
 */
Json::Array getResourcesUnderFolder(std::string parentId)
{
    long ret;
    Json::Array parsedArray;
    
    Header hdr;
    hdr.Add("Authorization: Bearer " + authToken.AccessToken());
    
    std::string query=parentId+"' in parents"; 
    JsonResponse resp;

    while( CheckRetry( ret = http.Get("https://www.googleapis.com/drive/v2/files?q="+query, &resp, hdr)));
    
    if ( ret >= 400 && ret < 500 )
    {
        debugPrintf("getResourcesUnderFolder: HTTP Server Error...%l\n", ret);
        // This could mean the internet disconnected, or the server crashed or whatever
        // Possibly exit the application?
    }
    else
    {
        Json parsedResp = resp.Response();
        parsedArray = parsedResp["items"].AsArray();
    }
    return parsedArray;
}

/**
 * The function checks if a resource exists on google drive and returns the item
 * @param resourceTitle the title of the resource to find
 * @param mimeType the mimetype of the resource
 * @param parentId the parentid of the resource's parent (if you don't know this, this function is useless, unless you're searching under root)
 * @return the resource object, otherwise null
 */
Json checkIfRemoteResourceExists(std::string resourceTitle,std::string mimeType,std::string parentId)
{
    long ret;
    Json resourceObject;
    
    Header hdr;
    hdr.Add("Authorization: Bearer " + authToken.AccessToken());
    
    std::string query="mimeType='"+mimeType+"' and trashed=false and title='"+resourceTitle+"' and '"+parentId+"' in parents"; 
    JsonResponse resp;
    
    std::string url = "https://www.googleapis.com/drive/v2/files?q=" + http.Escape(query) + "&fields=" + http.Escape(queryFields);
    
    while( CheckRetry( ret = http.Get(url, &resp, hdr)));
    
    if ( ret >= 400 && ret < 500 )
    {
        debugPrintf("checkIfRemoteResourceExists: HTTP Server Error...%l\n", ret);
        // This could mean the internet disconnected, or the server crashed or whatever
        // Possibly exit the application?
    }
    else
    {
        Json parsedResp = resp.Response();
        Json::Array parsedArray = parsedResp["items"].AsArray();
        if(parsedArray.size() > 0)
        {
                resourceObject = parsedArray.at(0);
        }
    }
    
    return resourceObject;
}

/**
 * Stores the current application configuration in the application config json
 */
void storeConfig()
{
    StdioFile file(appFile, SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, 0644);
    StdioFile backup(appFileBackup, SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, 0644);
    
    Json appConfig;
    appConfig.Add("refresh_token",Json(authToken.RefreshToken()));
    appConfig.Add("device_id",Json(authToken.DeviceCode()));
    appConfig.Add("root_id",Json(rootFolderId));
    appConfig.Add("last_sync",Json(last_sync));
    appConfig.Add("remote_fid", Json(remoteJsonId));
    
    appConfig.WriteFile(file);
    appConfig.WriteFile(backup);
    
    file.Close();
    backup.Close();
    
    sysFsChmod(appFile.c_str(),0644);
    sysFsChmod(appFileBackup.c_str(),0644);
}

/**
 * Utility function that reads the config file for refresh token etc
 * @return "not_found" if file not found or corrupt else the refresh_token
 */
std::string isUserAuthenticated()
{
    std::string result = "not_found";
    Json config;
    StdioFile file(appFile);
    
    if(file.Exists())
    {
        debugPrintf("Config file found, parsing...\n");
        
        config = Json::ParseFile(file);

        if(!config.AsObject().empty())
        {
                debugPrintf("Config file not empty, returning refresh token...\n");
                result = config["refresh_token"].Str();
                deviceId = config.Has("device_id") ? config["device_id"].Str() : "nill";
                rootFolderId = config.Has("root_id") ? config["root_id"].Str() : "root";
                last_sync = config.Has("last_sync") ? config["last_sync"].Str() : "0";
                remoteJsonId = config.Has("remote_fid") ? config["remote_fid"].Str() : "root";
        }
    }
    else
    {
        debugPrintf("Config file not found, returning empty handed...\n");
    }
    
    return result;
}

/**
 * Utility function to create meta data when a resource needs to be created on google drive
 * @param title the title of the resource
 * @param mimeType the mimetype. folder, octet etc
 * @param parentid the parentid of the resource, default is root
 * @return the json metadata in string notation
 */
std::string constructMetaData(std::string title, std::string mimeType, std::string parentid)
{
    Json parent;
    parent.Add("id", Json(parentid));

    std::vector<Json> array;
    array.push_back(parent);

    Json meta;
    meta.Add("title", Json(title));
    meta.Add("parents", Json(array));
    meta.Add("mimeType", Json(mimeType));

    return meta.Str();
}

int transferProgress(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    if(dltotal > 0.0)
    {
        double curProg = (dlnow / dltotal) * 100;
        if(curProg - progFuncLastProgress > 1)
        {
            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX1, curProg - progFuncLastProgress);
            
            debugPrintf("DOWN: %f of %f P: %f C: %f\r\n", dlnow, dltotal, progFuncLastProgress,curProg);
            
            progFuncLastProgress = curProg;
        }
    }
    
    if(ultotal > 0.0)
    {
        double curProg = (ulnow / ultotal) * 100;
        if(curProg - progFuncLastProgress > 1)
        {
            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX1, curProg - progFuncLastProgress);
            
            debugPrintf("UP: %f of %f P: %f C: %f\r\n", ulnow, ultotal, progFuncLastProgress,curProg);
                    
            progFuncLastProgress = curProg;
        }
    }
    
    return 0;
}

/**
 * Creates a directory locally
 * @param path the path of the directory to create
 */
void downloadDirectory(std::string path)
{    
    sysFsMkdir(path.c_str(),0755);
}

/**
 * Downloads a file from Google drive to local folder
 * @param localPath the local path to save the file to
 * @param fileId the id of the file in google drive to download
 */
bool downloadFile(std::string localPath, std::string fileId)
{
    long ret;
    Download file(localPath);
    
    Header hdr;
    hdr.Add("Authorization: Bearer " + authToken.AccessToken());

    JsonResponse resp;

    while( CheckRetry( ret = http.Get("https://www.googleapis.com/drive/v2/files/"+fileId + "?fields=downloadUrl" , &resp, hdr)));
    
    if ( ret >= 400 && ret < 500 )
    {
        debugPrintf("downloadFile: Could not get downloadUrl...%l\n", ret);
        // This could mean the internet disconnected, or the server crashed or whatever
        // Possibly exit the application?
        return false;
    }

    // the content upload URL is in the "Location" HTTP header
    Json fileRes = resp.Response();
    std::string downloadLink;
    
    if(fileRes.Has("downloadUrl"))
    {
        downloadLink = fileRes["downloadUrl"].Str();
    }
    else
    {
        return false;
    }
    resp.Clear();

    debugPrintf("Downlink is %s\n", downloadLink.c_str());

    progFuncLastProgress = 0.0;
    
    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX1);
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX1, localPath.c_str());
    
    while( CheckRetry( ret = http.customGet(downloadLink, &file, transferProgress, hdr)));
    
    debugPrintf("Return Code for file download is %d\n", ret);
    
    if ( ret >= 400 && ret < 500 )
    {
        debugPrintf("downloadFile: Could not download File..%l\n", ret);
        // This could mean the internet disconnected, or the server crashed or whatever
        // Possibly exit the application?
        return false;
    }
    
    return true;
}

void downloadChanges()
{
    u32 remSize = remoteResourceRoot["data"].AsArray().size();
    u32 i=0;
    u32 progressCounter =0;
    u32 totalProgress = 0;
    u32 prevProgress = 0;
    //u32 globProgress = 0;
            
    std::string result;
    
    for(i=0; i < remSize; i++)
    {
        Json resource = remoteResourceRoot["data"][i];
        if(resource["status"].Str() == "synced")
        {
            sysFSStat entry;
                
            if(sysFsStat(resource["path"].Str().c_str(), &entry) != 0)
            {
                totalProgress++;
            }
        }
    }
       
    debugPrintf("downloadChanges Step 1: Creating directories first on HDD\n");
    
    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX0);
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, "Downloading Parent Entries...");
    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, 0);
    
    for(i=0; i < remSize; i++)
    {
        if(dialog_action != 0) break;
        
        Json resource = remoteResourceRoot["data"][i];
        
        if(resource["status"].Str() == "synced")
        {
            if(resource["mimeType"].Str() == folderMime)
            {
                sysFSStat entry;
                result = resource["path"].Str();
                
                if(sysFsStat(result.c_str(), &entry) != 0)
                {
                    progressCounter++;
                    debugPrintf("downloadChanges: Creating Directory with path %s\n", result.c_str());
                    
                    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX1);
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX1, 0);
                    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX1, resource["path"].Str().c_str());

                    downloadDirectory(result);
                    
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX1,100);
                    
                    std::stringstream ss;
                    ss<<"Downloading Parent Entries: "<<progressCounter<<" of "<<totalProgress;
                        
                    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0,ss.str().c_str());
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    
                    /*if(userType == "three")
                    {
                        msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    }
                    if(userType == "two")
                    {   
                        msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 40) / totalProgress) - globProgress);                      
                    }
                    globProgress = (progressCounter * 40) / totalProgress;*/
                    prevProgress = (progressCounter * 100) / totalProgress;
                }
            }
        }
    }
 
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, "Downloading Child Entries...");
    
    debugPrintf("downloadChanges Step 2: Creating files on HDD\n");
        
    for(i=0; i < remSize; i++)
    {
        if(dialog_action != 0) break;
        
        Json resource = remoteResourceRoot["data"][i];
        
        if(resource["status"].Str() == "synced")
        {
            if(resource["mimeType"].Str() != folderMime)
            {
                sysFSStat entry;
                result = resource["path"].Str();
             
                if(sysFsStat(result.c_str(), &entry) != 0)
                {
                    progressCounter++;
                    debugPrintf("downloadChanges: Creating file with path %s\n", result.c_str() );
                    
                    if(!downloadFile(result,resource["id"].Str()))
                    {
                        continue;
                    }

                    std::stringstream ss;
                    ss<<"Downloading Child Entries: "<<progressCounter<<" of "<<totalProgress;
                        
                    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0,ss.str().c_str());
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    
                    /*if(userType == "three")
                    {
                        msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    }
                    if(userType == "two")
                    {   
                        msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 40) / totalProgress) - globProgress);                      
                    }
                    globProgress = (progressCounter * 40) / totalProgress;*/
                    prevProgress = (progressCounter * 100) / totalProgress;
                }
            }
        }
    }
      
    debugPrintf("downloadChanges finished, All synced files Downloaded\n");
}

/**
 * Uploads a file to Google Drive
 * @param path the local file path
 * @param filename the name of the file with extension
 * @param mimeType the mimetype of the file, default is octet
 * @param parentId the parent id in drive of the file
 * @return the created resource in Json format
 */
Json uploadFile(std::string path, std::string filename, std::string mimeType, std::string parentId, std::string fileId="")
{
    long ret;
    StdioFile file(path);
    Json obj;
    sysFSStat entry;
    
    if(sysFsStat(path.c_str(), &entry) != 0)
    {
        debugPrintf("uploadFile: Could not open file, another ps3?\n");
        return obj;
    }

    std::ostringstream xcontent_len;
    xcontent_len << "X-Upload-Content-Length: " << file.Size();

    Header hdr;
    hdr.Add("Authorization: Bearer " + authToken.AccessToken());
    hdr.Add("Content-Type: application/json");
    hdr.Add("X-Upload-Content-Type: " + mimeType);
    hdr.Add(xcontent_len.str());
    //hdr.Add( "If-Match: " + m_etag ) ;
    hdr.Add( "Expect:" ) ;

    std::string meta = constructMetaData(filename, mimeType, parentId);

    JsonResponse resp;
    
    std::string postUrl;
    
    if(fileId == "")
    {
        postUrl = "https://www.googleapis.com/upload/drive/v2/files?uploadType=resumable&fields=" + http.Escape("fileSize,id,md5Checksum,modifiedDate,parents/id");
        while( CheckRetry( ret = http.Post(postUrl, meta, &resp, hdr)));
    }
    else
    {
        postUrl = "https://www.googleapis.com/upload/drive/v2/files/" + fileId+ "?uploadType=resumable&fields=" + http.Escape("fileSize,id,md5Checksum,modifiedDate,parents/id");
        while( CheckRetry( ret = http.Put(postUrl, meta, &resp, hdr)));
    }
    
    if ( ret >= 400 && ret < 500 )
    {
        debugPrintf("uploadFile: Could not pose meta data..%l\n", ret);
        // This could mean the internet disconnected, or the server crashed or whatever
        // Possibly exit the application?
        return obj;
    }

    std::ostringstream content_len;
    content_len << "Content-Length: " << file.Size();
    
    Header uphdr;
    uphdr.Add("Authorization: Bearer " + authToken.AccessToken());
    uphdr.Add("Content-Type: " + mimeType);
    uphdr.Add(content_len.str());
    uphdr.Add("Expect:" );
    uphdr.Add("Accept:" );
    uphdr.Add("Accept-Encoding: gzip");

    // the content upload URL is in the "Location" HTTP header
    JsonResponse resp2;

    progFuncLastProgress = 0.0;
    
    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX1);
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX1, path.c_str());

    while( CheckRetry( ret = http.customPut(http.RedirLocation(), file, &resp2,transferProgress, uphdr)));
    
    if ( ret >= 400 && ret < 500 )
    {
        debugPrintf("uploadFile: Could not upload file..%l\n", ret);
        // This could mean the internet disconnected, or the server crashed or whatever
        // Possibly exit the application?
        return obj;
    }

    obj = resp2.Response();
    if (obj.Has("id"))
    {
        debugPrintf("Resource id is %s\n", obj["id"].Str().c_str());
    }
    else
    {
        debugPrintf("Resource id not found");
    }
    
    return obj;
}

/**
 * Creates a directory on the google drive
 * @param dirName the name to give the directory
 * @param parentId the parentid of this directory, default is root
 * @return the created folder resource in Json format
 */
Json uploadDirectory(std::string dirName, std::string parentId)
{
    long ret = -1;
    Json obj;

    //Step 1 - Creating SaveData Directory Online
    Header hdr;
    hdr.Add("Authorization: Bearer " + authToken.AccessToken());
    hdr.Add("Content-Type: application/json");
    hdr.Add("Expect:" );

    std::string meta = constructMetaData(dirName, folderMime, parentId);

    JsonResponse resp;

    debugPrintf("Attempting folder creation on GDrive...\n");
        
    while( CheckRetry( ret = http.Post("https://www.googleapis.com/drive/v2/files?fields=" + http.Escape("id,modifiedDate,parents/id"), meta, &resp, hdr)));
    
    if ( ret >= 400 && ret < 500 )
    {
        debugPrintf("uploadDirectory: Could not upload directory..%l\n", ret);
        // This could mean the internet disconnected, or the server crashed or whatever
        // Possibly exit the application?
        return obj;
    }
    
    obj = resp.Response();
    if (obj.Has("id"))
    {
        debugPrintf("Folder id is %d, %s\n", ret, obj["id"].Str().c_str());
    }
    else
    {
        debugPrintf("Folder creation failed with status %d\n", ret);
    }

    return obj;
}

std::string getParentIdByFolderName(std::string folderName)
{
    std::string result = "not_found";
    if(folderName == app_title)
    {
        result = rootFolderId;
    }
    else
    {
        int remSize = remoteResourceRoot["data"].AsArray().size();
        for(int i=0; i < remSize; i++)
        {
            Json resource = remoteResourceRoot["data"][i];
            if(resource["mimeType"].Str() == folderMime)
            {
                if(resource["title"].Str() == folderName)
                {
                    result = resource["id"].Str();
                }
            }
        }
    }
    
    debugPrintf("getParentIdByFolderName: Returning Parent id %s for folder %s\n", result.c_str(), folderName.c_str());
    
    return result;
}

void writeChangesToFile()
{
    //Backup remote.json before a write to save from any catastrophies
    StdioFile backup(remoteBackup, SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, 0644);
    remoteResourceRoot.WriteFile(backup);
    backup.Close();
    sysFsChmod(remoteBackup.c_str(),0644);
    
    StdioFile file(remoteResourceFile, SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, 0644);
    remoteResourceRoot.WriteFile(file);
    file.Close();
    sysFsChmod(remoteResourceFile.c_str(),0644);
    
    debugPrintf("writeChangesToFile: Changes to remote.json written successfully\n");
}

/**
 * Uploads everything marked in remote resource Tree 
 */
void uploadChanges()
{
    // First make sure that all folders are uploaded, we need this because file
    // resources in Google Drive don't have paths, instead they rely on
    // parent ids
    u32 remSize = remoteResourceRoot["data"].AsArray().size();
    u32 i=0;
    u32 syncJson = 0;
    u32 progressCounter =0;
    u32 totalProgress = 0;
    u32 prevProgress = 0;
    //u32 globProgress = 0;
    std::string result;
    
    for(i=0; i < remSize; i++)
    {
        Json resource = remoteResourceRoot["data"][i];
        if(resource["status"].Str() == "upload" || resource["status"].Str() == "update")
        {
            totalProgress++;
        }
    }
    
    debugPrintf("uploadChanges Step 1: Creating directories first on Google Drive,%d\n",totalProgress);
    
    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX0);

    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, "Uploading Parent Entries...");
    
    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, 0);
                    
    for(i=0; i < remSize; i++)
    {
        if(dialog_action != 0)
        {
            syncJson = 0;
            break;
        }
        
        Json resource = remoteResourceRoot["data"][i];
        
        if(resource["status"].Str() == "upload")
        {
            if(resource["mimeType"].Str() == folderMime)
            {
                result = getParentIdByFolderName(resource["parentFolder"].Str());
                
                if(result != "not_found")
                {
                    debugPrintf("uploadChanges: Creating Directory with name %s under %s\n",resource["title"].Str().c_str(), result.c_str() );
                    
                    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX1);
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX1, 0);
                    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX1, resource["path"].Str().c_str());

                    Json obj = uploadDirectory(resource["title"].Str(),result);
                                        
                    if(!obj.Has("id"))
                    {
                        continue;
                    }
                    
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX1, 100);
                    
                    remoteResourceRoot["data"][i].Add("id",Json(obj["id"].Str()));
                    remoteResourceRoot["data"][i].Add("parentid",Json(result));
                    remoteResourceRoot["data"][i].Add("modifiedDate",Json(obj["modifiedDate"].Str()));
                    remoteResourceRoot["data"][i].Add("status", Json("synced"));
                    writeChangesToFile();
                    
                    syncJson = 1;
                    progressCounter++;
                    
                    std::stringstream ss;
                    ss<<"Uploading Parent Entries: "<<progressCounter<<" of "<<totalProgress;
                        
                    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0,ss.str().c_str());
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                                       
                    /*if(userType == "one")
                    {
                            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    }
                    if(userType == "two")
                    {
                            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 40) / totalProgress) - globProgress);
                    }
                    globProgress = (progressCounter * 40) / totalProgress;*/
                    prevProgress = (progressCounter * 100) / totalProgress;
                }
            }
        }
    }
   
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, "Uploading Child Entries...");
    
    debugPrintf("uploadChanges Step 2: Creating files on Google Drive\n");
        
    for(i=0; i < remSize; i++)
    {   
        if(dialog_action != 0)
        {
            break;
        }
        
        Json resource = remoteResourceRoot["data"][i];
        
        if(resource["status"].Str() == "upload")
        {
            if(resource["mimeType"].Str() != folderMime)
            {
                result = getParentIdByFolderName(resource["parentFolder"].Str());
             
                if(result != "not_found")
                {
                    debugPrintf("uploadChanges: Creating file with name %s under %s\n",resource["title"].Str().c_str(), result.c_str() );
                    
                    Json obj = uploadFile(resource["path"].Str(),resource["title"].Str(),octetMime,result);
                    
                    if(!obj.Has("id"))
                    {
                        continue;
                    }
                    
                    remoteResourceRoot["data"][i].Add("id", Json(obj["id"].Str()));
                    remoteResourceRoot["data"][i].Add("parentid", Json(result));
                    remoteResourceRoot["data"][i].Add("modifiedDate", Json(obj["modifiedDate"].Str()));
                    remoteResourceRoot["data"][i].Add("fileSize",  Json(obj["fileSize"].Str()));
                    remoteResourceRoot["data"][i].Add("md5Checksum", Json(obj["md5Checksum"].Str()));
                    remoteResourceRoot["data"][i].Add("status", Json("synced"));

                    writeChangesToFile();
                    syncJson = 1;
                    progressCounter++;
                    
                    std::stringstream ss;
                    ss<<"Uploading Child Entries: "<<progressCounter<<" of "<<totalProgress;
                        
                    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0,ss.str().c_str());
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    
                    /*if(userType == "one")
                    {
                            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    }
                    if(userType == "two")
                    {
                            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 40) / totalProgress) - globProgress);
                    }
                    globProgress = (progressCounter * 40) / totalProgress;*/
                    prevProgress = (progressCounter * 100) / totalProgress;
                    
                    //get_free_memory();
                    //debugPrintf("System Memory: %u, %u\n",meminfo.avail,meminfo.total);
                }
                //else mark this file to be deleted, if this feature is ever required
            }
        }
        else if(resource["status"].Str() == "update")
        {
            if(resource["mimeType"].Str() != folderMime)
            {
                result = getParentIdByFolderName(resource["parentFolder"].Str());
             
                if(result != "not_found")
                {
                    debugPrintf("uploadChanges: Re-visioning file with name %s under %s\n",resource["title"].Str().c_str(), result.c_str() );
                    
                    Json obj = uploadFile(resource["path"].Str(),resource["title"].Str(),octetMime,result, resource["id"].Str());

                    if(!obj.Has("id"))
                    {
                        continue;
                    }
                    
                    remoteResourceRoot["data"][i].Add("modifiedDate", Json(obj["modifiedDate"].Str()));
                    remoteResourceRoot["data"][i].Add("fileSize",  Json(obj["fileSize"].Str()));
                    remoteResourceRoot["data"][i].Add("md5Checksum", Json(obj["md5Checksum"].Str()));
                    remoteResourceRoot["data"][i].Add("status", Json("synced"));

                    writeChangesToFile();
                    syncJson = 1;
                    progressCounter++;
                    
                    std::stringstream ss;
                    ss<<"Uploading Child Entries: "<<progressCounter<<" of "<<totalProgress;
                        
                    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0,ss.str().c_str());
                    msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    
                    /*if(userType == "one")
                    {
                            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 100) / totalProgress) - prevProgress);
                    }
                    if(userType == "two")
                    {
                            msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((progressCounter * 40) / totalProgress) - globProgress);
                    }
                    globProgress = (progressCounter * 40) / totalProgress;*/
                    prevProgress = (progressCounter * 100) / totalProgress;
                    
                    //get_free_memory();
                    //debugPrintf("System Memory: %u, %u\n",meminfo.avail,meminfo.total);
                }
            }
        }
    }
    
    debugPrintf("uploadChanges Step 3: Uploading remote.json to google drive\n" );
       
    if(remoteJsonId == "root")
    {
        Json remJson;
        remJson = uploadFile(remoteResourceFile,"remote.json",jsonMime,rootFolderId);
        if(remJson.Has("id"))
        {
            remoteJsonId = remJson["id"].Str();
            storeConfig();
        }
    }
    else if(syncJson == 1)
    {
        debugPrintf("Updating remote.json on the server...\n" );
        uploadFile(remoteResourceFile,"remote.json",jsonMime,rootFolderId,remoteJsonId);
    }
}

std::string calculateMD5Checksum(std::string fpath)
{
    unsigned char md5Out[16];
    char convBuf[64];

    if (md5_file(fpath.c_str(),md5Out) == 0)
    {
        for(int k = 0; k < 16; k++)
        {
                snprintf(convBuf, 64, "%s%02x", convBuf, md5Out[k]);
        }
        //std::string res = convBuf;
        return convBuf;
    }
    return "nill";
}

void detectChanges()
{
    // Upload Mode only for now
    // 1 - If an entry exists in local but not in remote, add it and mark for upload
    // 2 - If an entry exists in local and in remote, check if it's changed by size (folder can be ignored)
    
    u32 lrtSize = localResourceRoot["data"].AsArray().size();
    u32 rrtSize = remoteResourceRoot.Has("data") ? remoteResourceRoot["data"].AsArray().size() : 0;
    u32 i = 0;
    u32 j = 0;
    u32 prevProgress = 0;
    bool found;
     
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, "Detecting Changes...");
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX1, "Finding file differences...");
        
    for(i=0; i < lrtSize; i++)
    {
        found = false;
        
        if(dialog_action != 0)
        {
            break;
        }
        
        Json lres = localResourceRoot["data"][i];
        
        for(j=0; j < rrtSize; j++)
        {
            Json rres = remoteResourceRoot["data"][j];
            
            if(lres["path"].Str() == rres["path"].Str())
            {
                found = true;
                if(lres["mimeType"].Str() != folderMime)
                {
                    if(rres["status"].Str() == "synced")
                    {
                        sysFSStat lstat;
                        sysFsStat(lres["path"].Str().c_str(),&lstat) ;

                        time_t fileModTime = lstat.st_mtime;

                        std::string remoteTime = rres["modifiedDate"].Str().substr(0,18);

                        struct tm remoteModTime = {0};
                        strptime(remoteTime.c_str(), "%Y-%m-%dT%H:%M:%S", &remoteModTime);

                        time_t remoteEpoch = mktime(&remoteModTime);

                        //debugPrintf("Times are %lld, %lld\n", (long long int)fileModTime, (long long int)remoteEpoch);

                        double seconds = difftime(fileModTime, remoteEpoch);
                        if (seconds > 0) 
                        {
                            debugPrintf("Local file has been modified %s\n", rres["path"].Str().c_str());
                            
                            std::string md5c = calculateMD5Checksum(lres["path"].Str());
                        
                            if(md5c != "nill")
                            {
                                if(md5c != rres["md5Checksum"].Str())
                                {    
                                    debugPrintf("Local file MD5 has been changed %s,%s,%s\n", rres["path"].Str().c_str(),md5c.c_str(),rres["md5Checksum"].Str().c_str());
                                    remoteResourceRoot["data"][j].Add("status", Json("update"));
                                }
                            }
                        }
                    }
                }
            }
        }
              
        if(!found)
        {
            debugPrintf("Found New Entry in Local Resource, Adding for sync %s", lres["path"].Str().c_str());
            remoteResourceRoot["data"].Add(lres);
        }
        
        std::stringstream ss;
        ss<<"Finding File Differences: "<<i<<" of "<<lrtSize;

        msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX1,ss.str().c_str());
        msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((i * 100) / lrtSize) - prevProgress);
        msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX1, ((i * 100) / lrtSize) - prevProgress);

        /*if(userType=="two")
        {
                msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, ((i * 20) / lrtSize) - globProgress);
        }*/

        //globProgress = (i * 20) / lrtSize;
        prevProgress = (i * 100) / lrtSize;
    }
    //get_free_memory();
    //debugPrintf("System Memory: %u, %u\n",meminfo.avail,meminfo.total);
}

/**
 * Initiates the user authentication and is also responsible for getting device id
 * @return "authorization_complete" if authenticated and "not_found" if it fails
 */
std::string authenticateUser()
{
    std::string authStatus = "not_found";
    std::string usercode;

    debugPrintf("Device Authentication Function Called...\n");
    usercode = authToken.DeviceAuth();

    debugPrintf("Your User Code is:  %s \n", usercode.c_str());
    
    std::string userCodePrint = "Your User Code is: " + usercode + "\n\n Please visit http://www.google.com/device for approval";
    
    SDL_Surface *codeAuthText = NULL;
    codeAuthText = TTF_RenderText_Blended( Sans, userCodePrint.c_str(), GREY);
    draw_surface(screen, codeAuthText, (appWidth - codeAuthText->w) / 2, (72 * appHeight) / 100);

    u64 epochTime;
    u64 nsec;
    u64 previousCallTime;

    sysGetCurrentTime(&epochTime, &nsec);
    previousCallTime = epochTime;

    while (authStatus != "authorization_complete")
    {
        sysGetCurrentTime(&epochTime, &nsec);
        if (epochTime - previousCallTime > 10)
        {
            debugPrintf("Polling for user authentication....\n");
            authStatus = authToken.Auth();
            previousCallTime = epochTime;
        }
    }
    SDL_FreeSurface(codeAuthText);
    return authStatus;
}

/**
 * Builds a remote Resource Tree from root folder to every file there is
 */
void buildRemoteResourceTree()
{
    std::vector<Json> resourceTree;
    std::vector<std::string> foldersToTraverse;
    Json resourceObject = checkIfRemoteResourceExists(app_title,folderMime,rootFolderId);
    if(resourceObject.Has("id"))
    {
        resourceTree.push_back(Json(resourceObject));
        foldersToTraverse.push_back(resourceObject["id"].Str());
        while(foldersToTraverse.size() > 0)
        {
            Json::Array tempArray = getResourcesUnderFolder(foldersToTraverse.back());
            if(!tempArray.empty())
            {
                for ( Json::Array::iterator i = tempArray.begin() ; i != tempArray.end() ; ++i )
                {
                    Json::Object tmpObject = i->AsObject();
                    if(tmpObject["mimeType"].Str() == folderMime) foldersToTraverse.push_back(tmpObject["id"].Str());
                    resourceTree.push_back(Json(i->AsObject()));
                }          
            }
            foldersToTraverse.pop_back();
        }
    }
    
    debugPrintf("Remote Resource Tree built, writing to Remote Resource Json\n");

    remoteResourceRoot.Add("data",Json(resourceTree));
    
    writeChangesToFile();
}

/**
 * Builds a local resource tree given the path of a local profile folder
 */
void buildLocalResourceTree()
{
    std::vector<Json> resourceTree;

    debugPrintf("Building Resource Tree....\n");

    sysFSDirent entry;
    s32 fd;
    u64 read;
    Header profileList;
    Header profilePaths;

    std::string entryName = "";
    std::string path = "/dev_hdd0/home";
   
    // Check how many profiles exists in user's HDD 0000001 etc
    if (sysFsOpendir(path.c_str(), &fd) == 0)
    {
        while (!sysFsReaddir(fd, &entry, &read) && strlen(entry.d_name) > 0)
        {
            if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
            {
                continue;
            }
            else
            {              
                entryName.clear();
                entryName.assign(entry.d_name, strlen(entry.d_name));
                
                profileList.Add(entryName);
                profilePaths.Add(path + "/" + entryName + "/savedata");
                
                Json resource;
                resource.Add("id", Json("nill"));
                resource.Add("title", Json(entryName));
                resource.Add("path", Json(path + "/" + entryName + "/savedata"));
                resource.Add("mimeType", Json(folderMime));
                resource.Add("parentid", Json(rootFolderId));
                resource.Add("parentFolder",Json(app_title));
                resource.Add("modifiedDate", Json("0"));
                resource.Add("status",Json("upload"));

                resourceTree.push_back(Json(resource));
            }
        }
        sysFsClosedir(fd);
        fd = -1;
    }

    Header::iterator k = profileList.begin();
    Header::iterator l = profilePaths.begin();
    Header folderList;
    Header pathList;
    
    // Recursively list all saves under every profile
    while (k != profileList.end())
    {
        debugPrintf("Entering Save Path...%s\n",l->c_str());
        
        if (sysFsOpendir(l->c_str(), &fd) == 0)
        {
            while (!sysFsReaddir(fd, &entry, &read) && strlen(entry.d_name) > 0)
            {
                if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
                {
                    continue;
                }
                else
                {
                    folderList.Add(entry.d_name);
                    
                    entryName.clear();
                    entryName.assign(entry.d_name, strlen(entry.d_name));

                    std::stringstream fPath;
                    fPath<<*l<<"/"<<entryName;
                    
                    pathList.Add(fPath.str());
                    
                    //debugPrintf("Save Path Entry...%s\n",entryName.c_str());
                    
                    Json resource;
                    resource.Add("id", Json("nill"));
                    resource.Add("title", Json(entryName));
                    resource.Add("path", Json(fPath.str()));
                    resource.Add("mimeType", Json(folderMime));
                    resource.Add("parentid", Json("nill"));
                    resource.Add("parentFolder",Json(k->c_str()));
                    resource.Add("modifiedDate", Json("0"));
                    resource.Add("status",Json("upload"));

                    resourceTree.push_back(Json(resource));
                }
            }
            sysFsClosedir(fd);
            fd = -1;
        }
        ++k,++l;
    }

    debugPrintf("Directory enumeration complete...listing files inside them...\n");

    Header::iterator i = pathList.begin();
    Header::iterator j = folderList.begin();
    
    // Recursively list all files under every save directory
    while (i != pathList.end())
    {
        if (sysFsOpendir(i->c_str(), &fd) == 0)
        {
            while (!sysFsReaddir(fd, &entry, &read) && strlen(entry.d_name) > 0)
            {
                if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
                {
                    continue;
                }
                else
                {
                    entryName.clear();
                    entryName.assign(entry.d_name, strlen(entry.d_name));

                    std::stringstream fPath;
                    fPath<<*i<<"/"<<entryName;
                    
                    //debugPrintf("Entry Path...%s\n",fPath.str().c_str());
                    
                    Json resource;
                    resource.Add("id", Json("nill"));
                    resource.Add("title", Json(entryName));
                    resource.Add("path", Json(fPath.str()));
                    resource.Add("mimeType", Json(octetMime));
                    resource.Add("parentFolder",Json(j->c_str()));
                    resource.Add("parentid", Json("nill"));
                    resource.Add("modifiedDate", Json("0"));
                    resource.Add("fileSize",Json("0"));
                    resource.Add("md5Checksum",Json("0"));
                    resource.Add("status",Json("upload"));

                    resourceTree.push_back(Json(resource));
                }
            }
            sysFsClosedir(fd);
            fd = -1;
        }
        ++i;++j;
    }
    
    //Check for Existence of PS1 and PS2 memory cards
    if(pathExists("/dev_hdd0/savedata"))
    {
        if(pathExists("/dev_hdd0/savedata/vmc"))
        {
            if (sysFsOpendir("/dev_hdd0/savedata/vmc", &fd) == 0)
            {
                while (!sysFsReaddir(fd, &entry, &read) && strlen(entry.d_name) > 0)
                {
                    if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
                    {
                        continue;
                    }
                    else
                    {              
                        entryName.clear();
                        entryName.assign(entry.d_name, strlen(entry.d_name));
                        
                        debugPrintf("Memcard%s\n",entryName.c_str());

                        Json resource;
                        resource.Add("id", Json("nill"));
                        resource.Add("title", Json(entryName));
                        resource.Add("path", Json("/dev_hdd0/savedata/vmc/"+entryName));
                        resource.Add("mimeType", Json(octetMime));
                        resource.Add("parentid", Json(rootFolderId));
                        resource.Add("parentFolder",Json(app_title));
                        resource.Add("modifiedDate", Json("0"));
                        resource.Add("status",Json("upload"));

                        resourceTree.push_back(Json(resource));
                    }
                }
                sysFsClosedir(fd);
                fd = -1;
            }
        }
    }

    debugPrintf("Resource Tree built, writing to Resource Json\n");

    // Overwrite local.json every time
    StdioFile file(localResourceFile, SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, 0644);

    localResourceRoot.Add("data",Json(resourceTree));
    localResourceRoot.WriteFile(file);

    file.Close();
    sysFsChmod(localResourceFile.c_str(),0644);

    debugPrintf("Resource file has been written successfully...\n");
}

/**
 * Starts the authentication and sync process, should be the Drive class
 */
int initCloudDrive(void *arg)
{
    debugPrintf("Checking if User is Authenticated...\n");
    std::string authStatus = isUserAuthenticated();
    
    SDL_Surface *checkingAuthText = NULL;
    checkingAuthText = TTF_RenderText_Blended( Sans, "Checking if User is authenticated...", GREY);
    draw_surface(screen, image, 0, 0);
    draw_surface(screen, checkingAuthText, (appWidth - checkingAuthText->w) / 2, (70 * appHeight) / 100);
    
    if (authStatus == "not_found")
    {
        debugPrintf("User is not authenticated, starting Authentication...\n");
        authenticateUser();
    }
    else
    {
        userType = "two";
        debugPrintf("Possible Token Read from file:  %s \n", authStatus.c_str());

        authToken.setRefreshToken(authStatus);
        if (authToken.Refresh() != "valid")
        {
            debugPrintf("Token Read from file was invalid, need to Re-Authenticate");
            authenticateUser();
        }
    }

    storeConfig();
    debugPrintf("Authentication Phase Complete, checking remote root folder existence\n");
    
    syncing = 1;
    
    SDL_FreeSurface(checkingAuthText);
    
    SDL_Surface *userAuthedText = NULL;
    userAuthedText = TTF_RenderText_Blended( Sans, "Syncing...Please DO NOT Quit or Shutdown the PS3!", GREY);
    draw_surface(screen, image, 0,0);
    draw_surface(screen, userAuthedText, (appWidth - userAuthedText->w) / 2, (70 * appHeight) / 100);
    
    msgDialogOpen2(dialog_type, "Performing Sync...", dialog_handler, NULL, NULL);
    
    //If user new but remote data exists
    Json rootResource = checkIfRemoteResourceExists(app_title,folderMime,"root");
    if(rootResource.Has("id"))
    {
        rootFolderId = rootResource["id"].Str();
        debugPrintf("User has previous data on Google Drive with id %s\n",rootFolderId.c_str());
        if(userType == "one") userType = "three";
    }
    else
    {
        debugPrintf("User has no previous data on Google Drive\n");
        Json rtId;
        rtId = uploadDirectory(app_title,"root");
        
        if(rtId.Has("id"))
        {
            rootFolderId = rtId["id"].Str();
            debugPrintf("Created root directory on Google Drive with id %s\n",rootFolderId.c_str());
            storeConfig();
        }
        else
        {
            debugPrintf("Error creating root folder on Google Drive\n");
        }
    }
    
    buildLocalResourceTree();
    
    //NOTE: LRT = Local Resource Tree and RRT= Remote Resource Tree
    
    //First Time: App never run AND no data on Google Drive
    //DONE: Build Local Resource Tree and write to local.json
    //DONE: Copy Local Resource Tree to Remote Resource Tree
    //DONE: Write remote.json 
    //DONE: Upload all entries in remote.json to Google Drive
    //DONE: Update remote.json with all changes during uploading
    //DONE: Write remote.json to hard drive during changes
    //DONE: Upload remote.json to Google Drive
    if(userType == "one")
    {
        StdioFile file(localResourceFile);
        remoteResourceRoot = Json::ParseFile(file);
        file.Close();

        StdioFile file2(remoteResourceFile);
        remoteResourceRoot.WriteFile(file2);
        file.Close();

        uploadChanges();
    }

    //Second Time: App ran before AND data possibly exists on Google Drive
    //DONE: Build Local Resource Tree and write to local.json
    //DONE: Read Remote Resource Tree
    //TODO: Go through all entries in LRT and search for them in RRT
    //TODO: If any entry does not exist in RRT, insert it into RRT
    //TODO: If any entry exists in RRT, match size, if size different mark as dirty
    //TODO: Upload all changes marked as dirty in RRT to Google Drive
    //TODO: Write all changes or uploads back to RRT and save to HDD
    //TODO: Write RRT to HDD
    //TODO: Upload RRT to Google Drive
    if(userType == "two")
    {
        StdioFile file(remoteResourceFile);
        remoteResourceRoot = Json::ParseFile(file);
        file.Close();
        
        detectChanges();
        
        uploadChanges();
        downloadChanges();
    }

    //First Time: App is installed on a new system but Data exists on Google Drive
    //TODO: Build LRT and write to local.json
    //TODO: Download Remote Resource Tree and write to remote.json
    //TODO: Revert to Second Time scenario
    if(userType == "three")
    {
        Json remoteJson = checkIfRemoteResourceExists("remote.json",jsonMime,rootFolderId);
        
        if(remoteJson.Has("id"))
        {
            remoteJsonId = remoteJson["id"].Str();
            debugPrintf("Downloading remote.json from Google Drive with id %s\n",remoteJsonId.c_str());
            downloadFile(remoteResourceFile, remoteJsonId);
            
            StdioFile file(remoteResourceFile);
            remoteResourceRoot = Json::ParseFile(file);
            file.Close();
            
            downloadChanges();
            debugPrintf("Data from Google Drive downloaded, Please restart app to upload changes\n");
        }
        else
        {    
            debugPrintf("Wow, there's no remote.json on Google Drive!\n");
            //buildRemoteResourceTree();
            //downloadChanges();
        }
    }

    //TODO: After every scenario completion, write last sync date to config
    /*u64 epochTime;
    u64 nsec;

    sysGetCurrentTime(&epochTime, &nsec);
    
    last_sync = new std::string(epochTime);*/
    
    syncing = 0;
    
    msgDialogAbort();
    
    SDL_FreeSurface(userAuthedText);
    
    exitapp = 1;
    
    return 0;
}

int main(int argc, char **argv)
{
    padInfo padinfo;
    padData paddata;
    SDL_Thread *thread=NULL;
    
    std::string imagePath;
    char FontPath[]="/dev_hdd0/game/PSCD00001/USRDIR/font.ttf";
       
    netInitialize();
    debugInit();
    
    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    
    videoState state;
    videoGetState(0, 0, &state);
    videoResolution res;
    videoGetResolution(state.displayMode.resolution,&res);
    
    debugPrintf("Resolution %u,%u\n",res.width,res.height);
    
    appWidth = res.width;
    appHeight = res.height;
    
    switch(appHeight)
    {
        case 1080:
            imagePath = "/dev_hdd0/game/PSCD00001/USRDIR/bg.png";
            break;
        case 720:
            imagePath ="/dev_hdd0/game/PSCD00001/USRDIR/bg-720.png";
            break;
        case 480:
            imagePath ="/dev_hdd0/game/PSCD00001/USRDIR/bg-480.png";
            break;
        case 576:
            imagePath ="/dev_hdd0/game/PSCD00001/USRDIR/bg-576.png";
            break;
        default:
            imagePath = "/dev_hdd0/game/PSCD00001/USRDIR/bg.png";
            break;
    }
    
    screen = SDL_SetVideoMode(appWidth, appHeight, 32 ,SDL_SWSURFACE|SDL_DOUBLEBUF|SDL_FULLSCREEN);

    image = Load_Image(imagePath);
    draw_surface(screen, image, 0,0);

    Sans = TTF_OpenFont( FontPath , 20.0 );
    
    thread = SDL_CreateThread(initCloudDrive, NULL);
    
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysevent_callback, NULL);
    
    ioPadInit(7);
    
    dialog_action = 0;
    dialog_type = static_cast<msgType>(MSG_DIALOG_MUTE_ON | MSG_DIALOG_DOUBLE_PROGRESSBAR);
    
    while(exitapp == 0)
    {
        sysUtilCheckCallback();

        if(xmbopen != 1)
        {
            ioPadGetInfo(&padinfo);
            for(int i = 0; i < MAX_PADS; i++)
            {
                if(padinfo.status[i])
                {
                    ioPadGetData(i, &paddata);
                    if(paddata.BTN_CROSS)
                    {
                        debugPrintf("Quit Called...%d\n",exitapp);
                        if(syncing == 0)
                        {
                            dialog_action = 2;
                            exitapp = 1; //Quit application
                        }
                    }
                }
            }
        }
        SDL_Flip(screen);
    }
    
    debugPrintf("The application exited...%d\n",exitapp);

    msgDialogAbort();
    
    SDL_KillThread(thread);
    SDL_FreeSurface(image);
    SDL_FreeSurface(screen);
    TTF_CloseFont(Sans);

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    
    debugClose();
    netDeinitialize();
    ioPadEnd();

    return 0;
}