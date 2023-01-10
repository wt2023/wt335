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

class configloader_1v1arena : public WorldScript  //worldserver����������
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

class npc_1v1arena : public CreatureScript  //1v1NPC�ű�
{
public:
    npc_1v1arena() : CreatureScript("npc_1v1arena")
    {
    }


    bool JoinQueueArena(Player* player, Creature* me, bool isRated)     //������У������߼���
    {
        if(!player || !me)   //�������player  ���� �������Լ�
            return false;

        if(sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL) > player->getLevel())   //�����ҵȼ�С�� worldconf���õ�1v1 min�ȼ�
            return false;

		uint64 guid = player->GetGUID();    //��ȡ���GUID
        uint8 arenaslot = ArenaTeam::GetSlotByType(ARENA_TEAM_1v1);  //��ȡ���1v1ģʽ��   ������
        uint8 arenatype = ARENA_TYPE_1v1;   //���þ���ģʽΪ1V1  ARENA_TYPE_1v1 = 1
        uint32 arenaRating = 0;         //��������Ϊ0
        uint32 matchmakerRating = 0;     //ƥ������Ϊ0   ���ط֣� 

        // ignore if we already in BG or BG queue
        if (player->InBattleground())  //��������ս����
            return false;

        //check existance
        Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);   // BATTLEGROUND_AA=  ���о�����    bgָ�� ��ȡ���������е����о�����
        if (!bg) //������ǿյ�
        {
            sLog->outError("Arena", "Battleground: template bg (all arenas) not found��");//��������û���ҵ�
            return false;
        }

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, NULL))//������������������
        {
            ChatHandler(player->GetSession()).PSendSysMessage(LANG_ARENA_DISABLED);  //������ʾ�ı������͵ı��������ݶ�Ӧ���ݿ�acore_string��
            return false;
        }

		BattlegroundTypeId bgTypeId = bg->GetBgTypeID();   //��ȡս������ID
        BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bgTypeId, arenatype);   //��ȡս����������ID��ս������id����������  1v1��
        PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), player->getLevel());    //PVP�Ѷ�id����ָ��
        if (!bracketEntry)
            return false;

        GroupJoinBattlegroundResult err = ERR_GROUP_JOIN_BATTLEGROUND_FAIL;

        // check if already in queue   ����Ƿ���Լ������
        if (player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
            //player is already in this queue ����Ѿ������������
            return false;
        // check if has free queue slots  ����Ƿ��п��õ�λ��
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
            arenaRating = at->GetRating();   //��ȡ���������
            matchmakerRating = arenaRating;  //ƥ������
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
        ChatHandler(player->GetSession()).SendSysMessage("�Ѽ�����У���Ⱥ�ƥ��!");
        return true;
    }


    bool CreateArenateam(Player* player, Creature* me, std::string teamName)   //��������������
    {
        if(!player || !me)
            return false;

        uint8 slot = ArenaTeam::GetSlotByType(ARENA_TEAM_1v1);
        if (slot == 0)
            return false;

        // Check if player is already in an arena team
        if (player->GetArenaTeamId(slot))
        {
            player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, player->GetName(), "", ERR_ALREADY_IN_ARENA_TEAM); //������ʾ��Ϣ��������������ʱ  �Ѿ����ڶ��飩
            return false;
        }

        // Create arena team ����һ������������
        ArenaTeam* arenaTeam = new ArenaTeam();

        if (!arenaTeam->Create(player->GetGUID(), ARENA_TEAM_1v1, teamName, 4283124816, 45, 4294242303, 5, 4294705149))  //���û�����ɹ�
        {
            delete arenaTeam;
            ChatHandler(player->GetSession()).SendSysMessage("������������δ֪ԭ�򴴽�ʧ�ܣ�������!");
            return false;
        }

        // Register arena team
        sArenaTeamMgr->AddArenaTeam(arenaTeam);
        arenaTeam->AddMember(player->GetGUID());

        ChatHandler(player->GetSession()).SendSysMessage("���������鴴�����!");

        return true;
    }

    bool Arena1v1CheckTalents(Player* player)  //����츳
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
                ChatHandler(player->GetSession()).SendSysMessage("����1v1����������ֹ���츳,���ܼ���1V1������.");
                return false;
            }

            for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
                if (talentInfo->RankID[rank] == 0)
                    continue;
        }

        uint32 talentsss = sConfigMgr->GetIntDefault("Arena.1v1.ForbiddenTalentpoints", 35);
        if (count >= talentsss)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("�������ƻ���̹���츳��Ͷ�볬��%u���츳,����ֹ����1V1������", talentsss);
            return false;
        }

        return true;
    }

    bool OnGossipHello(Player* player, Creature* me)  //����NPC�Ի�ʱ�����ĺ���
    {
        if(!player || !me)
            return true;

		if (sWorld->getBoolConfig(CONFIG_ARENA_1V1_ENABLE) == false)//�����ȡ����confֵ,�Ǿ���ʱ��Ϊtrue   ��ȡconf����  1v1�Ƿ���
        {
            ChatHandler(player->GetSession()).SendSysMessage("1VS1�ѱ���ֹ");
            return true;
        }

        uint32 eventid = sConfigMgr->GetIntDefault("Arena.1v1.Event", 130);  //��ȡ1v1���¼�ID  Ĭ����130
        if (!sGameEventMgr->IsActiveEvent(eventid))
        {
            if (!eventid)  //����¼�id=0  ȡ������ture  ��ִ������� ������¼�ID
            {
                ChatHandler(player->GetSession()).SendSysMessage("������¼�ID");
                return true;
            }
            else
            {
                GameEventMgr::GameEventDataMap const& events = sGameEventMgr->GetEventMap();

                if (eventid >= events.size())
                {
                    ChatHandler(player->GetSession()).SendSysMessage("������¼�ID");
                    return true;
                }

                GameEventData const& eventData = events[eventid];
                if (!eventData.isValid())
                {
                    ChatHandler(player->GetSession()).SendSysMessage("������¼�ID");
                    return true;
                }

                uint32 diff = sGameEventMgr->NextCheck(eventid);  //80
                std::string timestd = secsToTimeString(diff, true);    //��ȡʱ���  ������ʼ������¼�
                ChatHandler(player->GetSession()).PSendSysMessage("1V1�����%s��ʼ", timestd.c_str());
                return true;
            }
        }

		if (sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL) > player->getLevel())//���ӵȼ�����
		{
			ChatHandler(player->GetSession()).PSendSysMessage("��ĵȼ���Ҫ�ﵽ%u����ȥ������", sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL));
			player->CLOSE_GOSSIP_MENU();
			return true;
		}
		
        if(player->InBattlegroundQueueForBattlegroundQueueType(BATTLEGROUND_QUEUE_1v1))  //�������Ѿ��ڶ�����
                player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "�뿪1V1����������", GOSSIP_SENDER_MAIN, 3, "��ȷ����?", 0, false);
        //else
        //    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "����1V1������(��ϰ��)", GOSSIP_SENDER_MAIN, 20);   �Ҳ������ϰ��


         //������û�ж��飬����ʾ�������飬����� ����ʾelse�ĶԻ�����
        if(player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_1v1)) == 0) 
            player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "����1V1����������", GOSSIP_SENDER_MAIN, 1, "���ڶһ���֤�뵯�����������봴����ս����\n\n�Ƿ񻨷����»��Ҵ���ս��\n\n��ȷ����?", sWorld->getIntConfig(CONFIG_ARENA_1V1_COSTS), true);
        else
        {
            if(player->InBattlegroundQueueForBattlegroundQueueType(BATTLEGROUND_QUEUE_1v1) == false)
            {
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "����1V1������(������)", GOSSIP_SENDER_MAIN, 2);
                player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "ɾ����ľ���������", GOSSIP_SENDER_MAIN, 5, "��ȷ����?\n\nɾ��������1v1����ȫ������!!!", 0, false);
            }

            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "����1V1������ͳ������", GOSSIP_SENDER_MAIN, 4);
        }

        //player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Script Info", GOSSIP_SENDER_MAIN, 8);
        player->SEND_GOSSIP_MENU(68, me->GetGUID());   //���ͶԻ���
        return true;
    }

    //���ڶԻ�����ѡ�����Ժ�
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
                ChatHandler(player->GetSession()).PSendSysMessage("�����ﵽ%u+������ܴ���1v1�������Ŷ� .", sWorld->getIntConfig(CONFIG_ARENA_1V1_MIN_LEVEL));
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
        case 2: // Join Queue Arena (rated)   player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "����1V1������(������)", GOSSIP_SENDER_MAIN, 2);
            {
                if(Arena1v1CheckTalents(player) && JoinQueueArena(player, me, true) == false)
                    ChatHandler(player->GetSession()).SendSysMessage("������з����˲��ִ��������¼���");

                player->CLOSE_GOSSIP_MENU();
                return true;
            }
            break;

        case 20: // Join Queue Arena (unrated)  ��ϰ��������ע���ˣ�
            {
                if(Arena1v1CheckTalents(player) && JoinQueueArena(player, me, false) == false)
                    ChatHandler(player->GetSession()).SendSysMessage("������з����˲��ִ��������¼���");

                player->CLOSE_GOSSIP_MENU();
                return true;
            }
            break;

        case 3: // Leave Queue  player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "�뿪1V1����������", GOSSIP_SENDER_MAIN, 3, "��ȷ����?", 0, false);
            {
                WorldPacket Data;
                Data << (uint8)0x1 << (uint8)0x0 << (uint32)BATTLEGROUND_AA << (uint16)0x0 << (uint8)0x0;
                player->GetSession()->HandleBattleFieldPortOpcode(Data);
                player->CLOSE_GOSSIP_MENU();
                return true;
            }
            break;

        case 4: // get statistics  player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "����1V1������ͳ������", GOSSIP_SENDER_MAIN, 4);
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


        case 5: // Disband arenateam   ɾ������������   player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_CHAT, "ɾ����ľ���������", GOSSIP_SENDER_MAIN, 5, "��ȷ����?\n\nɾ��������1v1����ȫ������!!!", 0, false);
            {
                WorldPacket Data;
                Data << (uint32)player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_1v1));
                player->GetSession()->HandleArenaTeamLeaveOpcode(Data);
                ChatHandler(player->GetSession()).SendSysMessage("1V1���������鱻ɾ��!");
                player->CLOSE_GOSSIP_MENU();
                return true;
            }
            break;

        case 8: // Script Info  �ѱ�ע��  ��Ч��case
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
            sArenaTeamMgr->DistributeArenaPoints();  //�������侺������
        }
    }
};

void AddSC_npc_1v1arena()
{
    new npc_1v1arena();
    new configloader_1v1arena();
    new eventstop();
}
