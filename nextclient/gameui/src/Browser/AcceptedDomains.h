#include <string>

size_t LoadAcceptedDomainsForJsApiFromDisk(std::string fileName);
bool IsUrlWithAcceptedDomain(std::string url);
bool IsV8CurrentContextOnAcceptedDomain();