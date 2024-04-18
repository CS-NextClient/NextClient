#include <string>

size_t LoadAcceptedDomainsFromDisk(std::string fileName);
bool IsUrlWithAcceptedDomain(std::string url);
bool IsV8CurrentContextOnAcceptedDomain();