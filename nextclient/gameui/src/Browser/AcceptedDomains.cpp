#include "AcceptedDomains.h"
#include <set>
#include <fstream>
#include <regex>
#include <windows.h>
#include "cef.h"

std::set<std::string> acceptedDomains;

size_t LoadAcceptedDomainsForJsApiFromDisk(std::string fileName) {
	std::ifstream file(fileName);
	if(!file.is_open()) return 0;

	std::string domain;
	while(file >> domain) {
		acceptedDomains.insert(domain);
	}

	return acceptedDomains.size();
}

bool IsUrlWithAcceptedDomain(std::string url) {
	static auto const regex = std::regex(R"(^(?:https?:\/\/)?(?:[^@\/\n]+@)?(?:www\.)?([^:\/\n]+))");
	std::smatch match;
	if(!std::regex_search(url, match, regex)) return false;

	auto domain = match.str(1);
	return acceptedDomains.find(domain) != acceptedDomains.end();
}

bool IsV8CurrentContextOnAcceptedDomain() {
	auto url = CefV8Context::GetCurrentContext()->GetFrame()->GetURL();
	return IsUrlWithAcceptedDomain(url);
}