#pragma once

struct serveritem_t;

class IGameList
{
public:
	enum InterfaceItem_e
	{
		FILTERS,
		GETNEWLIST,
		ADDSERVER,
		ADDCURRENTSERVER,
	};

	virtual bool SupportsItem(InterfaceItem_e item) = 0;
	virtual void StartRefresh(void) = 0;
	virtual void GetNewServerList(void) = 0;
	virtual void StopRefresh(void) = 0;
	virtual bool IsRefreshing(void) = 0;
	virtual serveritem_t &GetServer(unsigned int serverID) = 0;
	virtual void AddNewServer(serveritem_t &server) = 0;
	virtual void ListReceived(bool moreAvailable, int lastUnique) = 0;
	virtual void ApplyFilters(void) = 0;
	virtual int GetInvalidServerListID(void) = 0;
};
