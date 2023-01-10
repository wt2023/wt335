/*
 *
 * Copyright (C) 2013 Emu-Devstore <http://emu-devstore.com/>
 * Written by Teiby <http://www.teiby.de/>
 *
 */
#pragma execution_character_set("utf-8")
#include "ScriptMgr.h"
#include "ArenaTeamMgr.h"
#include "Common.h"
#include "DisableMgr.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "ArenaTeam.h"
#include "Language.h"
#include "Config.h"
#include "../Custom/String/myString.h"
 //Config
std::vector<uint32> forbiddenTalents;

class configloader_1v1arena : public WorldScript  //worldserver启动加载项
{
public:
    configloader_1v1arena() : WorldScript("configloader_1v1arena") {}


    virtual void OnAfterConfigLoad(bool Reload) override
    {
        std::string blockedTalentsStr = sConfigMgr->GetStringDefault("Arena.1v1.ForbiddenTalentsIDs", "");
        Tokenizer toks(blockedTalentsStr, ',');
        for (auto&& token : toks)
        {
            forbiddenTalents.push_back(std::stoi(token));
        }
    }

};

class npc_1v1arena : public CreatureScript  //1v1NPC脚本
{
public:
    npc_1v1arena() : CreatureScript("npc_1v1arena")
    {
    }


    bool JoinQueueArena(Player* player, Creature* me, bool isRated)     //加入队列，返回逻辑型
    {
        if(!player || !me)   //如果不是player  或者 不是我自己
            return false;

        if(sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL) > player->getLevel())   //如果玩家等级小于 worldconf设置的1v1 min等级
            return false;

		uint64 guid = player->GetGUID();    //获取玩家GUID
        uint8 arenaslot = ArenaTeam::GetSlotByType(ARENA_TEAM_1v1);  //获取玩家1v1模式的   排名？
        uint8 arenatype = ARENA_TYPE_1v1;   //设置竞技模式为1V1  ARENA_TYPE_1v1 = 1
        uint32 arenaRating = 0;         //竞技评级为0
        uint32 matchmakerRating = 0;     //匹配评级为0   隐藏分？ 

        // ignore if we already in BG or BG queue
        if (player->InBattleground())  //如果玩家在战场中
            return false;

        //check existance
        Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);   // BATTLEGROUND_AA=  所有竞技场    bg指针 获取竞技场表中的所有竞技场
        if (!bg) //如果表是空的
        {
            sLog->outError("Arena", "Battleground: template bg (all arenas) not found，");//竞技场表没有找到
            return false;
        }

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, NULL))//如果这个竞技场被禁用
        {
            ChatHandler(player->GetSession()).PSendSysMessage(LANG_ARENA_DISABLED);  //发送提示文本，发送的编号里的内容对应数据库acore_string表
            return false;
        }

		BattlegroundTypeId bgTypeId = bg->GetBgTypeID();   //获取战场类型ID
        BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bgTypeId, arenatype);   //获取战场队列类型ID（战场类型id，竞技类型  1v1）
        PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), player->getLevel());    //PVP难度id常量指针
        if (!bracketEntry)
            return false;

        GroupJoinBattlegroundResult err = ERR_GROUP_JOIN_BATTLEGROUND_FAIL;

        // check if already in queue   检测是否可以加入队列
        if (player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
            //player is already in this queue 玩家已经在这个队列了
            return false;
        // check if has free queue slots  检测是否有可用的位置
        if (!player->HasFreeBattlegroundQueueId())
            return false;

        uint32 ateamId = 0;

        if(isRated)
        {
            ateamId = player->GetArenaTeamId(arenaslot);
            ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(ateamId);
            if (!at)
            {
                player->GetSession()->SendNotInArenaTeamPacket(arenatype);
                return false;
            }

            // get the team rating for queueing
            arenaRating = at->GetRating();   //获取队伍的评级
            matchmakerRating = arenaRating;  //匹配评级
            // the arenateam id must match for everyone in the group

            if (arenaRating <= 0)
                arenaRating = 1;
        }

        BattlegroundQueue &bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
        bg->SetRated(isRated);

		GroupQueueInfo* ginfo = bgQueue.AddGroup(player, NULL, bracketEntry, isRated, false, arenaRating, matchmakerRating, ateamId);
        uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo);
        uint32 queueSlot = player->AddBattlegroundQueueId(bgQueueTypeId);

        WorldPacket data;
        // send status packet (in queue)
		sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenatype, TEAM_NEUTRAL, isRated);
        player->GetSession()->SendPacket(&data);
		if (ateamId)
			sBattlegroundMgr->ScheduleArenaQueueUpdate(ateamId, bgQueueTypeId, bracketEntry->GetBracketId());
        ChatHandler(player->GetSession()).SendSysMessage("已加入队列，请等候匹配!");
        return true;
    }


    bool CreateArenateam(Player* player, Creature* me, std::string teamName)   //创建竞技场队伍
    {
        if(!player || !me)
            return false;

        uint8 slot = ArenaTeam::GetSlotByType(ARENA_TEAM_1v1);
        if (slot == 0)
            return false;

        // Check if player is already in an arena team
        if (player->GetArenaTeamId(slot))
        {
            player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, player->GetName(), "", ERR_ALREADY_IN_ARENA_TEAM); //发送提示信息（创建竞技队伍时  已经存在队伍）
            return false;
        }

        // Create arena team 创建一个竞技场队伍
        ArenaTeam* arenaTeam = new ArenaTeam();

        if (!arenaTeam->Create(player->GetGUID(), ARENA_TEAM_1v1, teamName, 4283124816, 45, 4294242303, 5, 4294705149))  //如果没创建成功
        {
            delete arenaTeam;
            ChatHandler(player->GetSession()).SendSysMessage("竞技场队伍因未知原因创建失败，请重试!");
            return false;
        }

        // Register arena team
        sArenaTeamMgr->AddArenaTeam(arenaTeam);
        arenaTeam->AddMember(player->GetGUID());

        ChatHandler(player->GetSession()).SendSysMessage("竞技场队伍创建完成!");

        return true;
    }

    bool Arena1v1CheckTalents(Player* player)  //检测天赋
    {
        if (!player)
            return false;

        if (sWorld->getBoolConfig(CONFIG_ARENA_1V1_BLOCK_FORBIDDEN_TALENTS) == false)
            return true;

        uint32 count = 0;

        for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
        {
            TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

            if (!talentInfo)
                continue;

            if (std::find(forbiddenTalents.begin(), forbiddenTalents.end(), talentInfo->TalentID) != forbiddenTalents.end())
            {
                ChatHandler(player->GetSession()).SendSysMessage("你有1v1竞技场被禁止的天赋,不能加入1V1竞技场.");
                return false;
            }

            for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
                if (talentInfo->RankID[rank] == 0)
                    continue;
        }

        uint32 talentsss = sConfigMgr->GetIntDefault("Arena.1v1.ForbiddenTalentpoints", 35);
        if (count >= talentsss)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("你在治疗或者坦克天赋中投入超过%u点天赋,被禁止进入1V1竞技场", talentsss);
            return false;
        }

        return true;
    }

    bool OnGossipHello(Player* player, Creature* me)  //当与NPC对话时触发的函数
    {
        if(!player || !me)
            return true;

		if (sWorld->getBoolConfig(CONFIG_ARENA_1V1_ENABLE) == false)//如果读取不了conf值,那就暂时改为true   读取conf里面  1v1是否开启
        {
            ChatHandler(player->GetSession()).SendSysMessage("1VS1已被禁止");
            return true;
        }

        uint32 eventid = sConfigMgr->GetIntDefault("Arena.1v1.Event", 130);  //读取1v1的事件ID  默认是130
        if (!sGameEventMgr->IsActiveEvent(eventid))
        {
            if (!eventid)  //如果事件id=0  取反后是ture  就执行下面的 错误的事件ID
            {
                ChatHandler(player->GetSession()).SendSysMessage("错误的事件ID");
                return true;
            }
            else
            {
                GameEventMgr::GameEventDataMap const& events = sGameEventMgr->GetEventMap();

                if (eventid >= events.size())
                {
                    ChatHandler(player->GetSession()).SendSysMessage("错误的事件ID");
                    return true;
                }

                GameEventData const& eventData = events[eventid];
                if (!eventData.isValid())
                {
                    ChatHandler(player->GetSession()).SendSysMessage("错误的事件ID");
                    return true;
                }

                uint32 diff = sGameEventMgr->NextCheck(eventid);  //80
                std::string timestd = secsToTimeString(diff, true);    //获取时间差  距离活动开始差多少事件
                ChatHandler(player->GetSession()).PSendSysMessage("1V1活动将在%s后开始", timestd.c_str());
                return true;
            }
        }

		if (sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL) > player->getLevel())//增加等级过滤
		{
			ChatHandler(player->GetSession()).PSendSysMessage("你的等级需要达到%u，快去练级！", sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL));
			player->CLOSE_GOSSIP_MENU();
			return true;
		}
		
        if(player->InBattlegroundQueueForBattlegroundQueueType(BATTLEGROUND_QUEUE_1v1))  //如果玩家已经在队列中
                player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "离开1V1竞技场队列", GOSSIP_SENDER_MAIN, 3, "你确定吗?", 0, false);
        //else
        //    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "加入1V1竞技场(练习赛)", GOSSIP_SENDER_MAIN, 20);   我不想加练习赛


         //如果玩家没有队伍，就显示创建队伍，如果有 就显示else的对话内容
        if(player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_1v1)) == 0) 
            player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "创建1V1竞技场队伍", GOSSIP_SENDER_MAIN, 1, "请在兑换验证码弹窗中输入你想创建的战队名\n\n是否花费以下货币创建战队\n\n你确定吗?", sWorld->getIntConfig(CONFIG_ARENA_1V1_COSTS), true);
        else
        {
            if(player->InBattlegroundQueueForBattlegroundQueueType(BATTLEGROUND_QUEUE_1v1) == false)
            {
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "加入1V1竞技场(竞技赛)", GOSSIP_SENDER_MAIN, 2);
                player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "删除你的竞技场队伍", GOSSIP_SENDER_MAIN, 5, "你确定吗?\n\n删除后所有1v1数据全部清理!!!", 0, false);
            }

            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "查阅1V1竞技场统计数据", GOSSIP_SENDER_MAIN, 4);
        }

        //player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Script Info", GOSSIP_SENDER_MAIN, 8);
        player->SEND_GOSSIP_MENU(68, me->GetGUID());   //发送对话框
        return true;
    }

    //当在对话框中选择了以后
    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code) override
    {
        if (!player || !creature)
            return true;

        ClearGossipMenuFor(player);

        ChatHandler handler(player->GetSession());

        if (action == 1)   //GOSSIP_SENDER_MAIN, 1
        {
            if (sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL) <= player->getLevel())
            {
                if (player->GetMoney() >= sWorld->getIntConfig(CONFIG_ARENA_1V1_COSTS) && CreateArenateam(player, creature, code))
                {
                    player->ModifyMoney(sWorld->getIntConfig(CONFIG_ARENA_1V1_COSTS) * -1);
                    OnGossipHello(player, creature);
                    return true;
                } 
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("你必须达到%u+级别才能创建1v1竞技场团队 .", sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL));
            }
        }
        CloseGossipMenuFor(player);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* me, uint32 /*uiSender*/, uint32 uiAction)
    {
        if(!player || !me)
            return true;

        player->PlayerTalkClass->ClearMenus();

        switch (uiAction)
        {
        case 2: // Join Queue Arena (rated)   player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "加入1V1竞技场(竞技赛)", GOSSIP_SENDER_MAIN, 2);
            {
                if(Arena1v1CheckTalents(player) && JoinQueueArena(player, me, true) == false)
                    ChatHandler(player->GetSession()).SendSysMessage("加入队列发生了部分错误，请重新加入");

                player->CLOSE_GOSSIP_MENU();
                return true;
            }
            break;

        case 20: // Join Queue Arena (unrated)  练习赛（被我注释了）
            {
                if(Arena1v1CheckTalents(player) && JoinQueueArena(player, me, false) == false)
                    ChatHandler(player->GetSession()).SendSysMessage("加入队列发生了部分错误，请重新加入");

                player->CLOSE_GOSSIP_MENU();
                return true;
            }
            break;

        case 3: // Leave Queue  player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "离开1V1竞技场队列", GOSSIP_SENDER_MAIN, 3, "你确定吗?", 0, false);
            {
                WorldPacket Data;
                Data << (uint8)0x1 << (uint8)0x0 << (uint32)BATTLEGROUND_AA << (uint16)0x0 << (uint8)0x0;
                player->GetSession()->HandleBattleFieldPortOpcode(Data);
                player->CLOSE_GOSSIP_MENU();
                return true;
            }
            break;

        case 4: // get statistics  player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "查阅1V1竞技场统计数据", GOSSIP_SENDER_MAIN, 4);
            {
                ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_1v1)));
                if(at)
                {
                    std::string sendmess = sString->Format(sString->GetText(CORE_STR_TYPES(98)), at->GetName().c_str(), at->GetStats().Rating, at->GetStats().Rank, at->GetStats().SeasonGames, at->GetStats().SeasonWins, at->GetStats().WeekGames, at->GetStats().WeekWins);
                    sString->Replace(sendmess, "@", "\n");
                    ChatHandler(player->GetSession()).PSendSysMessage(sendmess.c_str());
                }
				player->CLOSE_GOSSIP_MENU();
				return true;
            }
            break;


        case 5: // Disband arenateam   删除竞技场队伍   player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "删除你的竞技场队伍", GOSSIP_SENDER_MAIN, 5, "你确定吗?\n\n删除后所有1v1数据全部清理!!!", 0, false);
            {
                WorldPacket Data;
                Data << (uint32)player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_1v1));
                player->GetSession()->HandleArenaTeamLeaveOpcode(Data);
                ChatHandler(player->GetSession()).SendSysMessage("1V1竞技场队伍被删除!");
                player->CLOSE_GOSSIP_MENU();
                return true;
            }
            break;

        case 8: // Script Info  已被注释  无效的case
            {
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Developer: sunwellcore", GOSSIP_SENDER_MAIN, uiAction);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Website: www.iwowo.top", GOSSIP_SENDER_MAIN, uiAction);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Version: 1.-", GOSSIP_SENDER_MAIN, uiAction);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "<-- Back", GOSSIP_SENDER_MAIN, 7);
                player->SEND_GOSSIP_MENU(68, me->GetGUID());
                return true;
            }
            break;

        }

        OnGossipHello(player, me);
        return true;
    }
};

class eventstop : public GameEventScript
{
public:
    eventstop() : GameEventScript("1v1arena_eventstop") {}

    virtual void OnStop(uint16 event_id) override
    {
        if (event_id == sConfigMgr->GetIntDefault("Arena.1v1.Event", 130))
        {
            sArenaTeamMgr->DistributeArenaPoints();  //结束分配竞技点数
        }
    }
};

void AddSC_npc_1v1arena()
{
    new npc_1v1arena();
    new configloader_1v1arena();
    new eventstop();
}
