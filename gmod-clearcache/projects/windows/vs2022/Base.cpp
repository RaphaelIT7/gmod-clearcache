#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/FactoryLoader.hpp>
#include "util.h"
#include "GameEventListener.h"
#include "datacache/imdlcache.h"
#include "vstdlib/jobthread.h";

#define Cache_Debug 0

static SourceSDK::FactoryLoader engine_loader("engine");
static SourceSDK::FactoryLoader datacache_loader("datacache");

static IGameEventManager2* gameeventmanager = nullptr;
static IMDLCache* MDLCache = nullptr;

static bool connected = false;
unsigned ClearCache(void *params) {
	ThreadSleep(1000);	// We need to wait for the client to shutdown. Because it uses the MDLCache and if we try to use it, it crashes.

	CThreadMutex().Lock();

	MDLCache->Flush(MDLCACHE_FLUSH_ALL);

	CThreadMutex().Unlock();

	return 0;
}

static ThreadHandle_t current_mdlthread;
class DisconnectEventListener : public IGameEventListener2
{
public:
	DisconnectEventListener() = default;

	void FireGameEvent(IGameEvent* event)
	{
		if (connected) 
		{
			current_mdlthread = CreateSimpleThread(ClearCache, nullptr);
			ThreadDetach(current_mdlthread);
		}
	}
};

// We need this listener because client_disconnect is called while joining a server, and we don't want to clear the cache when joining the server.
class ActivateEventListener : public IGameEventListener2
{
public:
	ActivateEventListener() = default;

	void FireGameEvent(IGameEvent* event)
	{
		connected = true;
	}
};
static ActivateEventListener* ActivateListener = new ActivateEventListener;
static DisconnectEventListener* DisconnectListener = new DisconnectEventListener;
void AddAsyncCacheDataType(MDLCacheDataType_t type, char* type_str) {
	MDLCache->SetAsyncLoad(type, true);

	if (!MDLCache->GetAsyncLoad(type)) {
		std::string msg = "Failed to enable AsyncLoad for type ";
		msg.append(type_str);
		msg.append("\n");
		ConDMsg(msg.c_str());
	}
}

#if Cache_Debug == 1
void CheckAsyncCacheDataType(MDLCacheDataType_t type, char* type_str) {
	if (!MDLCache->GetAsyncLoad(type)) {
		std::string msg = "Failed to enable AsyncLoad for type ";
		msg.append(type_str);
		msg.append("\n");
		ConDMsg(msg.c_str());
	}
}

LUA_FUNCTION_STATIC(check_async) {
	CheckAsyncCacheDataType(MDLCACHE_STUDIOHDR, "MDLCACHE_STUDIOHDR");
	CheckAsyncCacheDataType(MDLCACHE_STUDIOHWDATA, "MDLCACHE_STUDIOHWDATA");
	CheckAsyncCacheDataType(MDLCACHE_VCOLLIDE, "MDLCACHE_VCOLLIDE");
	CheckAsyncCacheDataType(MDLCACHE_ANIMBLOCK, "MDLCACHE_ANIMBLOCK");
	CheckAsyncCacheDataType(MDLCACHE_VIRTUALMODEL, "MDLCACHE_VIRTUALMODEL");
	CheckAsyncCacheDataType(MDLCACHE_VERTEXES, "MDLCACHE_VERTEXES");
	CheckAsyncCacheDataType(MDLCACHE_DECODEDANIMBLOCK, "MDLCACHE_DECODEDANIMBLOCK");

	return 1;
}
#endif

GMOD_MODULE_OPEN()
{
	GlobalLUA = LUA;
	gameeventmanager = (IGameEventManager2*)engine_loader.GetFactory()(INTERFACEVERSION_GAMEEVENTSMANAGER2, nullptr);
	if (gameeventmanager == nullptr)
		LUA->ThrowError("unable to initialize IGameEventManager2");

	MDLCache = (IMDLCache*)datacache_loader.GetFactory()(MDLCACHE_INTERFACE_VERSION, nullptr);
	if (MDLCache == nullptr)
		GlobalLUA->ThrowError("unable to initialize IMDLCache");

	gameeventmanager->AddListener(DisconnectListener, "client_disconnect", false);
	gameeventmanager->AddListener(ActivateListener, "player_activate", false);

	LuaPrint("[ClearCache] Successfully Loaded.");
	
	// AddAsyncCacheDataType(MDLCACHE_STUDIOHDR, "MDLCACHE_STUDIOHDR"); cannot be activated
	AddAsyncCacheDataType(MDLCACHE_STUDIOHWDATA, "MDLCACHE_STUDIOHWDATA");
	AddAsyncCacheDataType(MDLCACHE_VCOLLIDE, "MDLCACHE_VCOLLIDE");
	// AddAsyncCacheDataType(MDLCACHE_ANIMBLOCK, "MDLCACHE_ANIMBLOCK"); if activated, it breaks some playermodels
	// AddAsyncCacheDataType(MDLCACHE_VIRTUALMODEL, "MDLCACHE_VIRTUALMODEL"); cannot be activated
	AddAsyncCacheDataType(MDLCACHE_VERTEXES, "MDLCACHE_VERTEXES");
	// AddAsyncCacheDataType(MDLCACHE_DECODEDANIMBLOCK, "MDLCACHE_DECODEDANIMBLOCK"); cannot be activated

	#if Cache_Debug == 1
		LUA->GetField(-1, "engine");
			LUA->PushCFunction(check_async);
			LUA->SetField(-2, "check_async");
		LUA->Pop(1);
	#endif

	return 0;
}

GMOD_MODULE_CLOSE()
{
	return 0;
}