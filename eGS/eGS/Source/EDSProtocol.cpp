#include "..\Header\stdafx.h"
#include "..\Header\EDSProtocol.h"
#include "..\Header\GuildClass.h"
#include "..\Header\winutil.h"
#include "..\Header\LogProc.h"
#include "..\Header\TUnion.h"
#include "..\Header\GameServer.h"
#include "..\Header\GameMain.h"
#include "..\Header\TNotice.h"
#include "..\resource.h"

#include "..\Header\CastleSiege.h"
// GS-N 0.99.60T 0x0044B770
// GS-N	1.00.18	JPN	0x0045A3E0	-	Completed
// GS-CS	1.00.18	JPN	0x0045A3E0	-	Completed

enum EXDB_DATA_TRANSFER_TYPE {
  EX_SEND_P2P = 0x1,
  EX_SEND_BROADCASTING = 0x0,
};

enum EXDB_RESULT {
  EX_RESULT_SUCCESS = 0x1,
  EX_RESULT_FAIL = 0x0,
  EX_RESULT_FAIL_FOR_CASTLE = 0x10,
};


int gGuildListTotal;
int gGuildListCount;

void GDCharClose(int type, char* GuildName, char* Name)
{
	SDHP_USERCLOSE pMsg;

	pMsg.h.c = 0xC1;
	pMsg.h.size = sizeof(pMsg);
	pMsg.h.headcode = 0xD0;
	pMsg.h.subcode = 0x02;
	pMsg.Type = type;
	memcpy(pMsg.CharName, Name, sizeof(pMsg.CharName));
	memset(pMsg.GuildName, 0, sizeof(pMsg.GuildName));

	if ( GuildName != NULL )
	{
		memcpy(pMsg.GuildName, GuildName, sizeof(pMsg.GuildName));
	}

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}


void GDCharCloseRecv(SDHP_USERCLOSE * lpMsg )
{
	char szName[MAX_ACCOUNT_LEN+1];
	char szGuildName[MAX_GUILD_LEN+1];

	memset(szName, 0, sizeof(szName));
	memset(szGuildName, 0, sizeof(szGuildName));
	memcpy(szName, lpMsg->CharName, sizeof(lpMsg->CharName));
	memcpy(szGuildName, lpMsg->GuildName, sizeof(lpMsg->GuildName));

	Guild.CloseMember(szGuildName, szName);
}



struct SDHP_GUILDCREATE
{
	PBMSG_HEAD2 h;
	char GuildName[9];	// 3
	char Master[11];	// C
	BYTE Mark[32];	// 17
	BYTE NumberH;	// 37
	BYTE NumberL;	// 38
	BYTE btGuildType;	// 39
};


void GDGuildCreateSend(int aIndex, char* Name, char* Master,unsigned char* Mark, int iGuildType)
{
	SDHP_GUILDCREATE pMsg;

	pMsg.h.c = 0xC1;
	pMsg.h.headcode = 0xD0;
	pMsg.h.subcode = 0x03;
	pMsg.h.size = sizeof(pMsg);

	memcpy(pMsg.Mark, Mark, sizeof(pMsg.Mark));
	pMsg.Master[MAX_ACCOUNT_LEN] = 0;
	pMsg.GuildName[MAX_GUILD_LEN] = 0;
	memcpy(pMsg.Master, Master, MAX_ACCOUNT_LEN);
	memcpy(pMsg.GuildName, Name, MAX_GUILD_LEN);
	pMsg.NumberH = SET_NUMBERH(aIndex);
	pMsg.NumberL = SET_NUMBERL(aIndex);
	pMsg.btGuildType = iGuildType;

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}





void GDGuildCreateRecv(SDHP_GUILDCREATE_RESULT * lpMsg)
{
	int aIndex = -1;
	PMSG_GUILDCREATED_RESULT pMsg;
	_GUILD_INFO_STRUCT * lpNode;
	char szMaster[MAX_ACCOUNT_LEN+1];
	char szGuildName[MAX_GUILD_LEN+1];

	szMaster[MAX_ACCOUNT_LEN] = 0;
	szGuildName[MAX_GUILD_LEN] = 0;
	memcpy(szMaster, lpMsg->Master, MAX_ACCOUNT_LEN);
	memcpy(szGuildName, lpMsg->GuildName, MAX_GUILD_LEN);

	if (lpMsg->Result == 0 )
	{
		aIndex = MAKE_NUMBERW(lpMsg->NumberH, lpMsg->NumberL);

		PMSG_GUILDCREATED_RESULT pResult;

		PHeadSubSetB((LPBYTE)&pResult, 0xD0, 0x56, sizeof(pResult));
		pResult.Result = 0;
		pResult.btGuildType = lpMsg->btGuildType;

		DataSend(aIndex, (LPBYTE)&pResult, pResult.h.size);

		if ( gObj[aIndex].m_IfState.use != 0 && gObj[aIndex].m_IfState.type == 5 )
		{
			gObj[aIndex].m_IfState.use = 0;
		}

		return;
	}

	if ( lpMsg->Flag == 1 )
	{
		aIndex = MAKE_NUMBERW(lpMsg->NumberH, lpMsg->NumberL);

		if ( gObjIsConnected(aIndex) == TRUE )
		{
			if ( szMaster[0] == gObj[aIndex].Name[0] )
			{
				if ( strcmp(szMaster, gObj[aIndex].Name) == 0 )
				{
					pMsg.h.c = 0xC1;
					pMsg.h.headcode = 0x56;
					pMsg.h.size = sizeof(pMsg);
					pMsg.Result = lpMsg->Result;
					pMsg.btGuildType = lpMsg->btGuildType;

					DataSend(aIndex, (LPBYTE)&pMsg, pMsg.h.size);
				}
			}
		}
	}

	if ( lpMsg->Result == 1 )
	{
		lpNode = Guild.AddGuild(lpMsg->GuildNumber, szGuildName, lpMsg->Mark, szMaster, 0);

		if ( aIndex >= 0 && lpNode != NULL )
		{
			gObj[aIndex].lpGuild = lpNode;
			gObj[aIndex].GuildNumber = lpMsg->GuildNumber;
			gObj[aIndex].GuildStatus = 0x80;
			lpNode->iGuildRival = 0;
			lpNode->iGuildUnion = 0;
			lpNode->iTimeStamp = 0;
			lpNode->Use[0] = true;
			lpNode->pServer[0] = 1;
			lpNode->Count = 1;
			lpNode->GuildStatus[0] = 128;

			gObj[aIndex].iGuildUnionTimeStamp = 0;

			LogAdd("[U.System] Guild is Created - Guild : %s / [%s][%s]",
				szGuildName, gObj[aIndex].AccountID, gObj[aIndex].Name);

			LogAdd("guild war state : %d %d %s", lpNode->WarState, lpNode->WarDeclareState, szGuildName);
			GCGuildViewportNowPaint(aIndex, szGuildName, lpMsg->Mark, TRUE);
		}
	}
}


struct SDHP_GUILDDESTROY
{
	PBMSG_HEAD2 h;
	BYTE NumberH;	// 3
	BYTE NumberL;	// 4
	char GuildName[8];	// 5
	char Master[10];	// D
};



void GDGuildDestroySend(int aIndex, LPSTR Name, LPSTR Master)
{
	SDHP_GUILDDESTROY pMsg;

	pMsg.h.c = 0xC1;
	pMsg.h.headcode = 0xD0;
	pMsg.h.subcode = 0x04;
	pMsg.h.size = sizeof(pMsg);
	pMsg.NumberH = SET_NUMBERH(aIndex);
	pMsg.NumberL = SET_NUMBERL(aIndex);
	memcpy(pMsg.Master, Master, sizeof(pMsg.Master));
	memcpy(pMsg.GuildName, Name, sizeof(pMsg.GuildName));

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}




void GDGuildDestroyRecv(SDHP_GUILDDESTROY_RESULT * lpMsg)
{
	int aIndex = -1;
	char szMaster[MAX_ACCOUNT_LEN+1];
	char szGuildName[MAX_GUILD_LEN+1];
	szMaster[MAX_ACCOUNT_LEN] = 0;
	szGuildName[MAX_GUILD_LEN] = 0;

	memcpy(szMaster, lpMsg->Master, MAX_ACCOUNT_LEN);
	memcpy(szGuildName, lpMsg->GuildName, MAX_GUILD_LEN);

	if ( lpMsg->Flag == 1 )
	{
		aIndex = MAKE_NUMBERW(lpMsg->NumberH, lpMsg->NumberL);

		if ( gObjIsConnected(aIndex) == TRUE )
		{
			if ( szMaster[0] == gObj[aIndex].Name[0] )
			{
				if ( strcmp(szMaster, gObj[aIndex].Name) == 0 )
				{
					GCResultSend(aIndex, 0x53, lpMsg->Result);

					if ( lpMsg->Result == 1 || lpMsg->Result == 4 )
					{
						GCGuildViewportDelNow(aIndex, TRUE);
					}
				}
			}
		}
	}

	if ( lpMsg->Result == 1 || lpMsg->Result == 4)
	{
		_GUILD_INFO_STRUCT * lpGuild = Guild.SearchGuild(szGuildName);

		if ( lpGuild == NULL )
		{
			return;
		}

		_GUILD_INFO_STRUCT * lpRival = Guild.SearchGuild_Number(lpGuild->iGuildRival);

		if ( lpRival != NULL )
		{
			lpRival->iGuildRival = 0;
			lpRival->szGuildRivalName[0] = 0;
		}

		lpGuild->iGuildUnion = 0;
		lpGuild->iGuildRival = 0;

		for ( int n=0;n<MAX_USER_GUILD;n++)
		{
			int iGuildMemberIndex = lpGuild->Index[n];

			if ( lpGuild->Use[n] > 0 && iGuildMemberIndex != -1 )
			{
				LPOBJ lpObj = &gObj[iGuildMemberIndex];
				
				if ( lpObj == NULL )
				{
					continue;
				}

				if ( gObjIsConnected(iGuildMemberIndex) == FALSE )
				{
					continue;
				}

				gObjNotifyUpdateUnionV1(lpObj);
				gObjNotifyUpdateUnionV2(lpObj);
				lpObj->lpGuild = NULL;
				lpObj->GuildNumber = 0;
				lpObj->GuildStatus = -1;
				lpObj->iGuildUnionTimeStamp = 0;

				GCResultSend(n,0x53, 1);
				GCGuildViewportDelNow(lpObj->m_Index, FALSE);

				LogAdd("guild delete : %s guild member delete", gObj[n].Name);
			}
		}

		UnionManager.DelUnion(lpGuild->Number);
		Guild.DeleteGuild(szGuildName, szMaster);

		LogAdd("(%s) guild delete ( Master:%s )", szGuildName, szMaster);
	}
}



struct SDHP_GUILDMEMBERADD
{
	PBMSG_HEAD2 h;
	char GuildName[9];	// 3
	char MemberID[11];	// C
	BYTE NumberH;	// 17
	BYTE NumberL;	// 18
};



void GDGuildMemberAdd(int aIndex, LPSTR Name, LPSTR MemberId)
{
	SDHP_GUILDMEMBERADD pMsg;

	pMsg.h.c = 0xC1;
	pMsg.h.headcode = 0xD0;
	pMsg.h.subcode = 0x05;
	pMsg.h.size = sizeof(pMsg);
	pMsg.NumberH = SET_NUMBERH(aIndex);
	pMsg.NumberL = SET_NUMBERL(aIndex);
	pMsg.MemberID[MAX_ACCOUNT_LEN] = 0;	// #error should be MAX_ACCOUNT_LEN
	pMsg.GuildName[MAX_GUILD_LEN] = 0;	// #error should be MAX_GUILD_LEN
	memcpy(pMsg.MemberID, MemberId, MAX_ACCOUNT_LEN);
	memcpy(pMsg.GuildName, Name, MAX_GUILD_LEN);

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}





void GDGuildMemberAddResult(SDHP_GUILDMEMBERADD_RESULT * lpMsg)
{
	_GUILD_INFO_STRUCT * lpNode;
	int aIndex = -1;
	int HereUserIndex=-1;
	char szMember[MAX_ACCOUNT_LEN+1];
	char szGuildName[MAX_GUILD_LEN+1];
	szMember[MAX_ACCOUNT_LEN] = 0;
	szGuildName[MAX_GUILD_LEN] = 0;

	memcpy(szMember, lpMsg->MemberID, MAX_ACCOUNT_LEN);
	memcpy(szGuildName, lpMsg->GuildName, MAX_GUILD_LEN);

	if ( lpMsg->Flag == 1 )
	{
		aIndex = MAKE_NUMBERW(lpMsg->NumberH, lpMsg->NumberL);

		if ( gObjIsConnected(aIndex) == TRUE )
		{
			if ( szMember[0] == gObj[aIndex].Name[0] )
			{
				if ( strcmp(szMember, gObj[aIndex].Name) == 0 )
				{
					GCResultSend(aIndex, 0x51, lpMsg->Result);
					HereUserIndex = aIndex;
				}
			}
		}
	}

	if ( lpMsg->Result == 1 )
	{
		lpNode = Guild.AddMember(szGuildName, szMember, HereUserIndex, -1, 0, lpMsg->pServer);

		if ( aIndex >= 0 && lpNode != NULL )
		{
			gObj[aIndex].lpGuild = lpNode;
			gObj[aIndex].GuildStatus = 0;
			gObj[aIndex].GuildNumber = lpNode->Number;
			gObj[aIndex].iGuildUnionTimeStamp = 0;
			GCGuildViewportNowPaint(aIndex, szGuildName, gObj[aIndex].lpGuild->Mark, FALSE);
		}
	}
}



struct SDHP_GUILDMEMBERDEL
{
	PBMSG_HEAD2 h;	// C1:33
	BYTE NumberH;	// 3
	BYTE NumberL;	// 4
	char GuildName[8];	// 5
	char MemberID[10];	// D
};



void GDGuildMemberDel(int aIndex, LPSTR Name, LPSTR MemberId)
{
	SDHP_GUILDMEMBERDEL pMsg;

	pMsg.h.c = 0xC1;
	pMsg.h.headcode = 0xD0;
	pMsg.h.subcode = 0x06; 
	pMsg.h.size = sizeof(pMsg);
	pMsg.NumberH = SET_NUMBERH(aIndex);
	pMsg.NumberL = SET_NUMBERL(aIndex);
	memcpy(pMsg.MemberID, MemberId, MAX_ACCOUNT_LEN);
	memcpy(pMsg.GuildName, Name, MAX_GUILD_LEN);

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}




void GDGuildMemberDelResult(SDHP_GUILDMEMBERDEL_RESULT * lpMsg)
{
	int aIndex = -1;
	char szMember[MAX_ACCOUNT_LEN+1];
	char szGuildName[MAX_GUILD_LEN+1];
	szMember[MAX_ACCOUNT_LEN] = 0;
	szGuildName[MAX_GUILD_LEN] = 0;

	memcpy(szMember, lpMsg->MemberID, MAX_ACCOUNT_LEN);
	memcpy(szGuildName, lpMsg->GuildName, MAX_GUILD_LEN);

	if ( lpMsg->Flag == 1 )
	{
		aIndex = MAKE_NUMBERW(lpMsg->NumberH, lpMsg->NumberL);

		if ( gObjIsConnected(aIndex) == TRUE )
		{
			if ( szMember[0] == gObj[aIndex].Name[0] )
			{
				if ( strcmp(szMember, gObj[aIndex].Name) == 0 )
				{
					GCResultSend(aIndex, 0x53, lpMsg->Result);
					LogAdd("Request to delete guild member caused to send 53 %s", gObj[aIndex].Name);
				}
				else
				{
					GCResultSend(aIndex, 0x53, 5);
				}
			}
		}
		else
		{
			LogAdd("%s Not connected.", gObj[aIndex].Name);
		}
	}

	if ( lpMsg->Result == 1 )
	{
		int iMember = 0;

		while ( true )
		{
			if ( gObj[iMember].Connected == PLAYER_PLAYING)
			{
				if ( gObj[iMember].GuildNumber > 0 )
				{
					if ( gObj[iMember].Name[0] == szMember[0] && gObj[iMember].Name[1] == szMember[1] )
					{
						if ( strcmp(gObj[iMember].Name, szMember) == 0 )
						{
							GCGuildViewportDelNow(iMember, FALSE);
							gObj[iMember].lpGuild = NULL;
							gObj[iMember].GuildNumber = 0;
							gObj[iMember].GuildStatus = -1;
							gObjNotifyUpdateUnionV1(&gObj[iMember]);

							LogAdd("User is ejected from guild %d %s", aIndex, gObj[iMember].Name);
							break;
						}
					}
				}
			}

			if ( iMember < OBJMAX-1 )
			{
				iMember++;
			}
			else
			{
				break;
			}
		}

		Guild.DelMember(szGuildName, szMember);
	}
	else
	{
		LogAdd("(%s)Failed to delete from guild Result : %d", szGuildName, lpMsg->Result);
	}
}




void GDGuildUpdate(LPSTR Name, LPSTR Master, GUILDMARK Mark, int score, BYTE count)
{
//#error
/*	SDHP_GUILDUPDATE pMsg;

	pMsg.h.c = 0xC1;
	pMsg.h.headcode = 0xD0;
	pMsg.h.subcode = 0x08; 
	pMsg.h.size = sizeof(pMsg);
	memcpy(pMsg.GuildName, Name, MAX_GUILD_LEN);
	memcpy(pMsg.Mark, Mark, sizeof(pMsg.Mark));
	memcpy(pMsg.Master, Master, MAX_ACCOUNT_LEN);
	pMsg.Score = score;
	pMsg.Count = count;

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);*/
}


void DGGuildUpdateRecv(SDHP_GUILDUPDATE*  lpMsg)
{
	return;	// NO MAcro
}




struct SDHP_GUILDMEMBER_INFO_REQUEST
{
	PBMSG_HEAD2 h;	// C1:35
	BYTE NumberH;	// 3
	BYTE NumberL;	// 4
	char MemberID[10];	// 5
};


void DGGuildMemberInfoRequest(int aIndex)
{
	SDHP_GUILDMEMBER_INFO_REQUEST pMsg;

	pMsg.h.set((LPBYTE)&pMsg,0xD0, 0x07, sizeof(pMsg));
	pMsg.NumberH = SET_NUMBERH(aIndex);
	pMsg.NumberL = SET_NUMBERL(aIndex);
	memcpy(pMsg.MemberID, gObj[aIndex].Name, sizeof(pMsg.MemberID));

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}







void DGGuildMemberInfo(SDHP_GUILDMEMBER_INFO * lpMsg)
{
	BYTE GuildUserBuf[256] = {0};
	BYTE GuildInfoBuf[256] = {0};
	PMSG_SIMPLE_GUILDVIEWPORT pMsg;
	char szGuildName[MAX_GUILD_LEN+1];
	char szName[MAX_ACCOUNT_LEN+1];
	
	szGuildName[MAX_GUILD_LEN] = 0;
	szName[MAX_ACCOUNT_LEN] = 0;
	memcpy(szGuildName, lpMsg->GuildName, MAX_GUILD_LEN);
	memcpy(szName, lpMsg->MemberID, MAX_ACCOUNT_LEN);

	int GuildInfoOfs = sizeof(PMSG_SIMPLE_GUILDVIEWPORT_COUNT);
	int GuildUserOfs = sizeof(PMSG_SIMPLE_GUILDVIEWPORT_COUNT);

	for ( int n=OBJ_STARTUSERINDEX;n<OBJMAX;n++)
	{
		if ( gObj[n].Connected == PLAYER_PLAYING )
		{
			if ( gObj[n].Name[0] == lpMsg->MemberID[0] )
			{
				if ( strcmp(lpMsg->MemberID, gObj[n].Name ) == 0 )
				{
 
					gObj[n].m_ViewSkillState &= ~0xC0000;
					gObj[n].m_ViewSkillState &= ~0x400000;
					gObj[n].m_ViewSkillState &= ~0x800000;
					szGuildName[MAX_GUILD_LEN] = 0;
					g_CastleSiege.GetCsJoinSide(szGuildName, gObj[n].m_btCsJoinSide, gObj[n].m_bCsGuildInvolved);
					g_CastleSiege.NotifySelfCsJoinSide(n);

					strcpy_s(gObj[n].GuildName, sizeof(gObj[n].GuildName), szGuildName);
					gObj[n].lpGuild = Guild.SearchGuild(gObj[n].GuildName);

					if ( gObj[n].lpGuild != NULL )
					{
						gObj[n].GuildStatus = lpMsg->btGuildStatus;
						gObj[n].GuildNumber = gObj[n].lpGuild->Number;
						Guild.ConnectUser(gObj[n].lpGuild, lpMsg->GuildName, gObj[n].Name, n,  lpMsg->pServer);
						/*gObj[n].lpGuild->Notice[MAX_GUILD_NOTICE_LEN-1] = 0;
						gObj[n].lpGuild->Notice[MAX_GUILD_NOTICE_LEN] = 0;*/
	
						if ( strlen(gObj[n].lpGuild->Notice) > 0 )
						{
							GCServerMsgStringSend(gObj[n].lpGuild->Notice, n, 2);
						}

						LogAdd("[U.System][Guild Status][Info] Guild : %s / [%s][%s] / %d",
							szGuildName, gObj[n].AccountID, szName, gObj[n].GuildStatus);

						LogAdd("[%s][%s] GuildName : (%s)", gObj[n].AccountID, szName, szGuildName);

						pMsg.GuildNumber = gObj[n].GuildNumber;
						pMsg.NumberH = SET_NUMBERH(n) & 0x7F;
						pMsg.NumberL = SET_NUMBERL(n);

						if ( strcmp(gObj[n].lpGuild->Names[0], gObj[n].Name) == 0 ) // Case Guild Master
						{
							pMsg.NumberH |= 0x80;
						}

						pMsg.btGuildStatus = gObj[n].GuildStatus;

						if ( gObj[n].lpGuild != NULL )
						{
							pMsg.btGuildType = gObj[n].lpGuild->btGuildType;
						}
						else
						{
							pMsg.btGuildType = -1;
						}

						memcpy(&GuildInfoBuf[GuildInfoOfs], &pMsg, sizeof(PMSG_SIMPLE_GUILDVIEWPORT));
						GuildInfoOfs += sizeof(PMSG_SIMPLE_GUILDVIEWPORT);

						PMSG_SIMPLE_GUILDVIEWPORT_COUNT pGVCount;
						
						pGVCount.h.c = 0xC2;
						pGVCount.h.headcode = 0x65;
						pGVCount.h.sizeH = SET_NUMBERH(GuildInfoOfs);
						pGVCount.h.sizeL = SET_NUMBERL(GuildInfoOfs);
						pGVCount.Count = 0x01;
						memcpy(GuildInfoBuf, &pGVCount, sizeof(PMSG_SIMPLE_GUILDVIEWPORT_COUNT));

						DataSend(n, (LPBYTE)GuildInfoBuf, GuildInfoOfs);

						if ( gObj[n].lpGuild->WarState != 0 )
						{
							GCGuildWarScore(n);
							GCGuildWarDeclare(n, gObj[n].lpGuild->TargetGuildName);
						}
					}
					else
					{
 
						gObj[n].m_btCsJoinSide = 0;
						gObj[n].m_bCsGuildInvolved = 0;

						LogAdd("error : Failed to receive guild information while connecting");
					}

					return;
				}
			}
		}
	}

	Guild.SetServer(szGuildName, szName, lpMsg->pServer);
}




struct SDHP_GUILDALL
{
	char MemberID[11];	// 0
	BYTE btGuildStatus;	// B
	short pServer;	// C
};


struct SDHP_GUILDALL_COUNT
{
	PWMSG_HEAD h;
	int Number;	// 4
	char GuildName[9];	// 8
	char Master[11];	// 11
	unsigned char Mark[32];	// 1C
	int score;	// 3C
	BYTE btGuildType;	// 40
	int iGuildUnion;	// 44
	int iGuildRival;	// 48
	char szGuildRivalName[9];	// 4C
	unsigned char Count;	// 55
};



void DGGuildMasterListRecv(unsigned char * lpData)
{
	SDHP_GUILDALL_COUNT * lpCount;
	SDHP_GUILDALL * lpList;
	int lOfs = sizeof(SDHP_GUILDALL_COUNT);
	char szGuildName[MAX_GUILD_LEN+1];
	char szMasterName[MAX_ACCOUNT_LEN+1];

	memset(szMasterName, 0, sizeof(szMasterName));
	memset(szGuildName, 0, sizeof(szGuildName));
	lpCount = (SDHP_GUILDALL_COUNT *)lpData;
	memcpy(szGuildName, lpCount->GuildName, sizeof(lpCount->GuildName)-1);
	memcpy(szMasterName, lpCount->Master, sizeof(lpCount->Master)-1);

	if ( strlen(szGuildName) < 1 )
	{
		return;
	}

	if ( lpCount->Count < 1 )
	{
		return;
	}

	Guild.AddGuild(lpCount->Number, szGuildName, lpCount->Mark, szMasterName, lpCount->score);
	Guild.SetGuildType(szGuildName, lpCount->btGuildType);

	_GUILD_INFO_STRUCT * lpGuild = Guild.SearchGuild_Number(lpCount->Number);

	if ( lpGuild != NULL )
	{
		lpGuild->iGuildUnion = lpCount->iGuildUnion;
		lpGuild->iGuildRival = lpCount->iGuildRival;
		memcpy(lpGuild->szGuildRivalName, lpCount->szGuildRivalName, sizeof(lpCount->szGuildRivalName)-1);
	}

	for ( int n=0;n<lpCount->Count;n++)
	{
		lpList = (SDHP_GUILDALL *)&lpData[lOfs];

		memcpy(szMasterName, lpList->MemberID, sizeof(lpList->MemberID)-1);
		
		if ( Guild.AddMember(szGuildName, szMasterName, -1, lpCount->Count,lpList->btGuildStatus, lpList->pServer) == FALSE )
		{
			break;
		}

		lOfs += sizeof(SDHP_GUILDALL);
	}
}



void DGGuildScoreUpdate(LPSTR guildname, int score)
{
	SDHP_GUILDSCOREUPDATE pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0xD0, 0x08, sizeof(SDHP_GUILDSCOREUPDATE));
	pMsg.Score = score;
	strcpy_s(pMsg.GuildName, sizeof(pMsg.GuildName), guildname);

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);

	LogAdd("Guild score send : %s %d", guildname, score);
}




void GDGuildScoreUpdateRecv(SDHP_GUILDSCOREUPDATE * lpMsg)
{
	_GUILD_INFO_STRUCT * lpNode = Guild.SearchGuild(lpMsg->GuildName);

	if ( lpNode == NULL )
	{
		return;
	}

	lpNode->TotalScore = lpMsg->Score;
	LogAdd("(%s) Guild score update (%d)", lpMsg->GuildName, lpMsg->Score);
}




void GDGuildNoticeSave(LPSTR guild_name, LPSTR guild_notice)
{
	SDHP_GUILDNOTICE pMsg;

	int len = strlen((char*)guild_notice);

	if ( len < 1 )
	{
		return;
	}

	if ( len > MAX_CHAT_LEN-2 )
	{
		return;
	}

	pMsg.h.c = 0xC1;
	pMsg.h.headcode = 0xD0;
	pMsg.h.subcode = 0x09;
	pMsg.h.size = sizeof(SDHP_GUILDNOTICE);
	strcpy_s(pMsg.GuildName, sizeof(pMsg.GuildName), guild_name);
	strcpy_s(pMsg.szGuildNotice, sizeof(pMsg.szGuildNotice), guild_notice);

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}




void DGGuildNoticeRecv(SDHP_GUILDNOTICE * lpMsg)
{
	_GUILD_INFO_STRUCT * lpNode = Guild.SearchGuild(lpMsg->GuildName);

	if ( lpNode == NULL )
	{
		return;
	}

	strcpy_s(lpNode->Notice, sizeof(lpNode->Notice), lpMsg->szGuildNotice);
	LogAdd("Notice (%s) : %s", lpMsg->GuildName, lpMsg->szGuildNotice);

	for ( int n=0;n<MAX_USER_GUILD;n++)
	{
		if ( lpNode->Use[n] > 0 && lpNode->Index[n] >= 0 )
		{
			GCServerMsgStringSend(lpMsg->szGuildNotice, lpNode->Index[n], 2);
		}
	}
}



struct SDHP_GUILDLISTREQUEST
{
	PBMSG_HEAD h;	// C1:40
};


void DGGuildListRequest()
{
//#error
	/*SDHP_GUILDLISTREQUEST pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x40, sizeof(SDHP_GUILDLISTREQUEST));

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);*/
}









void DGGuildList(SDHP_GUILDCREATED * lpMsg)
{
	//#error
	/*Guild.AddGuild(lpMsg->Number, lpMsg->GuildName, lpMsg->Mark, lpMsg->Master, lpMsg->score);

	if ( GuildListhDlg != NULL )
	{
		SendMessage(::GuildListhPrs, 0x402, gGuildListCount, 0);
		gGuildListCount++;
		/*wsprintf(szTemp, "%8s [%d / %d]", lpMsg->GuildName, gGuildListCount, gGuildListTotal);
		SetDlgItemText(GuildListhDlg, IDS_CURRENT_GUILD_NAME, szTemp);
	}*/
}



struct EXSDHP_SERVERGROUP_GUILD_CHATTING_SEND
{
	PBMSG_HEAD h;	// C1:50
	int iGuildNum;	// 4
	char szCharacterName[10];	// 8
	char szChattingMsg[60];	// 12
};


void GDGuildServerGroupChattingSend(int iGuildNum,  PMSG_CHATDATA* lpChatData)
{
	//#error
	/*EXSDHP_SERVERGROUP_GUILD_CHATTING_SEND pMsg = {0};

	pMsg.h.set((LPBYTE)&pMsg, 0x50, sizeof(EXSDHP_SERVERGROUP_GUILD_CHATTING_SEND));
	pMsg.iGuildNum = iGuildNum;
	memcpy(pMsg.szCharacterName, lpChatData->chatid, sizeof(pMsg.szCharacterName));
	memcpy(pMsg.szChattingMsg, lpChatData->chatmsg, sizeof(pMsg.szChattingMsg));

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);*/
}


void DGGuildServerGroupChattingRecv( EXSDHP_SERVERGROUP_GUILD_CHATTING_RECV* lpMsg)
{
	//#error
	/*PMSG_CHATDATA pMsg ={0};

	pMsg.h.set((LPBYTE)&pMsg, 0x00, sizeof(pMsg));	// #error MAYBE the header is wrong
	memcpy(pMsg.chatid, lpMsg->szCharacterName, sizeof(pMsg.chatid));
	memcpy(pMsg.chatmsg, lpMsg->szChattingMsg, sizeof(pMsg.chatmsg));
	_GUILD_INFO_STRUCT * lpGuildInfo = Guild.SearchGuild_Number(lpMsg->iGuildNum);

	if ( lpGuildInfo == NULL )
		return;

	int iUserIndex = -1;

	if ( lpGuildInfo->Count >= 0 )
	{
		for ( int i=0;i<MAX_USER_GUILD;i++)
		{
			if ( lpGuildInfo->Use[i] != FALSE )
			{
				iUserIndex = lpGuildInfo->Index[i];

				if ( iUserIndex >= 0 )
				{
					if ( lpGuildInfo->Names[i][0] == gObj[iUserIndex].Name[0] )
					{
						if ( !strcmp(lpGuildInfo->Names[i], gObj[iUserIndex].Name ))
						{
							DataSend(iUserIndex, (LPBYTE)&pMsg, pMsg.h.size);
						}
					}
				}
			}
		}
	}*/
}



struct EXSDHP_SERVERGROUP_UNION_CHATTING_SEND
{
	PBMSG_HEAD h;
	int iUnionNum;	// 4
	char szCharacterName[10];	// 8
	char szChattingMsg[60];	// 12
};


void GDUnionServerGroupChattingSend(int iUnionNum,  PMSG_CHATDATA* lpChatData)
{
	//#error
	/*EXSDHP_SERVERGROUP_UNION_CHATTING_SEND pMsg = {0};

	pMsg.h.set((LPBYTE)&pMsg, 0x51, sizeof(pMsg));
	pMsg.iUnionNum = iUnionNum;
	memcpy(pMsg.szCharacterName, lpChatData->chatid, sizeof(pMsg.szCharacterName));
	memcpy(pMsg.szChattingMsg, lpChatData->chatmsg, sizeof(pMsg.szChattingMsg));

	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);*/
}


void DGUnionServerGroupChattingRecv( EXSDHP_SERVERGROUP_UNION_CHATTING_RECV* lpMsg)
{
	//#error
	/*PMSG_CHATDATA pMsg ={0};

	pMsg.h.set((LPBYTE)&pMsg, 0x00, sizeof(pMsg));
	memcpy(pMsg.chatid, lpMsg->szCharacterName, sizeof(pMsg.chatid));
	memcpy(pMsg.chatmsg, lpMsg->szChattingMsg, sizeof(pMsg.chatmsg));
	int iUnionNum = lpMsg->iUnionNum;
	int iGuildCount =0;
	int iGuildList[100] ={0};

	if ( UnionManager.GetGuildUnionMemberList(iUnionNum, iGuildCount, iGuildList) == TRUE)
	{
		int iUserIndex = -1;

		for ( int i=0;i<iGuildCount;i++)
		{
			_GUILD_INFO_STRUCT * lpGuildInfo = Guild.SearchGuild_Number(iGuildList[i]);

			if ( lpGuildInfo == NULL )
				continue;

			for ( int n=0;n<MAX_USER_GUILD;n++)
			{
				if ( lpGuildInfo->Use[n] != FALSE )
				{
					iUserIndex = lpGuildInfo->Index[n];

					if ( iUserIndex >= 0 )
					{
						if ( lpGuildInfo->Names[n][0] == gObj[iUserIndex].Name[0] )
						{
							if ( !strcmp(lpGuildInfo->Names[n], gObj[iUserIndex].Name ))
							{
								DataSend(iUserIndex, (LPBYTE)&pMsg, pMsg.h.size);
							}
						}
					}
				}
			}
		}
	}*/
}


struct EXSDHP_GUILD_ASSIGN_STATUS_REQ
{
	PBMSG_HEAD2 h;
	WORD wUserIndex;	// 4
	BYTE btType;	// 6
	BYTE btGuildStatus;	// 7
	char szGuildName[9];	// 8
	char szTargetName[11];	// 11
};


void GDGuildReqAssignStatus(int aIndex, int iAssignType, LPSTR szTagetName, int iGuildStatus)
{
	EXSDHP_GUILD_ASSIGN_STATUS_REQ pMsg = {0};

	pMsg.h.set((LPBYTE)&pMsg, 0xD0, 0x0A, sizeof(EXSDHP_GUILD_ASSIGN_STATUS_REQ));
	pMsg.btType = iAssignType;
	pMsg.btGuildStatus = iGuildStatus;
	pMsg.wUserIndex = aIndex;
	pMsg.szGuildName[MAX_GUILD_LEN] = 0;
	pMsg.szTargetName[MAX_ACCOUNT_LEN] = 0;
	memcpy(pMsg.szGuildName, gObj[aIndex].lpGuild->Name, MAX_GUILD_LEN);
	memcpy(pMsg.szTargetName, szTagetName, MAX_ACCOUNT_LEN);

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}






void DGGuildAnsAssignStatus(EXSDHP_GUILD_ASSIGN_STATUS_RESULT * lpMsg)
{
	if ( lpMsg->btFlag == 1 )
	{
		_GUILD_INFO_STRUCT * lpNode = Guild.SearchGuild(lpMsg->szGuildName);

		if ( lpNode == NULL )
		{
			lpMsg->btResult = 0;
		}

		PMSG_GUILD_ASSIGN_STATUS_RESULT pMsg = {0};

		pMsg.h.set((LPBYTE)&pMsg, 0xE1, sizeof(PMSG_GUILD_ASSIGN_STATUS_RESULT));
		pMsg.btType = lpMsg->btType;
		pMsg.btResult = lpMsg->btResult;
		memcpy(pMsg.szTagetName, lpMsg->szTargetName, sizeof(pMsg.szTagetName));

		DataSend(lpMsg->wUserIndex, (LPBYTE)&pMsg, sizeof(pMsg));

		if ( lpMsg->btResult == 0 )
		{
			return;
		}

		Guild.SetGuildMemberStatus(lpMsg->szGuildName, lpMsg->szTargetName, lpMsg->btGuildStatus);

		LogAdd("[U.System][Guild Status][Assign] Guild : ( %s ) Member : ( %s ) Status : ( %d )",
			lpMsg->szGuildName, lpMsg->szTargetName, lpMsg->btGuildStatus);

		for ( int i=0;i<MAX_USER_GUILD;i++)
		{
			if ( lpNode->Use[i] > 0 && lpNode->Index[i] >= 0)
			{
				TNotice _Notice(0);

				if ( lpMsg->btGuildStatus == GUILD_ASSISTANT )
				{
					_Notice.SendToUser(lpNode->Index[i], "%s's position will be changed to Assistant Guild Master.", lpMsg->szTargetName);
				}
				else if ( lpMsg->btGuildStatus == GUILD_BATTLE_MASTER )
				{
					_Notice.SendToUser(lpNode->Index[i], "%s's position will be changed to Battle Master.", lpMsg->szTargetName);
				}
				else
				{
					_Notice.SendToUser(lpNode->Index[i], "%s's position will be changed to Guild Member.", lpMsg->szTargetName);
				}
			}
		}
	}
	else if ( lpMsg->btFlag == 0 )
	{
		_GUILD_INFO_STRUCT * lpNode = Guild.SearchGuild(lpMsg->szGuildName);

		if ( lpNode == NULL )
		{
			return;
		}
		
		Guild.SetGuildMemberStatus(lpMsg->szGuildName, lpMsg->szTargetName, lpMsg->btGuildStatus);

		LogAdd("[U.System][Guild Status][Assign] Guild : ( %s ) Member : ( %s ) Status : ( %d )",
			lpMsg->szGuildName, lpMsg->szTargetName, lpMsg->btGuildStatus);
		
		if ( lpMsg->btResult == 0 )
		{
			return;
		}

		for ( int n=0;n<MAX_USER_GUILD;n++)
		{
			if ( lpNode->Use[n] > 0 && lpNode->Index[n] >= 0)
			{
				TNotice _Notice(0);


				if ( lpMsg->btGuildStatus == GUILD_ASSISTANT )
				{
					_Notice.SendToUser(lpNode->Index[n], "%s's position will be changed to Assistant Guild Master.", lpMsg->szTargetName);
				}
				else if ( lpMsg->btGuildStatus == GUILD_BATTLE_MASTER )
				{
					_Notice.SendToUser(lpNode->Index[n], "%s's position will be changed to Battle Master.", lpMsg->szTargetName);
				}
				else
				{
					_Notice.SendToUser(lpNode->Index[n], "%s's position will be changed to Guild Member.", lpMsg->szTargetName);
				}
			}
		}
	}
}


struct EXSDHP_GUILD_ASSIGN_TYPE_REQ
{
	PBMSG_HEAD2 h;
	WORD wUserIndex;	// 4
	BYTE btGuildType;	// 6;
	char szGuildName[9];	// 7
};




void GDGuildReqAssignType(int aIndex, int iGuildType)
{
	EXSDHP_GUILD_ASSIGN_TYPE_REQ pMsg = {0};

	pMsg.h.set((LPBYTE)&pMsg,0xD0, 0xB0, sizeof(EXSDHP_GUILD_ASSIGN_TYPE_REQ));
	pMsg.btGuildType = iGuildType;
	pMsg.wUserIndex = aIndex;
	pMsg.szGuildName[MAX_GUILD_LEN] = 0;
	memcpy(pMsg.szGuildName, gObj[aIndex].lpGuild->Name, MAX_GUILD_LEN);

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
}







void DGGuildAnsAssignType(EXSDHP_GUILD_ASSIGN_TYPE_RESULT * lpMsg)
{
	if ( lpMsg->btFlag == 1 )
	{
		_GUILD_INFO_STRUCT * lpNode = Guild.SearchGuild(lpMsg->szGuildName);

		if ( lpNode == NULL )
		{
			lpMsg->btResult = 0;
		}

		PMSG_GUILD_ASSIGN_TYPE_RESULT pMsg = {0};

		pMsg.h.set((LPBYTE)&pMsg, 0xE2, sizeof(PMSG_GUILD_ASSIGN_TYPE_RESULT));
		pMsg.btGuildType = lpMsg->btGuildType;
		pMsg.btResult = lpMsg->btResult;

		DataSend(lpMsg->wUserIndex, (LPBYTE)&pMsg,sizeof(pMsg));

		if ( lpMsg->btResult == 0 )
		{
			return;
		}

		Guild.SetGuildType(lpMsg->szGuildName, lpMsg->btGuildType);

		for ( int i=0;i<MAX_USER_GUILD;i++)
		{
			if ( lpNode->Use[i] > 0 && lpNode->Index[i] >= 0)
			{
				TNotice pNotice(0);
				pNotice.SendToUser(lpNode->Index[i], "The %s Guild type has been changed to %d ", lpMsg->szGuildName, lpMsg->btGuildType);
			}
		}
	}
	else if ( lpMsg->btFlag == 0 )
	{
		_GUILD_INFO_STRUCT * lpNode = Guild.SearchGuild(lpMsg->szGuildName);

		if ( lpNode == NULL )
		{
			return;
		}

		Guild.SetGuildType(lpMsg->szGuildName, lpMsg->btGuildType);

		if ( lpMsg->btResult == 0 )
		{
			return;
		}		
		
		for ( int n=0;n<MAX_USER_GUILD;n++)
		{
			if ( lpNode->Use[n] > 0 && lpNode->Index[n] >= 0)
			{
				TNotice _Notice(0);
				_Notice.SendToUser(lpNode->Index[n], "The %s Guild type has been changed to %d ", lpMsg->szGuildName, lpMsg->btGuildType);
			}
		}
	}
}





struct EXSDHP_RELATIONSHIP_JOIN_REQ
{
	PBMSG_HEAD h;
	WORD wRequestUserIndex;	// 4
	WORD wTargetUserIndex;	// 6
	BYTE btRelationShipType;	// 8
	int iRequestGuildNum;	// C
	int iTargetGuildNum;	// 10
};


void GDRelationShipReqJoin(int aIndex, int iTargetUserIndex, int iRelationShipType)
{
//#error
/*	_GUILD_INFO_STRUCT * lpGuildInfo = gObj[aIndex].lpGuild;
	_GUILD_INFO_STRUCT * lpTargetGuildInfo = gObj[iTargetUserIndex].lpGuild;

	if ( !lpGuildInfo || !lpTargetGuildInfo )
	{
		return;
	}

	EXSDHP_RELATIONSHIP_JOIN_REQ pMsg = {0};

	pMsg.h.set((LPBYTE)&pMsg, 0xE5, sizeof(EXSDHP_RELATIONSHIP_JOIN_REQ));
	pMsg.wRequestUserIndex = aIndex;
	pMsg.wTargetUserIndex = iTargetUserIndex;
	pMsg.btRelationShipType = iRelationShipType;
	pMsg.iRequestGuildNum = gObj[aIndex].lpGuild->Number;
	pMsg.iTargetGuildNum = gObj[iTargetUserIndex].lpGuild->Number;

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);*/
}







void DGRelationShipAnsJoin(EXSDHP_RELATIONSHIP_JOIN_RESULT * lpMsg)
{
	//#error
/*	_GUILD_INFO_STRUCT * lpReqGuild = Guild.SearchGuild_Number(lpMsg->iRequestGuildNum);
	_GUILD_INFO_STRUCT * lpTargetGuild = Guild.SearchGuild_Number(lpMsg->iTargetGuildNum);

	if ( lpMsg->btFlag == 1 )
	{
		if ( !lpReqGuild || !lpTargetGuild )
		{
			lpMsg->btResult = 0;
		}

		PMSG_RELATIONSHIP_JOIN_BREAKOFF_ANS pMsg ={0};

		pMsg.h.set((LPBYTE)&pMsg, 0xE6, sizeof(pMsg));
		pMsg.btResult = lpMsg->btResult;
		pMsg.btRequestType = G_RELATION_OPERATION_JOIN;
		pMsg.btRelationShipType = lpMsg->btRelationShipType;
		pMsg.btTargetUserIndexH = SET_NUMBERH(lpMsg->wTargetUserIndex);
		pMsg.btTargetUserIndexL = SET_NUMBERL(lpMsg->wTargetUserIndex);

		DataSend(lpMsg->wRequestUserIndex, (LPBYTE)&pMsg, sizeof(pMsg));

		pMsg.btTargetUserIndexH = SET_NUMBERH(lpMsg->wRequestUserIndex);
		pMsg.btTargetUserIndexL = SET_NUMBERL(lpMsg->wRequestUserIndex);

		DataSend(lpMsg->wTargetUserIndex, (LPBYTE)&pMsg, sizeof(pMsg));

		if ( lpMsg->btResult == EX_RESULT_FAIL ||lpMsg->btResult == EX_RESULT_FAIL_FOR_CASTLE )
			return;

	}
	else if ( lpMsg->btFlag == 0 )
	{
		if ( !lpReqGuild || !lpTargetGuild )
			return;

		if ( lpMsg->btResult == EX_RESULT_FAIL ||lpMsg->btResult == EX_RESULT_FAIL_FOR_CASTLE )
			return;

	}

	if (lpMsg->btRelationShipType == G_RELATIONSHIP_UNION )
	{
		lpReqGuild->SetGuildUnion(lpTargetGuild->Number);
		lpTargetGuild->SetGuildUnion(lpTargetGuild->Number);

		LogAdd("[U.System][Union][Join] ( Union Master Guild: %s ), (Union Member Guild: %s)",
			lpTargetGuild->Name, lpReqGuild->Name);
	}
	else if ( lpMsg->btRelationShipType == G_RELATIONSHIP_RIVAL )
	{
		lpReqGuild->SetGuildRival(lpTargetGuild->Number);
		lpTargetGuild->SetGuildRival(lpReqGuild->Number);
		memcpy(lpReqGuild->szGuildRivalName, lpMsg->szTargetGuildName, MAX_GUILD_LEN);
		memcpy(lpTargetGuild->szGuildRivalName, lpMsg->szRequestGuildName, MAX_GUILD_LEN);

		LogAdd("[U.System][Rival][Join] (  %s ) vs ( %s )",
			lpReqGuild->Name, lpTargetGuild->Name);
	}

	for ( int n=0;n<MAX_USER_GUILD;n++)
	{
		if ( lpReqGuild->Use[n] > 0 )
		{
			if ( lpReqGuild->Index[n] >= 0 )
			{
				TNotice _Notice(1);

				if ( lpMsg->btRelationShipType == G_RELATIONSHIP_UNION )
				{
					_Notice.SendToUser(lpReqGuild->Index[n], "Join the %s Alliance.", lpTargetGuild->Name);
				}
				else if ( lpMsg->btRelationShipType == G_RELATIONSHIP_RIVAL )
				{
					_Notice.SendToUser(lpReqGuild->Index[n], "The Hostile status against %s has been concluded.", lpTargetGuild->Name);
				}
			}
		}
	}

	for ( int n=0;n<MAX_USER_GUILD;n++)
	{
		if ( lpTargetGuild->Use[n] > 0 )
		{
			if ( lpTargetGuild->Index[n] >= 0 )
			{
				TNotice _Notice(1);

				if ( lpMsg->btRelationShipType == G_RELATIONSHIP_UNION )
				{
					_Notice.SendToUser(lpTargetGuild->Index[n], "The %s Guild is joining the %s Alliance.", lpReqGuild->Name, lpTargetGuild->Name);
				}
				else if ( lpMsg->btRelationShipType == G_RELATIONSHIP_RIVAL )
				{
					_Notice.SendToUser(lpTargetGuild->Index[n], "The Hostile status against %s has been concluded.", lpReqGuild->Name);
				}
			}
		}
	}*/
}




struct EXSDHP_RELATIONSHIP_BREAKOFF_REQ
{
	PBMSG_HEAD h;
	WORD wRequestUserIndex;	// 4
	WORD wTargetUserIndex;	// 6
	BYTE btRelationShipType;	// 8
	int iRequestGuildNum;	// C
	int iTargetGuildNum;	// 10
};


void GDUnionBreakOff(int aIndex, int iUnionNumber)
{
//#error
	/*_GUILD_INFO_STRUCT * lpGuildInfo = gObj[aIndex].lpGuild;

	if ( !lpGuildInfo )
	{
		return;
	}

	EXSDHP_RELATIONSHIP_BREAKOFF_REQ pMsg = {0};

	pMsg.h.set((LPBYTE)&pMsg, 0xE6, sizeof(EXSDHP_RELATIONSHIP_BREAKOFF_REQ));
	pMsg.wRequestUserIndex = aIndex;
	pMsg.wTargetUserIndex = -1;
	pMsg.btRelationShipType = G_RELATIONSHIP_UNION;
	pMsg.iRequestGuildNum = lpGuildInfo->Number;
	pMsg.iTargetGuildNum = iUnionNumber;

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
	*/
}


//*******************************************************
// START REIVEW HERE

void DGUnionBreakOff(EXSDHP_RELATIONSHIP_BREAKOFF_RESULT * lpMsg)
{
	//#error
	/*_GUILD_INFO_STRUCT * lpReqGuild = Guild.SearchGuild_Number(lpMsg->iRequestGuildNum);
	_GUILD_INFO_STRUCT * lpTargetGuild = Guild.SearchGuild_Number(lpMsg->iTargetGuildNum);

	if ( lpMsg->btFlag == 1 )
	{
		if ( !lpReqGuild )
		{
			lpMsg->btResult = 0;
		}

		PMSG_RELATIONSHIP_JOIN_BREAKOFF_ANS pMsg ={0};

		pMsg.h.set((LPBYTE)&pMsg, 0xE6, sizeof(pMsg));
		pMsg.btResult = lpMsg->btResult;
		pMsg.btRequestType = G_RELATION_OPERATION_BREAKOFF;
		pMsg.btRelationShipType = G_RELATIONSHIP_UNION;
		pMsg.btTargetUserIndexH = SET_NUMBERH(lpMsg->wTargetUserIndex);
		pMsg.btTargetUserIndexL = SET_NUMBERL(lpMsg->wTargetUserIndex);

		DataSend(lpMsg->wRequestUserIndex, (LPBYTE)&pMsg, sizeof(pMsg));

		if ( lpMsg->btResult == EX_RESULT_FAIL ||lpMsg->btResult == EX_RESULT_FAIL_FOR_CASTLE )
			return;

	}
	else if ( lpMsg->btFlag == 0 )
	{
		if ( !lpReqGuild )
			return;

		if ( lpMsg->btResult == EX_RESULT_FAIL ||lpMsg->btResult == EX_RESULT_FAIL_FOR_CASTLE )
			return;

	}

	lpReqGuild->SetGuildUnion(0);

	LogAdd("[GS][U.System][Union][BreakOff] Request Union Member Guild: %s)",
		lpReqGuild->Name);

	for ( int n=0;n<MAX_USER_GUILD;n++)
	{
		if ( lpReqGuild->Use[n] > 0 )
		{
			if ( lpReqGuild->Index[n] >= 0 )
			{
				TNotice _Notice(1);
				_Notice.SendToUser(lpReqGuild->Index[n], "Withdraw from the Alliance.");
			}
		}
	}

	if ( !lpTargetGuild)
		return;

	LogAdd("[GS][U.System][Union][BreakOff] ( Union Master Guild: %s ), (Union Member Guild: %s)",
		lpTargetGuild->Name, lpReqGuild->Name);

	for ( int n=0;n<MAX_USER_GUILD;n++)
	{
		if ( lpTargetGuild->Use[n] > 0 )
		{
			if ( lpTargetGuild->Index[n] >= 0 )
			{
				TNotice _Notice(1);
				_Notice.SendToUser(lpTargetGuild->Index[n], "%s Guild will withdraw from the %s Alliance.", lpReqGuild->Name, lpTargetGuild->Name);
			}
		}
	}*/
}





void GDRelationShipReqBreakOff(int aIndex, int iTargetUserIndex, int iRelationShipType)
{
//#error
	/*
	_GUILD_INFO_STRUCT * lpGuildInfo = gObj[aIndex].lpGuild;
	_GUILD_INFO_STRUCT * lpTargetGuildInfo = gObj[iTargetUserIndex].lpGuild;

	if ( !lpGuildInfo || !lpTargetGuildInfo )
		return;

	EXSDHP_RELATIONSHIP_BREAKOFF_REQ pMsg = {0};

	pMsg.h.set((LPBYTE)&pMsg, 0xE6, sizeof(EXSDHP_RELATIONSHIP_BREAKOFF_REQ));
	pMsg.wRequestUserIndex = aIndex;
	pMsg.wTargetUserIndex = iTargetUserIndex;
	pMsg.btRelationShipType = iRelationShipType;
	pMsg.iRequestGuildNum = lpGuildInfo->Number;
	pMsg.iTargetGuildNum = lpTargetGuildInfo->Number;

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);
	*/
}





void DGRelationShipAnsBreakOff(EXSDHP_RELATIONSHIP_BREAKOFF_RESULT * lpMsg)
{
	//#error
	/*
	if ( lpMsg->btRelationShipType == G_RELATIONSHIP_UNION )
	{
		DGUnionBreakOff(lpMsg);
		return;
	}


	_GUILD_INFO_STRUCT * lpReqGuild = Guild.SearchGuild_Number(lpMsg->iRequestGuildNum);
	_GUILD_INFO_STRUCT * lpTargetGuild = Guild.SearchGuild_Number(lpMsg->iTargetGuildNum);

	if ( lpMsg->btFlag == 1 )
	{
		if ( !lpReqGuild || !lpTargetGuild )
		{
			lpMsg->btResult = 0;
		}

		PMSG_RELATIONSHIP_JOIN_BREAKOFF_ANS pMsg ={0};

		pMsg.h.set((LPBYTE)&pMsg, 0xE6, sizeof(pMsg));
		pMsg.btResult = lpMsg->btResult;
		pMsg.btRequestType = 2;
		pMsg.btRelationShipType = lpMsg->btRelationShipType;
		pMsg.btTargetUserIndexH = SET_NUMBERH(lpMsg->wTargetUserIndex);
		pMsg.btTargetUserIndexL = SET_NUMBERL(lpMsg->wTargetUserIndex);

		if ( lpMsg->wRequestUserIndex != -1 )
			DataSend(lpMsg->wRequestUserIndex, (LPBYTE)&pMsg, sizeof(pMsg));

		pMsg.btTargetUserIndexH = SET_NUMBERH(lpMsg->wRequestUserIndex);
		pMsg.btTargetUserIndexL = SET_NUMBERL(lpMsg->wRequestUserIndex);

		if ( lpMsg->wTargetUserIndex != -1 )
			DataSend(lpMsg->wTargetUserIndex, (LPBYTE)&pMsg, sizeof(pMsg));

		if ( lpMsg->btResult == EX_RESULT_FAIL ||lpMsg->btResult == EX_RESULT_FAIL_FOR_CASTLE )
			return;

	}
	else if ( lpMsg->btFlag == 0 )
	{
		if ( !lpReqGuild || !lpTargetGuild )
			return;

		if ( lpMsg->btResult == EX_RESULT_FAIL ||lpMsg->btResult == EX_RESULT_FAIL_FOR_CASTLE )
			return;

	}

	if ( lpMsg->btRelationShipType == G_RELATIONSHIP_RIVAL )
	{
		lpReqGuild->SetGuildRival(0);
		lpTargetGuild->SetGuildRival(0);
		lpReqGuild->szGuildRivalName[0]=0;
		lpTargetGuild->szGuildRivalName[0]=0;
	}

	LogAdd("[U.System][Rival][BreakOff] (  %s ) vs ( %s )",
		lpReqGuild->Name, lpTargetGuild->Name);

	for ( int n=0;n<MAX_USER_GUILD;n++)
	{
		if ( lpReqGuild->Use[n] > 0 )
		{
			if ( lpReqGuild->Index[n] >= 0 )
			{
				TNotice _Notice(1);

				if ( lpMsg->btRelationShipType == G_RELATIONSHIP_UNION )
				{
					_Notice.SendToUser(lpReqGuild->Index[n], "Withdraw from the %s Alliance.", lpTargetGuild->Name);
				}
				else if ( lpMsg->btRelationShipType == G_RELATIONSHIP_RIVAL )
				{
					_Notice.SendToUser(lpReqGuild->Index[n], "Hostile status with %s has been canceled.", lpTargetGuild->Name);
				}
			}
		}
	}*/
}




void DGRelationShipNotificationRecv(EXSDHP_NOTIFICATION_RELATIONSHIP * lpMsg)
{
	//#error
	/*
	if ( lpMsg->btUpdateFlag == EX_RESULT_FAIL_FOR_CASTLE )
	{
		_GUILD_INFO_STRUCT * lpGuildInfo = Guild.SearchGuild_Number(lpMsg->iGuildList[0]);

		if ( lpGuildInfo )
		{
			lpGuildInfo->iGuildUnion = 0;
			lpGuildInfo->SetTimeStamp();

			LogAdd("[U.System][Union][Auto BreakOff] ( Union Master Guild: %s ), (Union Member Guild: %s)",
				lpGuildInfo->Name, lpGuildInfo->Name);

			for ( int n=0;n<MAX_USER_GUILD;n++)
			{
				if ( lpGuildInfo->Use[n] > 0 )
				{
					if ( lpGuildInfo->Index[n] >= 0 )
					{
						TNotice _Notice(1);
						_Notice.SendToUser(lpGuildInfo->Index[n], "The %s Alliance will be disbanded.", lpGuildInfo->Name);
					}
				}
			}
		}
	}

	for ( int i=0;i<lpMsg->btGuildListCount;i++)
	{
		_GUILD_INFO_STRUCT * lpGuildInfo = Guild.SearchGuild_Number(lpMsg->iGuildList[i]);

		if ( lpGuildInfo )
		{
			lpGuildInfo->SetTimeStamp();
		}
	}*/
}





void DGRelationShipListRecv(EXSDHP_UNION_RELATIONSHIP_LIST * lpMsg)
{
//#error
	/*if ( lpMsg->btRelationShipType == G_RELATIONSHIP_UNION )
	{
		UnionManager.AddUnion(lpMsg->iUnionMasterGuildNumber, lpMsg->szUnionMasterGuildName);
		UnionManager.SetGuildUnionMemberList(lpMsg->iUnionMasterGuildNumber, lpMsg->btRelationShipMemberCount, lpMsg->iRelationShipMember);
	}
	else if ( lpMsg->btRelationShipType == G_RELATIONSHIP_RIVAL )
	{
		UnionManager.AddUnion(lpMsg->iUnionMasterGuildNumber, lpMsg->szUnionMasterGuildName);
		UnionManager.SetGuildRivalMemberList(lpMsg->iUnionMasterGuildNumber, lpMsg->btRelationShipMemberCount, lpMsg->iRelationShipMember);
	}*/
}



struct EXSDHP_UNION_LIST_REQ
{
	PBMSG_HEAD h;
	WORD wRequestUserIndex;	// 4
	int iUnionMasterGuildNumber;	// 8
};


void GDUnionListSend(int iUserIndex, int iUnionMasterGuildNumber)
{
	//#error
	/*
	EXSDHP_UNION_LIST_REQ pMsg = {0};

	pMsg.h.set((LPBYTE)&pMsg, 0xE9, sizeof(EXSDHP_UNION_LIST_REQ));
	pMsg.wRequestUserIndex = iUserIndex;
	pMsg.iUnionMasterGuildNumber = iUnionMasterGuildNumber;

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);*/
}



struct EXSDHP_UNION_LIST
{
	BYTE btMemberNum;	// 0
	BYTE Mark[32];	// 1
	char szGuildName[8];	// 21
};


struct EXSDHP_UNION_LIST_COUNT
{
	PWMSG_HEAD2 h;
	BYTE btCount;	// 4
	BYTE btResult;	// 5
	WORD wRequestUserIndex;	// 6
	int iTimeStamp;	// 8
	BYTE btRivalMemberNum;	// C
	BYTE btUnionMemberNum;	// D
};


struct PMSG_UNIONLIST_COUNT
{
	PWMSG_HEAD2 h;
	BYTE btCount;	// 4
	BYTE btResult;	// 5
	BYTE btRivalMemberNum;	// 6
	BYTE btUnionMemberNum;	// 7
};



struct PMSG_UNIONLIST
{
	BYTE btMemberNum;	// 0
	BYTE Mark[32];	// 1
	char szGuildName[8];	// 21
};


void DGUnionListRecv(LPBYTE aRecv)
{
	//#error
	/*
	EXSDHP_UNION_LIST_COUNT * lpRecvMsg = (EXSDHP_UNION_LIST_COUNT *)aRecv;
	EXSDHP_UNION_LIST * lpRecvMsgBody = (EXSDHP_UNION_LIST *)((DWORD)aRecv + 0x10 ) ;	// #error it 0x10 ???
	char cBUFFER_V1[6000];
	memset(cBUFFER_V1, 0, sizeof(cBUFFER_V1));
	int iSize = MAKE_NUMBERW(lpRecvMsg->h.sizeH, lpRecvMsg->h.sizeL);
	int iBodySize = iSize - 0x10;	// #error

	if ( iBodySize < 0 )
		return;

	if ( iBodySize > 6000 )
		return;

	if ( lpRecvMsg->btCount < 0 || lpRecvMsg->btCount > 100 )
		return;

	PMSG_UNIONLIST_COUNT * lpMsg = (PMSG_UNIONLIST_COUNT *)cBUFFER_V1;
	PMSG_UNIONLIST * lpMsgBody = (PMSG_UNIONLIST *)((DWORD)cBUFFER_V1+sizeof(PMSG_UNIONLIST_COUNT));
	lpMsg->btResult = lpRecvMsg->btResult;
	lpMsg->btCount = lpRecvMsg->btCount;
	lpMsg->btRivalMemberNum = lpRecvMsg->btRivalMemberNum;
	lpMsg->btUnionMemberNum = lpRecvMsg->btUnionMemberNum;

	for (int i=0;i<lpMsg->btCount;i++)
	{
		lpMsgBody[i].btMemberNum = lpRecvMsgBody[i].btMemberNum;
		memcpy(lpMsgBody[i].szGuildName, lpRecvMsgBody[i].szGuildName, sizeof(lpMsgBody->szGuildName));
		memcpy(lpMsgBody[i].Mark, lpRecvMsgBody[i].Mark, sizeof(lpMsgBody->Mark));
	}

	PHeadSetW((LPBYTE)lpMsg, 0xE9, lpMsg->btCount * sizeof(PMSG_UNIONLIST) + 0x10);	// #warning Check plus operation ( why 0x10 ??? )
	DataSend(lpRecvMsg->wRequestUserIndex, (LPBYTE)lpMsg, MAKE_NUMBERW(lpMsg->h.sizeH, lpMsg->h.sizeL));*/
}




struct EXSDHP_KICKOUT_UNIONMEMBER_REQ
{
	PBMSG_HEAD2 h;
	WORD wRequestUserIndex;	// 4
	BYTE btRelationShipType;	// 6
	char szUnionMasterGuildName[8];	// 7
	char szUnionMemberGuildName[8];	// F
};



void GDRelationShipReqKickOutUnionMember(int aIndex, LPSTR szTargetGuildName)
{
	//#error
	/*
	_GUILD_INFO_STRUCT * lpGuildInfo = gObj[aIndex].lpGuild;

	if ( lpGuildInfo == NULL )
	{
		return;
	}


	EXSDHP_KICKOUT_UNIONMEMBER_REQ pMsg = {0};
	pMsg.h.set((LPBYTE)&pMsg, 0xEB, 0x01, sizeof(EXSDHP_KICKOUT_UNIONMEMBER_REQ));
	pMsg.wRequestUserIndex = aIndex;
	pMsg.btRelationShipType = GUILD_RELATION_UNION;
	memcpy(pMsg.szUnionMasterGuildName, lpGuildInfo->Name, sizeof(pMsg.szUnionMasterGuildName));
	memcpy(pMsg.szUnionMemberGuildName, szTargetGuildName, sizeof(pMsg.szUnionMemberGuildName));

	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);*/
}



struct PMSG_KICKOUT_UNIONMEMBER_RESULT
{
	PBMSG_HEAD2 h;
	BYTE btResult;	// 4
	BYTE btRequestType;	// 5
	BYTE btRelationShipType;	// 6
};


void DGRelationShipAnsKickOutUnionMember(EXSDHP_KICKOUT_UNIONMEMBER_RESULT * lpMsg)
{
	//#error
	/*
	_GUILD_INFO_STRUCT * lpUnionMasterGuild = Guild.SearchGuild(lpMsg->szUnionMasterGuildName);
	_GUILD_INFO_STRUCT * lpUnionMemberGuild = Guild.SearchGuild(lpMsg->szUnionMemberGuildName);

	if ( lpMsg->btFlag == 1 )
	{
		if ( !lpUnionMasterGuild )
		{
			lpMsg->btResult = 0;
		}

		PMSG_KICKOUT_UNIONMEMBER_RESULT pMsg ={0};

		pMsg.h.set((LPBYTE)&pMsg, 0xEB, 0x01, sizeof(pMsg));
		pMsg.btResult = lpMsg->btResult;
		pMsg.btRequestType = 2;
		pMsg.btRelationShipType = 1;

		DataSend(lpMsg->wRequestUserIndex, (LPBYTE)&pMsg, sizeof(pMsg));

		if ( lpMsg->btResult == 0 || lpMsg->btResult == 0x10 )
			return;
	}
	else if ( lpMsg->btFlag == 0 )
	{
		if ( !lpUnionMemberGuild )
			return;

		if ( lpMsg->btResult == 0 || lpMsg->btResult == 0x10 )
			return;
	}		

	if ( lpUnionMasterGuild )
	{
		for ( int n=0;n<MAX_USER_GUILD;n++)
		{
			if ( lpUnionMasterGuild->Use[n] > 0 )
			{
				if ( lpUnionMasterGuild->Index[n] >= 0 )
				{
					TNotice _Notice(1);
					_Notice.SendToUser(lpUnionMasterGuild->Index[n], "%s Guild will withdraw from the %s Alliance.",
						lpMsg->szUnionMemberGuildName, lpMsg->szUnionMasterGuildName);
				}
			}
		}
	}

	LogAdd("[GS][U.System][Union][KickOut] ( Union Master Guild: %s ), (Union Member Guild: %s)",
		lpMsg->szUnionMasterGuildName, lpMsg->szUnionMemberGuildName);

	if ( !lpUnionMemberGuild )
		return;

	lpUnionMemberGuild->SetGuildUnion(0);

	for ( int n=0;n<MAX_USER_GUILD;n++)
	{
		if ( lpUnionMemberGuild->Use[n] > 0 )
		{
			if ( lpUnionMemberGuild->Index[n] >= 0 )
			{
				TNotice _Notice(1);
				_Notice.SendToUser(lpUnionMemberGuild->Index[n], "Withdraw from the Alliance.");
			}
		}
	}
	*/
}





struct FHP_FRIENDLIST_REQ
{
	PBMSG_HEAD h;
	short Number;	// 4
	char Name[10];	// 6
	BYTE pServer;	// 10
};





void FriendListRequest(int aIndex)
{
	//#error
	/*
	if ( gObjIsConnectedGP(aIndex) == FALSE )
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	if (gObj[aIndex].m_FriendServerOnline == 2 )
	{
		LogAdd("error-L3 : [%s][%s] FriendServer Online Request ", gObj[aIndex].AccountID, gObj[aIndex].Name);
		return;
	}

	if (gObj[aIndex].m_FriendServerOnline == FRIEND_SERVER_STATE_OFFLINE )
	{
		LogAdd("error-L3 : [%s][%s] FriendServer Offline", gObj[aIndex].AccountID, gObj[aIndex].Name);
		return;
	}

	FHP_FRIENDLIST_REQ pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x60, sizeof(FHP_FRIENDLIST_REQ));
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));
	pMsg.Number = aIndex;
	pMsg.pServer = (BYTE)gGameServerCode;
	
	wsDataServerCli.DataSend((char*)&pMsg, pMsg.h.size);

	LogAdd("[%s] Friend List Request", gObj[aIndex].Name);

	gObj[aIndex].m_FriendServerOnline = 2;
	*/
}



struct FHP_FRIENDLIST
{
	char Name[10];	// 0
	BYTE Server;	// A
};


struct FHP_FRIENDLIST_COUNT
{
	PWMSG_HEAD2 h;
	short Number;	// 4
	char Name[10];	// 6
	BYTE Count;// 10
	BYTE MailCount;	// 11
};


struct PMSG_FRIEND_LIST_COUNT
{
	PWMSG_HEAD2 h;
	BYTE MemoCount;	// 4
	BYTE MailTotal;	// 5
	BYTE Count;	// 6
};


void FriendListResult(LPBYTE lpMsg)
{
//#error
	/*
	FHP_FRIENDLIST_COUNT * lpCount;
	FHP_FRIENDLIST * lpList;
	int lOfs = sizeof(FHP_FRIENDLIST_COUNT);
	PMSG_FRIEND_LIST_COUNT pCount;
	int pOfs = sizeof(PMSG_FRIEND_LIST_COUNT);

	lpCount = (FHP_FRIENDLIST_COUNT *)lpMsg;
	lpList  = (FHP_FRIENDLIST *)(&lpMsg[lOfs * sizeof(FHP_FRIENDLIST)]);

	char_ID Name( lpCount->Name);
	BYTE TmpSendBuf[2000];
	
	if ( !gObjIsConnectedGP(lpCount->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	gObj[lpCount->Number].m_FriendServerOnline = 0;
	pCount.Count = lpCount->Count;
	pCount.MemoCount = lpCount->MailCount;
	pCount.MailTotal = 50;

	if ( pCount.Count != 0 )
	{
		for ( int n=0;n<pCount.Count;n++)
		{
			lpList = (FHP_FRIENDLIST *)&lpMsg[lOfs];
			memcpy(&TmpSendBuf[pOfs], lpList, sizeof(FHP_FRIENDLIST));
			lOfs+=sizeof(FHP_FRIENDLIST);
			pOfs+=sizeof(FHP_FRIENDLIST);	// #warning why plus that??
		}
	}

	pCount.h.set((LPBYTE)&pCount, 0xD0, 0xC0, pOfs); //#error #ChatServer
	memcpy(TmpSendBuf, &pCount, sizeof(PMSG_FRIEND_LIST_COUNT));

	DataSend(lpCount->Number, TmpSendBuf, pOfs);

	LogAdd("[%s] Friend List Count (%d) Send",
		gObj[lpCount->Number].Name, pCount.Count);
		*/
}


struct PMSG_FRIEND_ADD_SIN_REQ
{
	PBMSG_HEAD h;
	char Name[10];	// 3
};



void WaitFriendListResult(FHP_WAITFRIENDLIST_COUNT * lpMsg)
{
	//#error
	/*
	PMSG_FRIEND_ADD_SIN_REQ pMsg;
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : (%s) Index %s %d ",
			Name.GetBuffer(), __FILE__, __LINE__);
		return;
	}

	if ( Name.GetLength() < 1 )
		return;

	pMsg.h.set((LPBYTE)&pMsg, 0xC2, sizeof(pMsg));
	memcpy(pMsg.Name, lpMsg->FriendName, sizeof(pMsg.Name));

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));

	LogAdd("[%s] WaitFriend List [%s]", gObj[lpMsg->Number].Name, Name.GetBuffer());
	*/
}



struct FHP_FRIEND_STATE_C
{
	PBMSG_HEAD h;
	short Number;	// 4
	char Name[10];	// 6
	BYTE State;	// 10
};


void FriendStateClientRecv(PMSG_FRIEND_STATE_C * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_FRIEND_STATE_C pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x62, sizeof(pMsg));
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));
	pMsg.State = lpMsg->ChatVeto;

	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);
	*/
}





void FriendStateRecv(FHP_FRIEND_STATE * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : (%s) Index %s %d ", Name.GetBuffer(), __FILE__, __LINE__);
		return;
	}

	PMSG_FRIEND_STATE pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0xC4, sizeof(pMsg));
	memcpy(pMsg.Name, lpMsg->FriendName, sizeof(pMsg.Name));
	pMsg.State = lpMsg->State;

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));

	LogAdd("[%s] Friend State (%d)", gObj[lpMsg->Number].Name, lpMsg->State);
	*/

}





struct FHP_FRIEND_ADD_REQ
{
	PBMSG_HEAD h;
	short Number;	// 4
	char Name[10];	// 6
	char FriendName[10];	// 10
};



struct PMSG_FRIEND_ADD_RESULT
{
	PBMSG_HEAD h;
	unsigned char Result;	// 3
	char Name[10];	// 4
	BYTE State;	// E
};



void FriendAddRequest(PMSG_FRIEND_ADD_REQ * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_FRIEND_ADD_REQ pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x63, sizeof(pMsg));
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));
	memcpy(pMsg.FriendName, lpMsg->Name, sizeof(pMsg.FriendName));
	pMsg.Number = aIndex;
	char_ID szFriendName(lpMsg->Name);
	
	LogAdd("[%s] Friend add request Name: [%s]", gObj[aIndex].Name,		szFriendName.GetBuffer());

	if ( !strcmp(gObj[aIndex].Name, szFriendName.GetBuffer()))
	{
		LogAdd("error-L3 : [%s] Friend add : Not self", gObj[aIndex].Name);
		
		PMSG_FRIEND_ADD_RESULT pResult;

		pResult.h.set((LPBYTE)&pResult, 0xC1, sizeof(pResult));
		memcpy(pResult.Name, lpMsg->Name, sizeof(pMsg.Name));
		pResult.Result = 5;
		pResult.State = -1;

		DataSend(aIndex, (LPBYTE)&pResult, sizeof(pResult));
	}
	else
	{
		wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);
	}
	*/
}		





void FriendAddResult(FHP_FRIEND_ADD_RESULT * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	PMSG_FRIEND_ADD_RESULT pMsg;
	char_ID szFriendName(lpMsg->FriendName);

	pMsg.h.set((LPBYTE)&pMsg, 0xC1, sizeof(pMsg));
	memcpy(pMsg.Name, lpMsg->FriendName, sizeof(pMsg.Name));
	pMsg.Result = lpMsg->Result;
	pMsg.State = lpMsg->Server;

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));

	LogAdd("[%s] Friend add result : (%d) friend Name : (%s)",
		Name.GetBuffer(), lpMsg->Result, szFriendName.GetBuffer());
		*/
}



struct FHP_WAITFRIEND_ADD_REQ
{
	PBMSG_HEAD h;
	unsigned char Result;	// 3
	short Number;	// 4
	char Name[10];	// 6
	char FriendName[10];	// 10
};


void WaitFriendAddRequest(PMSG_FRIEND_ADD_SIN_RESULT * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_WAITFRIEND_ADD_REQ pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x64, sizeof(pMsg));
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));
	memcpy(pMsg.FriendName, lpMsg->Name, sizeof(pMsg.FriendName));
	pMsg.Result = lpMsg->Result;
	pMsg.Number = aIndex;

	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);

	{
		char_ID fname(lpMsg->Name);
		LogAdd("[%s] WaitFriend add request [%s]", gObj[aIndex].Name, fname.GetBuffer());
	}
	*/
}





void WaitFriendAddResult(FHP_WAITFRIEND_ADD_RESULT * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	PMSG_FRIEND_ADD_RESULT pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0xC1, sizeof(pMsg));
	memcpy(pMsg.Name, lpMsg->FriendName, sizeof(pMsg.Name));
	pMsg.Result = lpMsg->Result;
	pMsg.State = lpMsg->pServer;

	char_ID szFriendName(lpMsg->FriendName);

	LogAdd("[%s] WaitFriend add result (%d) [%s]",
		Name.GetBuffer(), lpMsg->Result, szFriendName.GetBuffer());

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));
	*/
}





void FriendDelRequest(PMSG_FRIEND_DEL_REQ * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_FRIEND_ADD_REQ pMsg;
	char_ID Name(lpMsg->Name);

	pMsg.h.set((LPBYTE)&pMsg, 0x65, sizeof(pMsg));
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));
	memcpy(pMsg.FriendName, Name.GetBuffer(), sizeof(pMsg.FriendName));
	pMsg.Number = aIndex;

	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);

	LogAdd("[%s] Friend del request [%s]", gObj[aIndex].Name, Name.GetBuffer());
	*/
}



struct PMSG_FRIEND_DEL_RESULT
{
	PBMSG_HEAD h;
	unsigned char Result;	// 3
	char Name[10];	// 4
};


void FriendDelResult(FHP_FRIEND_DEL_RESULT * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	PMSG_FRIEND_DEL_RESULT pMsg;
	char_ID FriendName(lpMsg->FriendName);

	pMsg.h.set((LPBYTE)&pMsg, 0xC3, sizeof(pMsg));
	memcpy(pMsg.Name, lpMsg->FriendName, sizeof(pMsg.Name));
	pMsg.Result = lpMsg->Result;

	LogAdd("[%s] Friend del result (%d) [%s]",
		Name.GetBuffer(), lpMsg->Result, FriendName.GetBuffer());

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));
	*/
}


struct FHP_FRIEND_CHATROOM_CREATE_REQ
{
	PBMSG_HEAD h;
	short Number;	// 4
	char Name[10];	// 6
	char fName[10];	// 10
};



void FriendChatRoomCreateReq(PMSG_FRIEND_ROOMCREATE_REQ * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_FRIEND_CHATROOM_CREATE_REQ pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x66, sizeof(pMsg));
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));
	pMsg.Number = aIndex;
	memcpy(pMsg.fName, lpMsg->Name, sizeof(pMsg.fName));
	
	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);

	char_ID szName(lpMsg->Name);

	LogAdd("[%s] Chatroom create request [%s]", gObj[aIndex].Name, szName.GetBuffer());
	*/
}



struct PMSG_FRIEND_ROOMCREATE_RESULT
{
	PBMSG_HEAD h;
	unsigned char ServerIp[15];	// 3
	unsigned short RoomNumber;	// 12
	unsigned long Ticket;	// 14
	unsigned char Type;	// 18
	char Name[10];	// 19
	unsigned char Result;	// 23
};



void FriendChatRoomCreateResult(FHP_FRIEND_CHATROOM_CREATE_RESULT * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d (%s)", __FILE__, __LINE__, Name.GetBuffer());
		return;
	}

	PMSG_FRIEND_ROOMCREATE_RESULT pMsg;

	pMsg.h.setE((LPBYTE)&pMsg, 0xCA, sizeof(pMsg));
	memcpy(pMsg.ServerIp, lpMsg->ServerIp, sizeof(pMsg.ServerIp));
	pMsg.RoomNumber = lpMsg->RoomNumber;
	pMsg.Ticket = lpMsg->Ticket;
	pMsg.Type = lpMsg->Type;
	pMsg.Result = lpMsg->Result;
	memcpy(pMsg.Name, lpMsg->FriendName, sizeof(pMsg.Name));

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));

	LogAdd("[%s] Chatroom create result (%d) ticket:(%d)",
		gObj[lpMsg->Number].Name, lpMsg->Result, lpMsg->Ticket);
		*/
}




struct FHP_FRIEND_MEMO_SEND
{
	PWMSG_HEAD h;
	short Number;	// 4
	DWORD WindowGuid;	// 8
	char Name[10];	// C
	char ToName[10];	// 16
	char Subject[32];	// 20
	BYTE Dir;	// 40
	BYTE Action;	// 41
	short MemoSize;	// 42
	BYTE Photo[18];	// 44
	char Memo[1000];	// 56
};


void FriendMemoSend(PMSG_FRIEND_MEMO * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	char_ID Name(lpMsg->Name);

	if ( gObj[aIndex].Money < 1000 )
	{
		FHP_FRIEND_MEMO_SEND_RESULT pResult;

		pResult.Number = aIndex;
		pResult.Result = 7;
		pResult.WindowGuid = lpMsg->WindowGuid;
		memcpy(pResult.Name, gObj[aIndex].Name, sizeof(pResult.Name));

		FriendMemoSendResult(&pResult);

		return;
	}

	if ( lpMsg->MemoSize < 0 || lpMsg->MemoSize > 1000 )
	{
		FHP_FRIEND_MEMO_SEND_RESULT pResult;

		pResult.Number = aIndex;
		pResult.Result = 0;
		pResult.WindowGuid = lpMsg->WindowGuid;
		memcpy(pResult.Name, gObj[aIndex].Name, sizeof(pResult.Name));

		FriendMemoSendResult(&pResult);

		return;
	}

	if ( Name.GetLength() < 1 )
	{
		FHP_FRIEND_MEMO_SEND_RESULT pResult;

		pResult.Number = aIndex;
		pResult.Result = 0;
		pResult.WindowGuid = lpMsg->WindowGuid;
		memcpy(pResult.Name, gObj[aIndex].Name, sizeof(pResult.Name));

		FriendMemoSendResult(&pResult);

		return;
	}

	FHP_FRIEND_MEMO_SEND pMsg;
	int bsize = lpMsg->MemoSize + sizeof(pMsg) - sizeof(pMsg.Memo);

	pMsg.h.set((LPBYTE)&pMsg, 0x70, bsize);
	pMsg.WindowGuid = lpMsg->WindowGuid;
	pMsg.MemoSize = lpMsg->MemoSize;
	pMsg.Number = aIndex;
	pMsg.Dir = lpMsg->Dir;
	pMsg.Action = lpMsg->Action;
	memcpy(pMsg.Subject, lpMsg->Subject, sizeof(pMsg.Subject));
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));
	memcpy(pMsg.Photo, gObj[aIndex].CharSet, sizeof(pMsg.Photo));
	memcpy(pMsg.ToName, lpMsg->Name, sizeof(pMsg.ToName));
	memcpy(pMsg.Memo, lpMsg->Memo, lpMsg->MemoSize);

	wsDataServerCli.DataSend((PCHAR)&pMsg, bsize);

	LogAdd("[%s] Friend mail send %s (Size:%d)", gObj[aIndex].Name, Name.GetBuffer(), bsize);
	*/
}





void MngFriendMemoSend(PMSG_JG_MEMO_SEND * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( lpMsg->MemoSize < 0 || lpMsg->MemoSize > 1000 )
		return;

	if ( Name.GetLength() < 1 )
		return;

	FHP_FRIEND_MEMO_SEND pMsg;

	int bsize = lpMsg->MemoSize + sizeof(pMsg) - sizeof(pMsg.Memo);

	pMsg.h.set((LPBYTE)&pMsg, 0x70, bsize);
	pMsg.WindowGuid = 0;
	pMsg.MemoSize = lpMsg->MemoSize;
	pMsg.Number = -1;
	pMsg.Dir = 0;
	pMsg.Action = 0;
	memcpy(pMsg.Subject, lpMsg->Subject, sizeof(pMsg.Subject));
	memcpy(pMsg.Name, lpMsg->Name , sizeof(pMsg.Name));
	memset(pMsg.Photo, 0, sizeof(pMsg.Photo));
	memcpy(pMsg.ToName, lpMsg->TargetName, sizeof(pMsg.ToName));
	memcpy(pMsg.Memo, lpMsg->Memo, lpMsg->MemoSize);

	wsDataServerCli.DataSend((PCHAR)&pMsg, bsize);

	LogAdd("JoinServer mail send %s (Size:%d)", Name.GetBuffer(), bsize);
	*/
}





BOOL WithdrawUserMoney(LPSTR Type, LPOBJ lpObj, int Withdraw_Money)
{
	//#error
	/*
	int oldmoney;

	if ( (lpObj->Money - Withdraw_Money) >= 0 )
	{
		oldmoney = lpObj->Money;
		lpObj->Money -= Withdraw_Money;

		GCMoneySend(lpObj->m_Index, lpObj->Money);
		
		LogAdd("[%s][%s] (%s) Pay Money(In Inventory) : %d - %d = %d",
			lpObj->AccountID, lpObj->Name, Type, oldmoney, Withdraw_Money, lpObj->Money);

		return TRUE;
	}

	return FALSE;
	*/
	return 0;
}



struct PMSG_FRIEND_MEMO_RESULT
{
	PBMSG_HEAD h;
	BYTE Result;	// 3
	DWORD WindowGuid;	// 4
};


void FriendMemoSendResult(FHP_FRIEND_MEMO_SEND_RESULT * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !strcmp(Name.GetBuffer(), "webzen"))	// #warning Deathway
	{
		LogAdd("JoinServer Send Mail result : (%d)", lpMsg->Result);
		return;
	}

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	PMSG_FRIEND_MEMO_RESULT pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0xC5, sizeof(pMsg));
	pMsg.Result = lpMsg->Result;
	pMsg.WindowGuid = lpMsg->WindowGuid;

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));

	LogAdd("[%s] Friend mail Send result : (%d) guid:(%d)",
		gObj[lpMsg->Number].Name, lpMsg->Result, lpMsg->WindowGuid);

	if ( pMsg.Result == 1 )
	{
		WithdrawUserMoney("Mail", &gObj[lpMsg->Number], 1000);
	}
	*/
}


struct FHP_FRIEND_MEMO_LIST_REQ
{
	PBMSG_HEAD h;
	WORD Number;	// 4
	char Name[10];	// 6
};



void FriendMemoListReq(int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_FRIEND_MEMO_LIST_REQ pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x71, sizeof(pMsg));
	pMsg.Number = aIndex;
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));

	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);

	LogAdd("[%s] Friend mail list request", gObj[aIndex].Name);
	*/
}


struct PMSG_FRIEND_MEMO_LIST
{
	PBMSG_HEAD h;
	WORD Number;	// 4
	char Name[10];	// 6
	char Date[30];	// 10
	char Subject[32];	// 2E
	unsigned char read;	// 4E
};



void FriendMemoList(FHP_FRIEND_MEMO_LIST * lpMsg)
{
	//#error
	/*
	char_ID SendName(lpMsg->SendName);
	char_ID RecvName(lpMsg->RecvName);

	if ( !gObjIsConnectedGP(lpMsg->Number, RecvName.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	int memoindex;
	char subject[33]="";
	char date[31]="";

	memoindex = lpMsg->MemoIndex;
	memcpy(subject, lpMsg->Subject, sizeof(lpMsg->Subject));
	memcpy(date, lpMsg->Date, sizeof(lpMsg->Date));

	PMSG_FRIEND_MEMO_LIST pMsg;

	pMsg.h.setE((LPBYTE)&pMsg, 0xC6, sizeof(pMsg));
	memcpy(pMsg.Date, lpMsg->Date, sizeof(pMsg.Date));
	memcpy(pMsg.Name, lpMsg->SendName, sizeof(pMsg.Name));
	memcpy(pMsg.Subject, lpMsg->Subject, sizeof(pMsg.Subject));
	pMsg.Number = memoindex;	// #warning maybe here and error Deathway
	pMsg.read = lpMsg->read;

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));
	
	LogAdd("[%s] Friend Mail list : (%d:%10s:%s:%s)",
		SendName.GetBuffer(), memoindex, date, RecvName.GetBuffer(), subject);
		*/
}


struct FHP_FRIEND_MEMO_RECV_REQ
{
	PBMSG_HEAD h;
	short Number;	// 4
	WORD MemoIndex;	// 6
	char Name[10];	// 8
};



void FriendMemoReadReq(PMSG_FRIEND_READ_MEMO_REQ * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_FRIEND_MEMO_RECV_REQ pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x72, sizeof(pMsg));
	pMsg.MemoIndex = lpMsg->MemoIndex;
	pMsg.Number = aIndex;
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));

	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);

	LogAdd("[%s] Friend Mail read Index:%d", 
		gObj[aIndex].Name, lpMsg->MemoIndex);
		*/
}


struct PMSG_FRIEND_READ_MEMO
{
	PWMSG_HEAD h;
	WORD MemoIndex;	// 4
	short MemoSize;	// 6
	BYTE Photo[18];	// 8
	BYTE Dir;	// 1A
	BYTE Action;	//1B
	char Memo[1000];	// 1C
};



void FriendMemoRead(FHP_FRIEND_MEMO_RECV * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	PMSG_FRIEND_READ_MEMO pMsg;

	int nsize = sizeof(pMsg)-1000;

	if ( lpMsg->MemoSize > 1000 )
	{
		LogAdd("error-L2 : Friend Memo Max %s %d", __FILE__, __LINE__);
		return;
	}

	nsize += lpMsg->MemoSize;

	pMsg.h.setE((LPBYTE)&pMsg, 0xC7, nsize);
	pMsg.MemoIndex = lpMsg->MemoIndex;
	pMsg.MemoSize = lpMsg->MemoSize;
	pMsg.Dir = lpMsg->Dir;
	pMsg.Action = lpMsg->Action;
	memcpy(pMsg.Photo, lpMsg->Photo,sizeof(pMsg.Photo));
	memcpy(pMsg.Memo, lpMsg->Memo, lpMsg->MemoSize);

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, nsize);

	LogAdd("[%s] Friend Mail read (%d)",
		gObj[lpMsg->Number].Name, lpMsg->MemoIndex);
		*/
}


struct FHP_FRIEND_MEMO_DEL_REQ
{
	PBMSG_HEAD h;
	short Number;	// 4
	WORD MemoIndex;	// 6
	char Name[10];	// 8
};



void FriendMemoDelReq(PMSG_FRIEND_MEMO_DEL_REQ * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_FRIEND_MEMO_DEL_REQ pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x73, sizeof(pMsg));
	pMsg.MemoIndex = lpMsg->MemoIndex;
	pMsg.Number = aIndex;
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));

	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);

	LogAdd("[%s] Friend mail delete request Index:(%d)",
		gObj[aIndex].Name, lpMsg->MemoIndex);
		*/
}



struct PMSG_FRIEND_MEMO_DEL_RESULT
{
	PBMSG_HEAD h;
	unsigned char Result;	// 3
	WORD MemoIndex;	// 4
};


void FriendMemoDelResult(FHP_FRIEND_MEMO_DEL_RESULT * lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	PMSG_FRIEND_MEMO_DEL_RESULT pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0xC8, sizeof(pMsg));
	pMsg.MemoIndex = lpMsg->MemoIndex;
	pMsg.Result = lpMsg->Result;

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));

	LogAdd("[%s] Friend mail delete request (%d) index:(%d)",
		Name.GetBuffer(), lpMsg->Result, lpMsg->MemoIndex);
		*/
}


void _GUILD_INFO_STRUCT::SetGuildUnion(int iGuildNumber)	// line : 102
{
	this->iGuildUnion = iGuildNumber;
	this->SetTimeStamp();
};	// line : 105

void _GUILD_INFO_STRUCT::SetGuildRival(int iGuildNumber)	// line : 108
{
	this->iGuildRival = iGuildNumber;
	this->SetTimeStamp();
};	// line : 111

void _GUILD_INFO_STRUCT::SetTimeStamp()	// line : 117
{
	this->iTimeStamp++;
};


struct FHP_FRIEND_INVITATION_REQ
{
	PBMSG_HEAD h;
	short Number;	// 4
	char Name[10];	// 6
	char FriendName[10];	// 10
	WORD RoomNumber;	// 1A
	DWORD WindowGuid;	// 1C
};


void FriendRoomInvitationReq(PMSG_ROOM_INVITATION * lpMsg, int aIndex)
{
	//#error
	/*
	if ( !gObjIsConnectedGP(aIndex))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	FHP_FRIEND_INVITATION_REQ pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0x74, sizeof(pMsg));
	pMsg.Number = aIndex;
	pMsg.RoomNumber = lpMsg->RoomNumber;
	pMsg.WindowGuid = lpMsg->WindowGuid;
	memcpy(pMsg.FriendName, lpMsg->Name, sizeof(pMsg.FriendName));
	memcpy(pMsg.Name, gObj[aIndex].Name, sizeof(pMsg.Name));

	wsDataServerCli.DataSend((PCHAR)&pMsg, pMsg.h.size);

	LogAdd("[%s] Friend Invistation request room:%d winguid:%d",
		gObj[aIndex].Name, lpMsg->RoomNumber, lpMsg->WindowGuid);
		*/
}





struct PMSG_ROOM_INVITATION_RESULT
{
	PBMSG_HEAD h;
	unsigned char Result;	// 3
	DWORD WindowGuid;	// 4
};


void FriendRoomInvitationRecv(FHP_FRIEND_INVITATION_RET* lpMsg)
{
	//#error
	/*
	char_ID Name(lpMsg->Name);

	if ( !gObjIsConnectedGP(lpMsg->Number, Name.GetBuffer()))
	{
		LogAdd("error-L2 : Index %s %d", __FILE__, __LINE__);
		return;
	}

	PMSG_ROOM_INVITATION_RESULT pMsg;

	pMsg.h.set((LPBYTE)&pMsg, 0xCB, sizeof(pMsg));
	pMsg.Result = lpMsg->Result;
	pMsg.WindowGuid = lpMsg->WindowGuid;

	DataSend(lpMsg->Number, (LPBYTE)&pMsg, sizeof(pMsg));

	LogAdd("[%s] Friend Invitation result :%d",
		Name.GetBuffer(), pMsg.Result);
		*/
}