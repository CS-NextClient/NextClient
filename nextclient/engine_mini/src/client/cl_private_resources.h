#pragma once

#include <string>

// Private resource folder accessible from filesytem interface.
// The client must be connected to the server to use the function!
std::string PrivateRes_GetPrivateFolder();

// Sends a request to the server for a list of private resources
void PrivateRes_ListRequest();

// Parses the list of private resources and sets it in client_stateex
void PrivateRes_ParseList(const char* data, int len);

// Tries to parse the string and, if it matches a private resource marker, sets the path of the file with the list of private resources
void PrivateRes_ParseDownloadPath(const std::string& cmd);

// Does a few things, such as clearing the resource cache, and setting aliases for private resources
void PrivateRes_PrepareToPrecache();

// Clears the state of private resources and unloads private resources from the cache
void PrivateRes_Clear();

//
void PrivateRes_AddClientOnlyResources(resource_t* list);