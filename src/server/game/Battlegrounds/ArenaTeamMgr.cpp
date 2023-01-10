/*
 * Copyright (C) 2016+     AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-GPL2
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "Define.h"
#include "ArenaTeamMgr.h"
#include "World.h"
#include "Log.h"
#include "DatabaseEnv.h"
#include "Language.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"

ArenaTeamMgr::ArenaTeamMgr()
{
    NextArenaTeamId = 1;
    LastArenaLogId = 0;
}

ArenaTeamMgr::~ArenaTeamMgr()
{
    for (ArenaTeamContainer::iterator itr = ArenaTeamStore.begin(); itr != ArenaTeamStore.end(); ++itr)
        delete itr->second;
}

ArenaTeamMgr* ArenaTeamMgr::instance()
{
    static ArenaTeamMgr instance;
    return &instance;
}

// Arena teams collection
ArenaTeam* ArenaTeamMgr::GetArenaTeamById(uint32 arenaTeamId) const
{
    ArenaTeamContainer::const_iterator itr = ArenaTeamStore.find(arenaTeamId);
    if (itr != ArenaTeamStore.end())
        return itr->second;

    return nullptr;
}

ArenaTeam* ArenaTeamMgr::GetArenaTeamByName(const std::string& arenaTeamName) const
{
    std::string search = arenaTeamName;
    std::transform(search.begin(), search.end(), search.begin(), ::toupper);
    for (ArenaTeamContainer::const_iterator itr = ArenaTeamStore.begin(); itr != ArenaTeamStore.end(); ++itr)
    {
        std::string teamName = itr->second->GetName();
        std::transform(teamName.begin(), teamName.end(), teamName.begin(), ::toupper);
        if (search == teamName)
            return itr->second;
    }
    return nullptr;
}

ArenaTeam* ArenaTeamMgr::GetArenaTeamByCaptain(uint64 guid) const
{
    for (ArenaTeamContainer::const_iterator itr = ArenaTeamStore.begin(); itr != ArenaTeamStore.end(); ++itr)
        if (itr->second->GetCaptain() == guid)
            return itr->second;

    return nullptr;
}

void ArenaTeamMgr::AddArenaTeam(ArenaTeam* arenaTeam)
{
    ArenaTeamStore[arenaTeam->GetId()] = arenaTeam;
}

void ArenaTeamMgr::RemoveArenaTeam(uint32 arenaTeamId)
{
    ArenaTeamStore.erase(arenaTeamId);
}

uint32 ArenaTeamMgr::GenerateArenaTeamId()
{
    if (NextArenaTeamId >= 0xFFFFFFFE)
    {
        sLog->outError("Arena team ids overflow!! Can't continue, shutting down server. ");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return NextArenaTeamId++;
}

void ArenaTeamMgr::LoadArenaTeams()
{
    uint32 oldMSTime = getMSTime();

    // Clean out the trash before loading anything
    CharacterDatabase.Execute("DELETE FROM arena_team_member WHERE arenaTeamId NOT IN (SELECT arenaTeamId FROM arena_team)");       // One-time query

    //                                                        0        1         2         3          4              5            6            7           8
    QueryResult result = CharacterDatabase.Query("SELECT arenaTeamId, name, captainGuid, type, backgroundColor, emblemStyle, emblemColor, borderStyle, borderColor, "
    //      9        10        11         12           13        14
        "rating, weekGames, weekWins, seasonGames, seasonWins, `rank` FROM arena_team ORDER BY arenaTeamId ASC");

    if (!result)
    {
        sLog->outString(">> Loaded 0 arena teams. DB table `arena_team` is empty!");
        sLog->outString();
        return;
    }

    QueryResult result2 = CharacterDatabase.Query(
        //              0              1           2             3              4                 5          6     7          8                  9      10
        "SELECT arenaTeamId, atm.guid, atm.weekGames, atm.weekWins, atm.seasonGames, atm.seasonWins, c.name, class, personalRating, matchMakerRating, maxMMR FROM arena_team_member atm"
        " INNER JOIN arena_team ate USING (arenaTeamId)"
        " LEFT JOIN characters AS c ON atm.guid = c.guid"
        " LEFT JOIN character_arena_stats AS cas ON c.guid = cas.guid AND (cas.slot = 0 AND ate.type = 2 OR cas.slot = 1 AND ate.type = 3 OR cas.slot = 2 AND ate.type = 5)"
        " ORDER BY atm.arenateamid ASC");

    uint32 count = 0;
    do
    {
        ArenaTeam* newArenaTeam = new ArenaTeam;

        if (!newArenaTeam->LoadArenaTeamFromDB(result) || !newArenaTeam->LoadMembersFromDB(result2))
        {
            newArenaTeam->Disband(nullptr);
            delete newArenaTeam;
            continue;
        }

        AddArenaTeam(newArenaTeam);

        ++count;
    }
    while (result->NextRow());

    sLog->outString(">> Loaded %u arena teams in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
    sLog->outString();
}

void ArenaTeamMgr::DistributeArenaPoints()  //分配竞技点数
{
    // Used to distribute arena points based on last week's stats  用于  根据上周的统计数据分配竞技场点数
    sWorld->SendWorldText(LANG_DIST_ARENA_POINTS_START);  //发送聊天框系统信息给玩家  分配竞技场点数依靠队伍等级,这可能会花费几分钟时间,请等待..

    sWorld->SendWorldText(LANG_DIST_ARENA_POINTS_ONLINE_START);  //发送聊天框系统信息给玩家 为玩家分发竞技点数...

    // Temporary structure for storing maximum points to add values for all players   临时结构，用于存储所有玩家的最大值
    std::map<uint32, uint32> PlayerPoints;

    // At first update all points for all team members  首先更新所有团队成员的所有积分
    for (ArenaTeamContainer::iterator teamItr = GetArenaTeamMapBegin(); teamItr != GetArenaTeamMapEnd(); ++teamItr)
        if (ArenaTeam* at = teamItr->second)   //at 赋值 第二个字段的值    当不为0时
        {
            at->UpdateArenaPointsHelper(PlayerPoints);    //更新点数
            sScriptMgr->OnBeforeUpdateArenaPoints(at, PlayerPoints);
        }

    SQLTransaction trans = CharacterDatabase.BeginTransaction();

    PreparedStatement* stmt;

    // Cycle that gives points to all players   给所有玩家打分的循环
    for (std::map<uint32, uint32>::iterator playerItr = PlayerPoints.begin(); playerItr != PlayerPoints.end(); ++playerItr)
    {
        // Add points to player if online  给在线玩家增加点数  
        if (Player* player = HashMapHolder<Player>::Find(playerItr->first))
            player->ModifyArenaPoints(playerItr->second, &trans);    

        else    // Update database 更新数据库数据
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_ARENA_POINTS);
            stmt->setUInt32(0, playerItr->second);
            stmt->setUInt32(1, playerItr->first);
            trans->Append(stmt);
        }
    }

    CharacterDatabase.CommitTransaction(trans);

    PlayerPoints.clear();

    sWorld->SendWorldText(LANG_DIST_ARENA_POINTS_ONLINE_END);    //发送信息   为在线玩家完成设置竞技场点数.

    sWorld->SendWorldText(LANG_DIST_ARENA_POINTS_TEAM_START);    //修改账号、竞技场点数等给登录的竞技场队伍，发送更新的数据给在线玩家....

    for (ArenaTeamContainer::iterator titr = GetArenaTeamMapBegin(); titr != GetArenaTeamMapEnd(); ++titr)
    {
        if (ArenaTeam* at = titr->second)
        {
            at->FinishWeek();
            at->SaveToDB();
            at->NotifyStatsChanged();
        }
    }

    sWorld->SendWorldText(LANG_DIST_ARENA_POINTS_TEAM_END);   //发送消息  修改完成.

    sWorld->SendWorldText(LANG_DIST_ARENA_POINTS_END);    //发送消息   分发竞技点数结束.
}
