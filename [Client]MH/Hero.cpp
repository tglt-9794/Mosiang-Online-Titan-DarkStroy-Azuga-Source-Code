// Hero.cpp: implementation of the CHero class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Hero.h"
#include "GameIn.h"
#include "MHCamera.h"
#include "ObjectManager.h"
#include "MoveManager.h"
#include "ObjectStateManager.h"
//#include "MinimapDialog.h"
#include "ItemManager.h"
#include "StatsCalcManager.h"
#include "SkillManager_Client.h"
#include "KyungGongManager.h"
#include "TacticManager.h"
#include "PartyManager.h"
#include "MugongManager.h"
#include "ObjectGuagen.h"
#include "./Audio/MHAudioManager.h"
#include "StreetStallManager.h"
#include "CheatMsgParser.h"
#include "interface/cWindowManager.h"
#include "OptionManager.h"
#include "WantedManager.h"
#include "ChatManager.h"
#include "TitanManager.h"

#include "MixDialog.h"
#include "ReinforceDlg.h"
#include "UpgradeDlg.h"
#include "DissolutionDialog.h"

#ifdef _DEBUG
#include "interface\cFont.h"
#include "ChatManager.h"
#endif


#include "WindowIdEnum.h"

#include "ReviveDialog.h"
#include "CharacterDialog.h"
#include "InventoryExDialog.h"
#include "DealDialog.h"
#include "CharStateDialog.h"

#include "mhMap.h"
#include "GameEventManager.h"

#include "BattleSystem_Client.h"
#include "NpcScriptDialog.h"
#include "CommonCalcFunc.h"
#include "AppearanceManager.h"

#include "AbilityInfo.h"

#include "SkillPointRedist.h"
#include "SkillPointNotify.h"
#include "SuryunDialog.h"
#include "AbilityManager.h"

#include "ChaseinputDialog.h"
#include "ShoutDialog.h"
#include "MoveDialog.h"

#include "QuickDialog.h"
#include "PeaceWarModeManager.h"
#include "GuageDialog.h"
#include "MussangManager.h"
#include "WeatherManager.h"
#include "PetManager.h"
#include "InventoryExDialog.h"
#include "PyoGukDialog.h"
#include "cMsgBox.h"

#include "EventMapInfo.h"
#include "SiegeWarMgr.h"
#include "PartyWar.h"
#include "GuildFieldWar.h"
#include "GuildManager.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

VECTOR3 gHeroPos;

CHero::CHero()
{
	m_NextAction.Clear();
	m_CurComboNum = 0;
	m_maxExpPoint = 0;
	m_PyogukNum = 0;	
	m_bUngiFlag = FALSE;
	m_AbilityGroup.SetOwenerObject(this);
}

CHero::~CHero()
{
	Release();
}

void CHero::InitHero(HERO_TOTALINFO * pHeroTotalInfo)
{	
	m_DelayGroup.Init();

	m_bSkillCooling = 0;
	m_MovingAction.Clear();
	m_NextAction.Clear();
	m_SkillStartAction.Clear();
	m_bIsAutoAttackMode = TRUE;
	m_bIsAutoAttacking = FALSE;
	m_bIsKyungGongMode = FALSE;

	memcpy(&m_HeroTotalInfo, pHeroTotalInfo, sizeof(HERO_TOTALINFO));
		
	m_maxExpPoint = GAMERESRCMNGR->GetMaxExpPoint(GetLevel());
	MUSSANGMGR->SetMussangReady(FALSE);
	MUSSANGMGR->SetMussangMaxPoint(pHeroTotalInfo->MaxMussangPoint);
//	m_HeroTotalInfo.KyungGongGrade = OPTIONMGR->GetKyungGongIdx();	//***
//	m_bIsKyungGongMode = OPTIONMGR->GetKyungGongMode();				//***

	//인터페이스에 적용
	//ApplyInterface(); RegisterHero로 옮김
	STATSMGR->Clear(&m_itemStats);
	HERO->ClearSetitemOption();		// 2007. 6. 18. CBH - 세트아이탬 능력치 구조체 초기화	

	m_dwLastSocietyAction = gCurTime;
	m_bActionPenalty = TRUE;
}


#define SKILL_PERSUIT_TICK	1000

void CHero::Process()
{
	static DWORD	dwSkillPersuitTime	= 0;

	CPlayer::Process();
	
	CAMERA->Process();	//? Gamin의 프로세스에 있다. KES 040406 //헉.. 여기서 안하니 카메라가 흔들린다.
	WEATHERMGR->Process();
	if(m_MovingAction.HasAction())
	{
		if(m_MovingAction.CheckTargetDistance(&GetCurPosition()) == TRUE)
		{
			MOVEMGR->HeroMoveStop();

			m_MovingAction.ExcuteAction(this);
			m_MovingAction.Clear();
		}
		else
		{
			if( m_MovingAction.GetActionKind() == eActionKind_Skill )
			{
				if( gCurTime - dwSkillPersuitTime > SKILL_PERSUIT_TICK )
				{
					m_MovingAction.ExcuteAction(this);
					dwSkillPersuitTime = gCurTime;
				}
			}
			else if( MOVEMGR->IsMoving(this) == FALSE )
			{
				m_MovingAction.Clear();
			}
			
//			if(MOVEMGR->IsMoving(this) == FALSE)
//			{
//				if(m_MovingAction.GetActionKind() == eActionKind_Skill)
//				{
//					m_MovingAction.ExcuteAction(this);
//				}
//				else
//					m_MovingAction.Clear();
//			}			
		}
		

	}
	else
	{
		if(m_bIsAutoAttacking)
		{
			if(m_NextAction.HasAction() == FALSE)
			{
				if(GetCurComboNum() < 2 || GetCurComboNum() == SKILL_COMBO_NUM)	// 자동공격은 콤보 2까지만	12/3일 회의 결과 3에서 2로 바뀜
				{
					if(SKILLMGR->OnSkillCommand(this,&m_AutoAttackTarget,FALSE) == FALSE)
						DisableAutoAttack();
				}
			}
		}
	}

	SKILLMGR->UpdateSkillObjectTargetList(this);

	// 경공
	if(m_bIsKyungGongMode)
	{
		if( MOVEMGR->IsMoving(this) &&
			m_MoveInfo.KyungGongIdx == 0 &&
			GetKyungGongGrade() &&
			GetMoveSpeed() != 0 &&
			GetState() != eObjectState_Ungijosik)			
		{
			if(m_MoveInfo.m_bEffectMoving == FALSE)
			{
				// RaMa - 05.09.05 - 경공딜레이가 없다.
				if( GetAvatarOption()->bKyungGong || GetShopItemStats()->bKyungGong )
					MOVEMGR->ToggleHeroKyungGongMode();
/*	//SW 벚꽃 이벤트 관련 임시 하드 코드
// 2005 크리스마스 이벤트 코드
				else if((WEATHERMGR->GetWeatherState() == eWS_Snow ) &&
					   ((GetShopItemStats()->Avatar[eAvatar_Dress] == EVENT_SHOPITEM_SNOWMAN_DRESS && GetShopItemStats()->Avatar[eAvatar_Hat] == EVENT_SHOPITEM_SNOWMAN_HAT) ||
						(GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_RUDOLP_DRESS && GetShopItemStats()->Avatar[eAvatar_Hat] ==  EVENT_SHOPITEM_RUDOLP_HAT) ||
						(GetShopItemStats()->Avatar[eAvatar_Dress] == EVENT_SHOPITEM_SNOWMAN_DRESS2 && GetShopItemStats()->Avatar[eAvatar_Hat] == EVENT_SHOPITEM_SNOWMAN_HAT2) ||
						(GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_RUDOLP_DRESS2 && GetShopItemStats()->Avatar[eAvatar_Hat] ==  EVENT_SHOPITEM_RUDOLP_HAT2) ||
						(GetShopItemStats()->Avatar[eAvatar_Dress] == EVENT_SHOPITEM_SNOWMAN_DRESS && GetShopItemStats()->Avatar[eAvatar_Hat] == EVENT_SHOPITEM_SNOWMAN_HAT2) ||
						(GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_RUDOLP_DRESS && GetShopItemStats()->Avatar[eAvatar_Hat] ==  EVENT_SHOPITEM_RUDOLP_HAT2) ||
						(GetShopItemStats()->Avatar[eAvatar_Dress] == EVENT_SHOPITEM_SNOWMAN_DRESS2 && GetShopItemStats()->Avatar[eAvatar_Hat] == EVENT_SHOPITEM_SNOWMAN_HAT) ||
						(GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_RUDOLP_DRESS2 && GetShopItemStats()->Avatar[eAvatar_Hat] ==  EVENT_SHOPITEM_RUDOLP_HAT) ||
						GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_SNOWMAN1_HK || GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_SNOWMAN2_HK ||
						GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_SNOWMAN3_HK || GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_RUDOLP1_HK ||
						GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_RUDOLP2_HK || GetShopItemStats()->Avatar[eAvatar_Dress] ==  EVENT_SHOPITEM_RUDOLP3_HK ))
*/
#define SHOPITEM_COS_CAT_HAT		56019
#define SHOPITEM_COS_CAT_DRESS		56020
#define SHOPITEM_COS_WEDDING_MAN	56021
#define SHOPITEM_COS_WEDDING_WOMAN	56022
				else if( (WEATHERMGR->GetWeatherState() == eWS_Snow ) &&
						( GetShopItemStats()->Avatar[eAvatar_Dress] ==  SHOPITEM_COS_CAT_DRESS ||
						GetShopItemStats()->Avatar[eAvatar_Dress] ==  SHOPITEM_COS_WEDDING_MAN ||
						GetShopItemStats()->Avatar[eAvatar_Dress] ==  SHOPITEM_COS_WEDDING_WOMAN )
						)
					MOVEMGR->ToggleHeroKyungGongMode();
				else
				{
					#ifdef TAIWAN_LOCAL
						MOVEMGR->ToggleHeroKyungGongMode();
					#else
						CKyungGongInfo* pKGInfo = KYUNGGONGMGR->GetKyungGongInfo(GetKyungGongGrade());
						DWORD ElapsedTime = gCurTime - m_ObjectState.State_Start_Time;
						if(ElapsedTime > pKGInfo->GetChangeTime())
						{
							MOVEMGR->ToggleHeroKyungGongMode();
						}
					#endif		// TAWAN_LOCAL
				}
			}
		}
	}

	// 스킬 쿨타임
	if(m_bSkillCooling)
	{
		if(gCurTime - m_SkillCoolTimeStart > m_SkillCoolTimeDuration)
			m_bSkillCooling = FALSE;
	}

	VECTOR3 pos;
	float angle;
	GetPosition(&pos);
	angle = GetAngle();

	// 그림자용 라이트 이동을 위해	
	MAP->SetShadowPivotPos(&pos);

	// 3D 사운드의 Listener 설정을 위해
	AUDIOMGR->SetListenerPosition(&pos,angle);
	
	// 죽었을때 일정한 시간이 지나면 창 띄운다.
//	if(GetState() == eObjectState_Die)
//	{	
//		if( gCurTime - GetStateStartTime() > PLAYERREVIVE_TIME ) //일정 시간만 들어가게
//		{
			//if( ) 비무로 죽었다. 아니다.
			//확인요망! confirm						//process가 느렸다면 NONE일 수도 있다.
//			if( BATTLESYSTEM->GetBattle(this)->GetBattleKind() == eBATTLE_KIND_NONE )
//			{
//				WANTEDMGR->SetActiveDialog();
//			}
//		}
//	}

	if(GetState() == eObjectState_SkillUsing)
	{
		if(gCurTime - GetStateStartTime() > 10000)	//숫자를 줄이기엔 진법부분이 걸린다.
		{
			OBJECTSTATEMGR->EndObjectState(this,eObjectState_SkillUsing);
			//////////////////////////////////////////////////////////////////////////
			// 06. 04. 스킬 버그 수정 - 이영준
			// 스킬 사용 상태를 강제로 종료할때 메세지를 못받아
			// 생성하지 못한 임시스킬객체를 지워준다.
			CSkillObject* pSObj = SKILLMGR->GetSkillObject(TEMP_SKILLOBJECT_ID);
			if(pSObj)
				SKILLMGR->DeleteTempSkill();
			//////////////////////////////////////////////////////////////////////////
		}
	}

	gHeroPos = pos;
	
	// Play Time 기록	
	m_HeroTotalInfo.Playtime  += gTickTime/1000;
	
	//SW050810 평화모드 자동 변환
	PEACEWARMGR->DoCheckForPeaceMode(this);

	//SW051129 Pet
	PETMGR->PetMgrProcess();

}

void CHero::GetHeroTotalInfo(HERO_TOTALINFO * info)
{
	ASSERT(info);
	memcpy(info, &m_HeroTotalInfo, sizeof(HERO_TOTALINFO));
}


void CHero::ApplyInterface()
{
#ifndef _TESTCLIENT_
	SetMoney(m_HeroTotalInfo.Money);
	SetLevel(m_CharacterInfo.Level);
	SetMaxLife(m_CharacterInfo.MaxLife);
	SetLife(m_CharacterInfo.Life, 0);
	SetMaxNaeRyuk(m_HeroTotalInfo.wMaxNaeRyuk);
	SetNaeRyuk(m_HeroTotalInfo.wNaeRyuk, 0);
	SetMaxShield(m_CharacterInfo.MaxShield);
	SetShield(m_CharacterInfo.Shield, 0);
	SetMaxExpPoint(m_maxExpPoint);

	SetExpPoint(m_HeroTotalInfo.ExpPoint);
	
	SetGenGol(m_HeroTotalInfo.wGenGol);
	SetMinChub(m_HeroTotalInfo.wMinChub);
	SetCheRyuk(m_HeroTotalInfo.wCheRyuk);
	SetSimMek(m_HeroTotalInfo.wSimMek);
	
	SetPartyIdx(m_HeroTotalInfo.PartyID);
	SetMunpaMemberRank(m_CharacterInfo.PositionInMunpa);
	SetMunpaID(m_CharacterInfo.MunpaID);
	SetGuildName(m_CharacterInfo.GuildName);

	SetGuageName(m_BaseObjectInfo.ObjectName);
	SetMunpa();
	SetBadFame(m_CharacterInfo.BadFame);

	SetStage(m_CharacterInfo.Stage);

	// RaMa
	GAMEIN->GetCharacterDialog()->RefreshPointInfo();//RefreshLevelupPoint();
	GAMEIN->GetSkPointDlg()->RefreshAbilityPoint();
#endif
}

void CHero::SetGuageName(char * szName)
{
	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGENAME);
	if( ps )	ps->SetStaticText( szName );
}

void CHero::SetMaxLife(DWORD maxlife)
{
	CPlayer::SetMaxLife(maxlife);

	DWORD newMaxLife = GetMaxLife();
	if(newMaxLife == 0)
	{
		newMaxLife = 1;
	}
	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGELIFE))->SetValue(
		(float)m_CharacterInfo.Life/(float)newMaxLife, 0 );

	// 인터페이스 게이지 변경
	GAMEIN->GetCharacterDialog()->SetLife(m_CharacterInfo.Life);

	// 인터페이스 : 수치 표시
	char szValue[50];
	sprintf( szValue, "%d / %d", GetLife(), newMaxLife );

	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGELIFETEXT);
	if( ps )	ps->SetStaticText( szValue );	
}


void CHero::SetMaxShield(DWORD maxShield)
{
	CPlayer::SetMaxShield(maxShield);

	DWORD newMaxShield = GetMaxShield();
	if(newMaxShield == 0)
	{
		newMaxShield = 1;
	}
	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGESHIELD))->SetValue(
		(float)m_CharacterInfo.Shield/(float)newMaxShield, 0 );

	// 인터페이스 게이지 변경
	GAMEIN->GetCharacterDialog()->SetShield(m_CharacterInfo.Shield);
	// 인터페이스 : 수치 표시
	char szValue[50];
	sprintf( szValue, "%d / %d", GetShield(), newMaxShield );

	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGESHIELDTEXT);
	if( ps )	ps->SetStaticText( szValue );
}
void CHero::Heal(CObject* pHealer,BYTE HealKind,DWORD HealVal)
{
	ChangeLife(HealVal);
}
void CHero::ChangeLife(int changeval)
{
	DWORD newlife = GetLife() + changeval;

	SetLife(newlife);
	
	if( m_bUngiFlag == FALSE )
	if( newlife )
	if( (GetMaxLife()/newlife) > 2 )
	{
		GAMEEVENTMGR->AddEvent(eGameEvent_Ability, 401);
		m_bUngiFlag = TRUE;
	}
}
void CHero::ChangeShield(int changeval)
{
	if(changeval < 0)
	{
		ASSERT((int)GetShield() >= (-changeval));
	}
	
	DWORD newShield = GetShield() + changeval;
	DWORD maxShield = GetMaxShield();

	SetShield(newShield);
}
void CHero::SetLife(DWORD life, BYTE type)
{
	CPlayer::SetLife(life);

	// 인터페이스 게이지 변경
	DWORD MaxLife = GetMaxLife();
	if(MaxLife == 0)
	{
		MaxLife = 1;
	}
	
	
	if(GetState() == eObjectState_Die)
		life = 0;

	//SW070127 타이탄
	// 타이탄 탑승상태에서 체력 30% 미만시 경고
	TITANMGR->CheckNotifyMsg(TNM_MASTER_LIFE);

	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGELIFE))->SetValue(
		(float)life/(float)MaxLife, 
		(type == 0 ? 0 : (2000/MaxLife)*life) );
	GAMEIN->GetCharacterDialog()->SetLife(life);
	// 인터페이스 : 수치 표시
	char szValue[50];
	sprintf( szValue, "%d / %d", life, GetMaxLife() );
	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGELIFETEXT);
	if( ps )	ps->SetStaticText( szValue );
}

void CHero::SetShield(DWORD Shield, BYTE type)
{
	CPlayer::SetShield(Shield);

	// 인터페이스 게이지 변경
	DWORD MaxShield = GetMaxShield();
	if(MaxShield == 0)
	{
		MaxShield = 1;
	}

	
	if(GetState() == eObjectState_Die)
		Shield = 0;

	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGESHIELD))->SetValue(
		(float)Shield/(float)MaxShield, 
		(type == 0 ? 0 : (2000/MaxShield)*Shield) );
	GAMEIN->GetCharacterDialog()->SetShield(Shield);
	// 인터페이스 : 수치 표시
	char szValue[50];
	sprintf( szValue, "%d / %d", Shield, GetMaxShield() );
	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGESHIELDTEXT);
	if( ps )	ps->SetStaticText( szValue );
}

void CHero::SetMoney(MONEYTYPE Money)
{
	m_HeroTotalInfo.Money = Money;
	if(GAMEIN->GetInventoryDialog())
		GAMEIN->GetInventoryDialog()->SetMoney(Money);
}

void CHero::SetKyungGongGrade(WORD grade)
{	
	m_HeroTotalInfo.KyungGongGrade = grade;
	
//	if(grade != 0)
//		OPTIONMGR->SetKyungGongIdx(HERO->GetKyungGongGrade());//***
	GAMEIN->GetCharacterDialog()->UpdateData();
}

void CHero::SetMaxExpPoint(EXPTYPE dwPoint)
{
	m_maxExpPoint = dwPoint;

	//흔벎랙君뇜0댄轎,鹿섟뎠된섬>=100,�薨뗀�駱槨40聾
	if (m_maxExpPoint == 0 || GetLevel()>= 100)
	{
		m_maxExpPoint = 4000000000;
	}

	double value = (double)GetExpPoint()/(double)m_maxExpPoint;

	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGEEXPPOINT))->SetValue( (GUAGEVAL)value, 0 );
	GAMEIN->GetCharacterDialog()->SetExpPointPercent( (float)(value*100) );
	
	// 인터페이스 : 수치 표시
	char szValue[50];
	sprintf( szValue, "%I64d / %I64d", GetExpPoint(), dwPoint );
	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGEEXPPOINTTEXT);
	if( ps )	ps->SetStaticText( szValue );
}

void CHero::SetExpPoint(EXPTYPE dwPoint, BYTE type)
{
	m_HeroTotalInfo.ExpPoint = dwPoint;

	// 인터페이스 게이지 변경
	double value = (double)dwPoint/(double)m_maxExpPoint;
	// magi82 //////////////////////////////////////////////////////////////////////////
	double value2 = value / 0.2f;
	double value3 = value2 - (EXPTYPE)value2;
	static double value4 = 0.0f;


	DWORD valueTime = (type == 0)? 0 : (DWORD)((2000/m_maxExpPoint)*dwPoint);

	// 경험치가 쌓일때마다 누적바 초기화
	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT1))->SetActive(FALSE);
	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT2))->SetActive(FALSE);
	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT3))->SetActive(FALSE);
	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT4))->SetActive(FALSE);

	if(GetLevel() == 99 || type == 0 )
		((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGEEXPPOINT))->SetValue( 1, 0 );
	else
	{
		ySWITCH((int)value2)
		yCASE(1)
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT1))->SetActive(TRUE);
		yCASE(2)
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT1))->SetActive(TRUE);
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT2))->SetActive(TRUE);
		yCASE(3)
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT1))->SetActive(TRUE);
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT2))->SetActive(TRUE);
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT3))->SetActive(TRUE);
		yCASE(4)
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT1))->SetActive(TRUE);
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT2))->SetActive(TRUE);
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT3))->SetActive(TRUE);
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(EXP_POINT4))->SetActive(TRUE);
		yENDSWITCH

		if((EXPTYPE)value4 == (EXPTYPE)value2)
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGEEXPPOINT))->SetValue( (GUAGEVAL)value3, valueTime );
		else
		{
			((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGEEXPPOINT))->SetValue( (GUAGEVAL)value3, 0 );
			value4 = value2;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////


	// 인터페이스 : 캐릭터 창
	CObjectGuagen* ps = (CObjectGuagen *)GAMEIN->GetCharacterDialog()->GetWindowForID(CI_EXPPOINTGUAGE);
	if( ps )
	{
		if(type == 0)
			ps->SetValue( (GUAGEVAL)value, 0 );
		else
			ps->SetValue( (GUAGEVAL)value, valueTime );
	}	

	GAMEIN->GetCharacterDialog()->SetExpPointPercent((float)(value*100));
	// 인터페이스 : 수치 표시

	char szValue[50];
	if(GetLevel() >= 121)//뎠된섬>=99珂,畇供俚,君瞳맣槨121섬
		sprintf( szValue, "[ %s ]", CHATMGR->GetChatMsg(179) );	//"[ 完 ]"
	else
		sprintf( szValue, "%.2f%% / %.2f%%", value3*100, value*100 );	// magi82

#ifdef _CHEATENABLE_	
	if(CHEATMGR->IsCheatEnable())
	{
		char buf1[66];
		wsprintf(buf1, " %I64d/%I64d", dwPoint, GetMaxExpPoint());
		strcat(szValue, buf1);
	}
#endif

	cStatic* ps2 = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGEEXPPOINTTEXT);
	if( ps2 )	ps2->SetStaticText( szValue );
}

void CHero::SetLevel(LEVELTYPE level)
{
	// 인터페이스 창 표시 수치 변경
	GAMEIN->GetCharacterDialog()->SetLevel(level);
	
	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGELEVEL);
	if( ps )	ps->SetStaticValue( level );

	// 레벨업시 이펙트표시
	if(level > CPlayer::GetLevel())
	{
		// magi82(4) - Titan(071023) 타이탄일때 이펙트 처리(레벨업)
		if(InTitan() == TRUE)
			EFFECTMGR->StartEffectProcess(eEffect_TitanLevelUpSentence,HERO,NULL,0,0);
		else
			EFFECTMGR->StartEffectProcess(eEffect_LevelUpSentence,HERO,NULL,0,0);
	}

	CPlayer::SetLevel(level);

	//색 변환
	ITEMMGR->RefreshAllItem();
	MUGONGMGR->RefreshMugong();
	if( GAMEIN->GetQuickDialog() )
		GAMEIN->GetQuickDialog()->RefreshIcon();

}

void CHero::SetMaxNaeRyuk(DWORD val)
{
	m_HeroTotalInfo.wMaxNaeRyuk = val;
	
	// 인터페이스 게이지 변경
	DWORD newMaxNaeRyuk = GetMaxNaeRyuk();
	if(newMaxNaeRyuk == 0)
	{
		newMaxNaeRyuk = 1;
	}
	((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGENAERYUK))->SetValue(
		(float)m_HeroTotalInfo.wNaeRyuk/(float)newMaxNaeRyuk, 0 );

	GAMEIN->GetCharacterDialog()->SetNaeRyuk(m_HeroTotalInfo.wNaeRyuk);

	// 인터페이스 : 수치 표시
	char szValue[50];
	sprintf( szValue, "%d / %d", GetNaeRyuk(), newMaxNaeRyuk );
	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGENAERYUKTEXT);
	if( ps )	ps->SetStaticText( szValue );
}

void CHero::SetNaeRyuk(DWORD val, BYTE type)
{
	if(val > GetMaxNaeRyuk()) 
	{
//		ASSERT(0);
		val = GetNaeRyuk();
	}
	
	// 인터페이스 창 표시 수치 변경
	DWORD dwMaxNaeRyuk = GetMaxNaeRyuk();
//	if( dwMaxNaeRyuk == 0 )
//		dwMaxNaeRyuk = 1;

	if(m_HeroTotalInfo.wMaxNaeRyuk != 0)
		((CObjectGuagen*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGENAERYUK))->SetValue(
		(float)val/(float)dwMaxNaeRyuk, 
		( type == 0 ? 0 : (1000/dwMaxNaeRyuk)*val) );

	GAMEIN->GetCharacterDialog()->SetNaeRyuk(val);

	// 인터페이스 : 수치 표시
	char szValue[50];
	sprintf( szValue, "%d / %d", val, dwMaxNaeRyuk );
	cStatic* ps = (cStatic *)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGENAERYUKTEXT);
	if( ps )	ps->SetStaticText( szValue );
	
	// 수치변경
	m_HeroTotalInfo.wNaeRyuk = val;
}
void CHero::SetGenGol(WORD val)
{
	m_HeroTotalInfo.wGenGol = val;

	//SW060906 펫작업으로 변경
	STATSMGR->CalcItemStats(this);
	//STATSMGR->CalcCharStats(this);
	
	// 인터페이스 창 표시 수치 변경
	GAMEIN->GetCharacterDialog()->SetGenGol();
	GAMEIN->GetCharacterDialog()->SetAttackRate();
	GAMEIN->GetCharacterDialog()->UpdateData();
}
void CHero::SetMinChub(WORD val)
{
	m_HeroTotalInfo.wMinChub = val;

	STATSMGR->CalcItemStats(this);
	//STATSMGR->CalcCharStats(this);
	

	// 인터페이스 창 표시 수치 변경
	GAMEIN->GetCharacterDialog()->SetMinChub();
	GAMEIN->GetCharacterDialog()->SetAttackRate();
	GAMEIN->GetCharacterDialog()->UpdateData();
}
void CHero::SetCheRyuk(WORD val)
{
	m_HeroTotalInfo.wCheRyuk = val;

	STATSMGR->CalcItemStats(this);
	//STATSMGR->CalcCharStats(this);
	
	// 인터페이스 창 표시 수치 변경
	GAMEIN->GetCharacterDialog()->SetCheRyuk();
	GAMEIN->GetCharacterDialog()->SetDefenseRate();
	GAMEIN->GetCharacterDialog()->UpdateData();
}
void CHero::SetSimMek(WORD val)
{
	m_HeroTotalInfo.wSimMek = val;

	STATSMGR->CalcItemStats(this);
	//STATSMGR->CalcCharStats(this);
	
	// 인터페이스 창 표시 수치 변경
	GAMEIN->GetCharacterDialog()->SetSimMek();
	GAMEIN->GetCharacterDialog()->UpdateData();
	GAMEIN->GetCharacterDialog()->SetShield(GetShield());
	GAMEIN->GetCharacterDialog()->SetNaeRyuk(GetNaeRyuk());
}
void CHero::SetMunpa()
{
	GAMEIN->GetCharacterDialog()->RefreshGuildInfo();
}

void CHero::SetFame(FAMETYPE val)
{
	m_HeroTotalInfo.Fame = val;
	GAMEIN->GetCharacterDialog()->SetFame(GetFame());
}

void CHero::SetBadFame( FAMETYPE val )
{
	m_CharacterInfo.BadFame = val;
	GAMEIN->GetCharacterDialog()->SetBadFame( val );	
}

BOOL CHero::StartSocietyAct( int nStartMotion, int nIngMotion, int nEndMotion, BOOL bHideWeapon )
{
	if( GetState() != eObjectState_None && GetState() != eObjectState_Immortal )
	{
		if( GetState() == eObjectState_Society )
		{
			if( m_ObjectState.State_End_Motion != -1 )	//앉기 동작의 경우
			{
				if( m_ObjectState.bEndState == TRUE )
					return FALSE;
				if( gCurTime - GetStateStartTime() < GetEngineObject()->GetAnimationTime(m_ObjectState.State_Start_Motion ) )
					return FALSE;

				OBJECTSTATEMGR->EndObjectState( this, GetState(), GetEngineObject()->GetAnimationTime( m_ObjectState.State_End_Motion ) );
				return TRUE;
			}
		}
		else
		{
			return FALSE;
		}
	}

	m_ObjectState.State_Start_Motion	= (short int)nStartMotion;
	m_ObjectState.State_Ing_Motion		= (short int)nIngMotion;
	m_ObjectState.State_End_Motion		= (short int)nEndMotion;

	if( nEndMotion != -1 )
		m_ObjectState.State_End_MotionTime = GetEngineObject()->GetAnimationTime(nEndMotion);
	else
		m_ObjectState.State_End_MotionTime = 0;


	if(m_ObjectState.State_Start_Motion != -1)
	{
		ChangeMotion( m_ObjectState.State_Start_Motion, FALSE );

		if( m_ObjectState.State_Ing_Motion != -1 )
			ChangeBaseMotion( m_ObjectState.State_Ing_Motion );
	}

	// magi82 - UniqueItem(071010) 권이 아닐때만 무기를 숨긴다.
	if( bHideWeapon && GetWeaponEquipType() != WP_GWUN)
		APPEARANCEMGR->HideWeapon( this );

	m_ObjectState.BeforeState		= m_BaseObjectInfo.ObjectState;
	m_BaseObjectInfo.ObjectState	= eObjectState_Society;
	m_ObjectState.State_Start_Time	= gCurTime;
	m_ObjectState.bEndState			= FALSE;
	
	if( m_ObjectState.State_Ing_Motion == -1 )
		OBJECTSTATEMGR->EndObjectState( this, eObjectState_Society, GetEngineObject()->GetAnimationTime( m_ObjectState.State_Start_Motion ) );

	return TRUE;
}

BOOL CHero::EndSocietyAct()
{
	if( m_ObjectState.State_Ing_Motion != -1 )
	{
		return FALSE;
	}
	return TRUE;
}

void CHero::StartSocietyActForQuest( int nStartMotion, int nIngMotion /* = -1 */, int nEndMotion /* = -1 */, BOOL bHideWeapon /* = FALSE  */ )
{
	m_ObjectState.State_Start_Motion = (short int)nStartMotion;
	m_ObjectState.State_Ing_Motion = (short int)nIngMotion;
	m_ObjectState.State_End_Motion = (short int)nEndMotion;

	if( nEndMotion != -1 )
		m_ObjectState.State_End_MotionTime = GetEngineObject()->GetAnimationTime(nEndMotion);
	else
		m_ObjectState.State_End_MotionTime = 0;

	if( m_ObjectState.State_Start_Motion != -1 )
	{
		ChangeMotion( m_ObjectState.State_Start_Motion, FALSE );
		if( m_ObjectState.State_Ing_Motion != -1 )
			ChangeBaseMotion( m_ObjectState.State_Ing_Motion );
	}

	if( bHideWeapon )
		APPEARANCEMGR->HideWeapon( this );
}

void CHero::OnStartObjectState(BYTE State)
{
	CPlayer::OnStartObjectState(State);
}
void CHero::OnEndObjectState(BYTE State)
{
	switch(State)
	{
	case eObjectState_Deal:
		{
			HERO->HeroStateNotify(eObjectState_Deal);
		}
		break;
		
	case eObjectState_SkillUsing:
	case eObjectState_SkillBinding:
		SKILLMGR->OnComboTurningPoint(this);
		break;
		
	case eObjectState_SkillSyn:
		{
#ifndef _TESTCLIENT_
			if(SKILLMGR->GetSkillObject(TEMP_SKILLOBJECT_ID) != NULL)	// 서버에서 응답이 안온경우
				OBJECTSTATEMGR->StartObjectState(this,eObjectState_SkillUsing);
			else				
				SKILLMGR->OnComboTurningPoint(this);
#else
			SKILLMGR->OnComboTurningPoint(this);
#endif
		}
		break;
		
	case eObjectState_Move:
		{
			if(PARTYMGR->GetTacticObjectID())
			{
				TACTICMGR->HeroTacticJoin();
			}
		}
		break;
	case eObjectState_SkillStart:
		{
			if(m_SkillStartAction.GetActionKind() != eActionKind_Skill_RealExecute)
				break;
			
			m_SkillStartAction.ExcuteAction(this);
			m_SkillStartAction.Clear();
		}
		break;
	}
	CPlayer::OnEndObjectState(State);
}

//////////////////////////////////////////////////////////////////////////
// 행동 함수들..
void CHero::Die(CObject* pAttacker,BOOL bFatalDamage,BOOL bCritical, BOOL bDecisive)
{
//	ASSERT(GetLife() == 0);
	
	WINDOWMGR->CloseAllMsgBox();

	GAMEIN->GetDealDialog()->SetDisable(FALSE);
	GAMEIN->GetDealDialog()->SetActive(FALSE);	// 상점창 닫기
	
	GAMEIN->GetNpcScriptDialog()->SetDisable(FALSE);	//NPC창도 닫기
	GAMEIN->GetNpcScriptDialog()->SetActive(FALSE);

	//강화 분해 등급업창 닫기 -- 루팅당할 수 도 있으니..

	if( GAMEIN->GetUpgradeDialog()->IsActive() &&
		!GAMEIN->GetUpgradeDialog()->IsDisable() )
	{
		GAMEIN->GetUpgradeDialog()->Release();
		GAMEIN->GetUpgradeDialog()->SetActiveRecursive( FALSE );
	}
	if( GAMEIN->GetMixDialog()->IsActive() &&
		!GAMEIN->GetMixDialog()->IsDisable() )
	{
		GAMEIN->GetMixDialog()->Release();
		GAMEIN->GetMixDialog()->SetActiveRecursive( FALSE );
	}
	if( GAMEIN->GetReinforceDialog()->IsActive() &&
		!GAMEIN->GetReinforceDialog()->IsDisable() )
	{
		GAMEIN->GetReinforceDialog()->Release();
		GAMEIN->GetReinforceDialog()->SetActiveRecursive( FALSE );
	}
	if( GAMEIN->GetDissolutionDialog()->IsActive() &&
		!GAMEIN->GetDissolutionDialog()->IsDisable() )
	{
		GAMEIN->GetDissolutionDialog()->SetActive( FALSE );
	}
	//
	if( GAMEIN->GetSkPointDlg()->IsActive() )
	{
		GAMEIN->GetSkPointDlg()->SetActive( FALSE );
	}

	SetMovingAction(NULL);
	DisableAutoAttack();					// 오토공격 끄기
	SetLife(0);
	
// LBS 03.10.04 죽었을때 노점상을 닫는다.	//여기서? 죽으면 맵서버에서 취소해주는 것이 좋을텐데?
	STREETSTALLMGR->CloseStreetStall();
	
	if( pAttacker )
	if(pAttacker->GetObjectKind() & eObjectKind_Monster )
	{
//		GAMEEVENTMGR->AddEvent(eGameEvent1_HeroDie);
		if( GetLevel() < 5 )
			GAMEEVENTMGR->AddEvent(eGameEvent_Die);
		else
			GAMEEVENTMGR->AddEvent(eGameEvent_DieAfter5);
	}

	if(GAMEIN->GetCharStateDialog())
		GAMEIN->GetCharStateDialog()->SetUngiMode( FALSE );

	DWORD time = 0;
	if( bFatalDamage )
		time = HERO->GetEngineObject()->GetAnimationTime(eMotion_Died_Fly);
	else
		time = HERO->GetEngineObject()->GetAnimationTime(eMotion_Died_Normal);
	CAMERA->SetCharState( eCS_Die, time );

	CPlayer::Die(pAttacker,bFatalDamage,bCritical,bDecisive);
	
	GAMEIN->GetCharacterDialog()->SetAttackRate();
	GAMEIN->GetCharacterDialog()->SetDefenseRate();
	GAMEIN->GetCharacterDialog()->UpdateData();

	// magi82 - SOS(070719)
	// magi82(36)
	// 사용중인 스킬이 공격효과 타겟 타입(NegativeResultTargetType)이거나 방어효과 타겟 타입(PositiveResultTargetType)이면
	// 타겟 리스트에 자기 자신도 추가가 되는데(각 효과를 적용 시키기 위해서) 그 순간 죽으면
	// Attacker가 자기 자신으로 된다.
	// 그런 경우를 방지하기 위해 Attacker가 자기 자신인지 체크해야한다.
	if( HEROID != pAttacker->GetID() )
	{
		if(pAttacker->GetObjectKind() == eObjectKind_Player)
		{
			CBattle* pTargetBattle = BATTLESYSTEM->GetBattle( HERO );
			CBattle* pAttackerBattle = BATTLESYSTEM->GetBattle( pAttacker->GetID() );
			WORD MapNum = MAP->GetMapNum();
			DWORD dwCheckBit = eBossMap | eEventMap | eSurvivalMap | eTournament;

			if(HERO->GetMunpaIdx())
			{
				if( HERO->IsPKMode() == TRUE
					|| (pTargetBattle->GetBattleKind() == eBATTLE_KIND_VIMUSTREET && pTargetBattle->GetBattleID() == pAttackerBattle->GetBattleID())
					|| (PARTYWAR->IsPartyWar() && PARTYWAR->IsEnemy((CPlayer*)pAttacker))
					|| GUILDFIELDWAR->IsEnemy((CPlayer*)pAttacker)
					|| MapNum == SIEGEMGR->GetSiegeWarMapNum()
					|| MAP->IsMapKind(dwCheckBit) )
					//|| MapNum == BOSSMONSTER_MAP
					//|| MapNum == BOSSMONSTER_2NDMAP || MapNum == RUNNINGMAP
					//|| MapNum == PKEVENTMAP || MAP->CheckMapKindIs(eSurvival)
					//|| EVENTMAP->IsEventMap() || MapNum == Tournament )
				{
					int a = 0;
				}
				else
				{
					WINDOWMGR->MsgBox( MBI_SOS, MBT_YESNO, CHATMGR->GetChatMsg(1633) );
				}
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////

	SKILLMGR->RemoveTarget(this, SKILLRESULTKIND_POSITIVE|SKILLRESULTKIND_NEGATIVE);

	if( m_bActionPenalty == FALSE )
	{
		WINDOWMGR->MsgBox( MBI_ACTION_PENALTY, MBT_OK, CHATMGR->GetChatMsg(1606) );
		m_bActionPenalty = TRUE;
	}
}

void CHero::Damage(CObject* pAttacker,BYTE DamageKind,DWORD Damage,DWORD ShieldDamage,BOOL bCritical, BOOL bDecisive, DWORD titanObserbDamage)
{
	//////////////////////////////////////////////////////////////////////////
	// 카메라 뒤로 밀림
	VECTOR3 push;
	MATRIX4 my;
	SetVector3(&push,0,0,30);
	SetRotationYMatrix(&my,-m_RotateInfo.Angle.ToRad());
	TransformVector3_VPTR2(&push,&push,&my,1);
	CAMERA->SetPushedVibration(&push,200);
	//////////////////////////////////////////////////////////////////////////
	
	CPlayer::Damage(pAttacker,DamageKind,Damage,ShieldDamage,bCritical,bDecisive,titanObserbDamage);
	
}

void CHero::OnHitTarget(CObject* pMainTarget)
{
	//////////////////////////////////////////////////////////////////////////
	// 주인공과 데미지 입는 메인타겟과 가까우면 조금씩 떨어트림	
	VECTOR3 targetpos = pMainTarget->GetCurPosition();
	VECTOR3 Curpos = HERO->GetCurPosition();

	float WeaponMinDistance;
	switch(GetWeaponEquipType())
	{
	case WP_GUM:	WeaponMinDistance = 150;	break;
	case WP_GWUN:	WeaponMinDistance = 100;	break;
	case WP_DO:		WeaponMinDistance = 150;	break;
	case WP_CHANG:	WeaponMinDistance = 150;	break;
	case WP_GUNG:	WeaponMinDistance = 100;	break;
	case WP_AMGI:	WeaponMinDistance = 100;	break;	
	default: ASSERT(0);
	}

	if(CalcDistanceXZ(&targetpos,&Curpos) < WeaponMinDistance)
	{
		VECTOR3 pos;	pos.x = 0;	pos.y = 0;	pos.z = 10.f;
		float angle = HERO->GetAngle();
		TransToRelatedCoordinate(&pos,&pos,angle);
		pos = pos + Curpos;
		MOVEMGR->HeroEffectMove(HERO,&pos,0,0);
	}
}


void CHero::ClearTarget(CObject* pObject)
{
	if(m_AutoAttackTarget.GetTargetID() == pObject->GetID())
		DisableAutoAttack();
	if(m_NextAction.GetTargetID() == pObject->GetID())
	{
		m_NextAction.Clear();
	}
}


void CHero::Revive(VECTOR3* pRevivePos)
{
	DisableAutoAttack();

	CAMERA->SetCharState( eCS_Normal, 0 );	//일어나는 동작이 없다.
	
	CPlayer::Revive(pRevivePos);
}

BOOL CHero::GetKyungGongMode()
{
	return m_bIsKyungGongMode;
}

void CHero::SetKyungGongMode(BOOL bMode)
{
	m_bIsKyungGongMode = bMode;

	if( m_bIsKyungGongMode == FALSE )
	{
		if(MOVEMGR->IsMoving(this) && m_MoveInfo.KyungGongIdx != 0)
			MOVEMGR->ToggleHeroKyungGongMode();
	}
	else
	{
		if(MOVEMGR->IsMoving(this) && m_MoveInfo.KyungGongIdx != 0)
			MOVEMGR->ToggleHeroKyungGongMode();
	}

//KES CHARSTATEDLG 031016
	if(GAMEIN->GetCharStateDialog())
		GAMEIN->GetCharStateDialog()->SetKyungGongMode( m_bIsKyungGongMode );
//
	
//	OPTIONMGR->SetKyungGongMode(m_bIsKyungGongMode);//***
}


int CHero::GetMugongLevel(WORD MugongIdx)
{
	//if(MugongIdx < 100)		// 콤보
	//	return 1;

	// !!!!!!!!!!! magi82 - 원래는 타이탄 콤보를 따로 만들어야하지만 지금은 급해서 일단 이렇게 임시로 씀 !!!!!!!!!!!!!!!!!!1
	if(MugongIdx < 100 || ( 10000 <= MugongIdx && MugongIdx < 10000 + 100 ))		// 콤보
		return 1;

	CMugongBase* pMugong = MUGONGMGR->GetMugongByMugongIdx(MugongIdx);
	if(pMugong == NULL)
		return 0;

	return pMugong->GetSung();
}

//////////////////////////////////////////////////////////////////////////
// 06. 06. 2차 전직 - 이영준
// 무공 변환 추가
WORD CHero::GetSkillOptionIndex(WORD MugongIdx)
{
	if(MugongIdx < 100)		// 콤보
		return 0;

	//if( MugongIdx > SKILLNUM_TO_TITAN && InTitan() )
	//	MugongIdx -= SKILLNUM_TO_TITAN;

	CMugongBase* pMugong = MUGONGMGR->GetMugongByMugongIdx(MugongIdx);
	if(pMugong == NULL)
		return 0;

	return pMugong->GetOption();
}
//////////////////////////////////////////////////////////////////////////

void CHero::AddStatus(CStatus* pStatus)
{
	CObject::AddStatus(pStatus);
	
	MOVEMGR->RefreshHeroMove();

	SetMaxLife(m_CharacterInfo.MaxLife);
	SetMaxShield(m_CharacterInfo.MaxShield);
	SetMaxNaeRyuk(m_HeroTotalInfo.wMaxNaeRyuk);
	
	GAMEIN->GetCharacterDialog()->SetAttackRate();
	GAMEIN->GetCharacterDialog()->SetDefenseRate();
	GAMEIN->GetCharacterDialog()->UpdateData();
}
void CHero::RemoveStatus(CStatus* pStatus)
{
	CObject::RemoveStatus(pStatus);

	MOVEMGR->RefreshHeroMove();
/*

	((cGuage*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGENAERYUK))->SetMaxValue(GetMaxNaeRyuk());
	GAMEIN->GetCharacterDialog()->SetNaeRyuk(m_HeroTotalInfo.wNaeRyuk);
	((cGuage*)GAMEIN->GetGuageDlg()->GetWindowForID(CG_GUAGELIFE))->SetMaxValue(GetMaxLife());
	GAMEIN->GetCharacterDialog()->SetLife(m_CharacterInfo.Life);
*/

	GAMEIN->GetCharacterDialog()->SetAttackRate();
	GAMEIN->GetCharacterDialog()->SetDefenseRate();
	GAMEIN->GetCharacterDialog()->UpdateData();
}

DWORD CHero::DoGetPhyAttackPowerMin()
{
	const player_calc_stats * pChar_stats = GetCharStats();
	const player_calc_stats * pItem_stats = GetItemStats();
	const SET_ITEM_OPTION* pSetItem_Stats = GetSetItemStats(); //2007. 6. 13. CBH - 세트아이탬 능력치 추가

	WORD WeaponKind = GetWeaponEquipType();
	if(WeaponKind == WP_GUNG || WeaponKind == WP_AMGI)
	{
		WORD MinChub = (WORD)(pChar_stats->MinChub + GetAbilityStats()->StatMin + pItem_stats->MinChub + GetShopItemStats()->Minchub + GetAvatarOption()->Minchub + pSetItem_Stats->wMinChub);

#ifdef _JAPAN_LOCAL_
		
		WORD RareWeapon = pItem_stats->RangeAttackPowerMin;

		if( PARTYMGR->IsHelpPartyMember() && BATTLESYSTEM->GetBattle(this)->GetBattleKind() != eBATTLE_KIND_VIMUSTREET )
			RareWeapon = RareWeapon + RareWeapon * 20 / 100;

		WORD WeaponAttack = RareWeapon
						  * ( 1.f + pItem_stats->RangeAttackPowerAddRateMin )
						  + GetAvatarOption()->Attack;

		DWORD dwPhyAttackPowerMin = CalcRangeAttackPower( MinChub, WeaponAttack )
								  + GetLevel() * 4
								  + GetAbilityStats()->GetPhyAttackUp( WeaponKind );

		return dwPhyAttackPowerMin;
#else
		return (DWORD)((CalcRangeAttackPower(MinChub,pItem_stats->RangeAttackPowerMin+GetAvatarOption()->Attack+pSetItem_Stats->wRangeAttackMin)
			+ GetAbilityStats()->GetPhyAttackUp(WeaponKind))
			* (1+GetItemStats()->RangeAttackPowerAddRateMin));
#endif
	}
	else
	{
		WORD GenGol = (WORD)(pChar_stats->GenGol + GetAbilityStats()->StatGen + pItem_stats->GenGol + GetShopItemStats()->Gengol + GetAvatarOption()->Gengol + pSetItem_Stats->wGenGol);
		WORD WeaponItemIdx = GetWearedItemIdx(eWearedItem_Weapon);
		WORD DefaultPower = 0;
		if( WeaponItemIdx == 0 )
			DefaultPower = 5;
		
#ifdef _JAPAN_LOCAL_

		WORD RareWeapon = pItem_stats->MeleeAttackPowerMin + DefaultPower;

		if( PARTYMGR->IsHelpPartyMember() && BATTLESYSTEM->GetBattle(this)->GetBattleKind() != eBATTLE_KIND_VIMUSTREET )
			RareWeapon = RareWeapon + RareWeapon * 20 / 100;

		WORD WeaponAttack = RareWeapon
							* ( 1.f + pItem_stats->MeleeAttackPowerAddRateMin )
							+ GetAvatarOption()->Attack;

		DWORD dwPhyAttackPowerMin = CalcMeleeAttackPower( GenGol, WeaponAttack )
								  + GetLevel() * 4
								  + GetAbilityStats()->GetPhyAttackUp( WeaponKind );

		return dwPhyAttackPowerMin;
#else
		return (DWORD)((CalcMeleeAttackPower(GenGol,pItem_stats->MeleeAttackPowerMin+DefaultPower+GetAvatarOption()->Attack+pSetItem_Stats->wMeleeAttackMin)
			+ GetAbilityStats()->GetPhyAttackUp(WeaponKind))
			* (1+GetItemStats()->MeleeAttackPowerAddRateMin));
#endif
	}
}
DWORD CHero::DoGetPhyAttackPowerMax()
{
	const player_calc_stats * pChar_stats = GetCharStats();
	const player_calc_stats * pItem_stats = GetItemStats();
	const SET_ITEM_OPTION* pSetItem_Stats = GetSetItemStats(); //2007. 6. 13. CBH - 세트아이탬 능력치 추가

	WORD WeaponKind = GetWeaponEquipType();
	if(WeaponKind == WP_GUNG || WeaponKind == WP_AMGI)
	{
		WORD MinChub = (WORD)(pChar_stats->MinChub + GetAbilityStats()->StatMin + pItem_stats->MinChub + GetShopItemStats()->Minchub + GetAvatarOption()->Minchub + pSetItem_Stats->wMinChub);
#ifdef _JAPAN_LOCAL_

		WORD RareWeapon = pItem_stats->RangeAttackPowerMax;

		if( PARTYMGR->IsHelpPartyMember() && BATTLESYSTEM->GetBattle(this)->GetBattleKind() != eBATTLE_KIND_VIMUSTREET )
			RareWeapon = RareWeapon + RareWeapon * 20 / 100;

		WORD WeaponAttack = RareWeapon
						  * ( 1.f + pItem_stats->RangeAttackPowerAddRateMax )
						  + GetAvatarOption()->Attack;

		DWORD dwPhyAttackPowerMax = CalcRangeAttackPower( MinChub, WeaponAttack )
								  + GetLevel() * 4
								  + GetAbilityStats()->GetPhyAttackUp( WeaponKind );

		return dwPhyAttackPowerMax;
#else
		return (DWORD)((CalcRangeAttackPower(MinChub,pItem_stats->RangeAttackPowerMax+GetAvatarOption()->Attack+pSetItem_Stats->wRangeAttackMax)
			+ GetAbilityStats()->GetPhyAttackUp(WeaponKind))
			* (1+GetItemStats()->RangeAttackPowerAddRateMax));
#endif
	}
	else
	{
		WORD GenGol = (WORD)(pChar_stats->GenGol + GetAbilityStats()->StatGen + pItem_stats->GenGol + GetShopItemStats()->Gengol + GetAvatarOption()->Gengol + pSetItem_Stats->wGenGol);

		WORD WeaponItemIdx = GetWearedItemIdx(eWearedItem_Weapon);
		WORD DefaultPower = 0;
		if( WeaponItemIdx == 0 )
			DefaultPower = 5;
#ifdef _JAPAN_LOCAL_

		WORD RareWeapon = pItem_stats->MeleeAttackPowerMax+DefaultPower;

		if( PARTYMGR->IsHelpPartyMember() && BATTLESYSTEM->GetBattle(this)->GetBattleKind() != eBATTLE_KIND_VIMUSTREET )
			RareWeapon = RareWeapon + RareWeapon * 20 / 100;

		WORD WeaponAttack = RareWeapon
							* ( 1.f + pItem_stats->MeleeAttackPowerAddRateMax )
							+ GetAvatarOption()->Attack;

		DWORD dwPhyAttackPowerMax = CalcMeleeAttackPower( GenGol, WeaponAttack )
								  + GetLevel() * 4
								  + GetAbilityStats()->GetPhyAttackUp( WeaponKind );

		return dwPhyAttackPowerMax;
#else		
		return (DWORD)((CalcMeleeAttackPower(GenGol,pItem_stats->MeleeAttackPowerMax+DefaultPower+GetAvatarOption()->Attack+pSetItem_Stats->wMeleeAttackMax)
			+ GetAbilityStats()->GetPhyAttackUp(WeaponKind))
			* (1+GetItemStats()->MeleeAttackPowerAddRateMax));
#endif
	}
}

/* 종료해야할 상태값을 서버에도 알린다. */
void CHero::HeroStateNotify(BYTE EndState)
{
	MSG_BYTE msg;
	msg.Category = MP_CHAR;
	msg.Protocol = MP_CHAR_STATE_NOTIFY;
	msg.dwObjectID = GetID();
	msg.bData = EndState;

	NETWORK->Send(&msg, sizeof(msg));
}

BYTE CHero::GetNaeRyukPercent()
{
	return (BYTE)((GetNaeRyuk()*100) / GetMaxNaeRyuk());
}

DWORD CHero::DoGetMaxNaeRyuk()
{
	return m_HeroTotalInfo.wMaxNaeRyuk;
}

void CHero::ClearGuildInfo()
{
//	SafeStrCpy(m_HeroTotalInfo.MunpaName, "", MAX_GUILD_NAME+1);
	CPlayer::ClearGuildInfo();
}

char* CHero::GetGuildEntryDate()
{
	return m_HeroTotalInfo.MunpaCanEntryDate;
}

void CHero::SetGuildEntryDate(char* day)
{
	SafeStrCpy(m_HeroTotalInfo.MunpaCanEntryDate, day, 11);
}

void CHero::CalcShopItemOption( WORD wIdx, BOOL bAdd, DWORD Param )
{
	if( wIdx == 0 )
		return;

	ITEM_INFO* pItemInfo = ITEMMGR->GetItemInfo( wIdx );
	if(!pItemInfo)
	{
		char buf[256]= { 0, };
		sprintf(buf, "S ItemInfo가 없다. PlayerIdx[%d] Idx[%d]", GetID(), wIdx);
		ASSERTMSG(0, buf);
		return;
	}

	int calc = -1;
	if(bAdd)	calc=1;

	//////////////////////////////////////////////////////
	// 06. 07 내공 적중(일격) - 이영준
	if( pItemInfo->NaeRyukRecover )
	{
		m_ShopItemOption.Decisive += (pItemInfo->NaeRyukRecover*calc);
		if(m_ShopItemOption.Decisive <= 0)
			m_ShopItemOption.Decisive=0;
	}
	//////////////////////////////////////////////////////

	if( pItemInfo->ItemKind == eSHOP_ITEM_PREMIUM )
	{
		// 경험치   GenGol
		if( pItemInfo->GenGol>0 )
		{
			m_ShopItemOption.AddExp += (pItemInfo->GenGol*calc);
			if(m_ShopItemOption.AddExp <= 0)
				m_ShopItemOption.AddExp=0;
			
		}
		// 아이템 드랍확률
		if( pItemInfo->MinChub>0 )
		{
			m_ShopItemOption.AddItemDrop += (pItemInfo->MinChub*calc);
			if(m_ShopItemOption.AddItemDrop <= 0)
				m_ShopItemOption.AddItemDrop=0;
		}
		// 죽었을때 경험치와 돈 패널티 없는것		
	}
	else if( pItemInfo->ItemKind == eSHOP_ITEM_INCANTATION )
	{
		// 조합확률
		/*if( pItemInfo->LimitJob )
		{
			m_ShopItemOption.ItemMixSuccess = (char)pItemInfo->LimitJob;
			if(m_ShopItemOption.ItemMixSuccess < 0)
				m_ShopItemOption.ItemMixSuccess=0;
		}		
		else */
		if( pItemInfo->GenGol )
		{
			if( bAdd )
			{
				m_ShopItemOption.StatePoint += pItemInfo->GenGol;
				GAMEIN->GetCharacterDialog()->RefreshPointInfo();
			}
		}
		else if( pItemInfo->Life )
		{
			m_ShopItemOption.SkillPoint += pItemInfo->Life;
			GAMEIN->GetSkPointDlg()->RefreshAbilityPoint();
			GAMEIN->GetSuryunDialog()->RedistBtnActive();			
			GAMEIN->GetSkPointNotifyDlg()->SetActive( TRUE );
		}
		else if( pItemInfo->CheRyuk )
		{
			if( bAdd )
				m_ShopItemOption.ProtectCount = (char)pItemInfo->CheRyuk;
			else
				m_ShopItemOption.ProtectCount = (char)Param;
		}
		else if( ( pItemInfo->ItemIdx == eIncantation_Tracking ||
				   pItemInfo->ItemIdx == eIncantation_Tracking7 ||
				   pItemInfo->ItemIdx == eIncantation_Tracking7_NoTrade ) && bAdd )
		{
			//if( MAP->GetMapNum() == RUNNINGMAP || MAP->GetMapNum() == PKEVENTMAP )
			if( MAP->IsMapSame(eRunningMap) || MAP->GetMapNum() == PKEVENTMAP )
			{
				CHATMGR->AddMsg( CTC_SYSMSG, CHATMGR->GetChatMsg(1457) );
				return;
			}

			GAMEIN->GetChaseinputDlg()->SetActive( TRUE );
			GAMEIN->GetChaseinputDlg()->SetItemIdx( pItemInfo->ItemIdx );
		}
		else if( pItemInfo->ItemIdx == eIncantation_ChangeName && bAdd )
		{
/*			GAMEIN->GetNameChangeNotifyDlg()->SetActive( TRUE );
			GAMEIN->GetInventoryDialog()->SetDisable( TRUE );*/
		}
		else if( pItemInfo->LimitJob )			// 레벨제한해제 주문서
		{
			if( bAdd )
				m_ShopItemOption.EquipLevelFree += (BYTE)pItemInfo->LimitJob;
			else
			{
				m_ShopItemOption.EquipLevelFree -= (BYTE)pItemInfo->LimitJob;
			}
			STATSMGR->CalcItemStats( HERO );
			GAMEIN->GetCharacterDialog()->RefreshInfo();
			ITEMMGR->RefreshAllItem();
		}
		else if( pItemInfo->ItemIdx == eIncantation_MugongExtend && bAdd )
		{
#ifdef _JAPAN_LOCAL_
			 ++m_CharacterInfo.ExtraMugongSlot;
#elif defined _HK_LOCAL_
			 ++m_CharacterInfo.ExtraMugongSlot;
#elif defined _TL_LOCAL_
			 ++m_CharacterInfo.ExtraMugongSlot;
#else
			GAMEIN->GetMugongDialog()->SetMugongExtend( TRUE );
#endif
			if( GAMEIN->GetMugongDialog()->IsActive() )
				GAMEIN->GetMugongDialog()->SetActive( TRUE );
		}
		else if( pItemInfo->ItemIdx == eIncantation_MemoryMoveExtend || pItemInfo->ItemIdx == eIncantation_MemoryMoveExtend7 || pItemInfo->ItemIdx == eIncantation_MemoryMove2 ||
			     pItemInfo->ItemIdx == eIncantation_MemoryMoveExtend30 )
		{
			GAMEIN->GetMoveDialog()->SetExtend( bAdd );
		}
#ifdef _JAPAN_LOCAL_
		else if( pItemInfo->ItemIdx == eIncantation_InvenExtend )
		{
			if( bAdd )
			{
				m_CharacterInfo.ExtraInvenSlot += 1;
				if( GAMEIN->GetInventoryDialog()->IsActive() )
					GAMEIN->GetInventoryDialog()->SetActive( TRUE );
			}
		}
		else if( pItemInfo->ItemIdx == eIncantation_PyogukExtend )
		{
			if( bAdd )
			{
				m_CharacterInfo.ExtraPyogukSlot += 1;
				CHATMGR->AddMsg( CTC_SYSMSG, CHATMGR->GetChatMsg(1302) );
			}
		}
#endif
#ifdef _HK_LOCAL_
		else if( pItemInfo->ItemIdx == eIncantation_InvenExtend )
		{
			if( bAdd )
			{
				m_CharacterInfo.ExtraInvenSlot += 1;
				if( GAMEIN->GetInventoryDialog()->IsActive() )
					GAMEIN->GetInventoryDialog()->SetActive( TRUE );
			}
		}
		else if( pItemInfo->ItemIdx == eIncantation_PyogukExtend )
		{
			if( bAdd )
			{
				m_CharacterInfo.ExtraPyogukSlot += 1;
				CHATMGR->AddMsg( CTC_SYSMSG, CHATMGR->GetChatMsg(1302) );
			}
		}
#endif
#ifdef _TL_LOCAL_
		else if( pItemInfo->ItemIdx == eIncantation_InvenExtend )
		{
			if( bAdd )
			{
				m_CharacterInfo.ExtraInvenSlot += 1;
				if( GAMEIN->GetInventoryDialog()->IsActive() )
					GAMEIN->GetInventoryDialog()->SetActive( TRUE );
			}
		}
		else if( pItemInfo->ItemIdx == eIncantation_PyogukExtend )
		{
			if( bAdd )
			{
				m_CharacterInfo.ExtraPyogukSlot += 1;
				CHATMGR->AddMsg( CTC_SYSMSG, CHATMGR->GetChatMsg(1302) );
			}
		}
#endif
	}
	else if( pItemInfo->ItemKind == eSHOP_ITEM_CHARM )
	{
		if( pItemInfo->GenGol>0 )
		{
			m_ShopItemOption.Gengol += (pItemInfo->GenGol*calc);
			if(m_ShopItemOption.Gengol <= 0)
				m_ShopItemOption.Gengol=0;

			STATSMGR->CalcItemStats(HERO);
			GAMEIN->GetCharacterDialog()->SetGenGol();
			GAMEIN->GetCharacterDialog()->SetAttackRate();
		}
		if( pItemInfo->MinChub>0 )
		{
			m_ShopItemOption.Minchub += (pItemInfo->MinChub*calc);
			if(m_ShopItemOption.Minchub <= 0)
				m_ShopItemOption.Minchub=0;

			STATSMGR->CalcItemStats(HERO);
			GAMEIN->GetCharacterDialog()->SetMinChub();
			GAMEIN->GetCharacterDialog()->SetAttackRate();
		}
		if( pItemInfo->CheRyuk>0 )
		{		
			m_ShopItemOption.Cheryuk += (pItemInfo->CheRyuk*calc);
			if(m_ShopItemOption.Cheryuk <= 0)
				m_ShopItemOption.Cheryuk=0;

			STATSMGR->CalcItemStats(this);

			GAMEIN->GetCharacterDialog()->SetCheRyuk();
			GAMEIN->GetCharacterDialog()->SetDefenseRate();
		}
		if( pItemInfo->SimMek>0 )
		{
			m_ShopItemOption.Simmek += (pItemInfo->SimMek*calc);
			if(m_ShopItemOption.Simmek <= 0)
				m_ShopItemOption.Simmek=0;

			STATSMGR->CalcItemStats(HERO);
			GAMEIN->GetCharacterDialog()->SetSimMek();
		}
		// 내공데미지   Life
		if( pItemInfo->Life>0 )
		{
			m_ShopItemOption.NeagongDamage += (char)(pItemInfo->Life*calc);
			if(m_ShopItemOption.NeagongDamage <= 0)
				m_ShopItemOption.NeagongDamage=0;
		}
		// 외공대미지   Shield
		if( pItemInfo->Shield>0 )
		{
			m_ShopItemOption.WoigongDamage += (char)(pItemInfo->Shield*calc);
			if(m_ShopItemOption.WoigongDamage <= 0)
				m_ShopItemOption.WoigongDamage=0;
		}
		// 무공특기치 1씩  Plus_MugongIdx
		if( pItemInfo->NaeRyuk>0 )
		{
			m_ShopItemOption.AddSung += (pItemInfo->NaeRyuk*calc);
			if( m_ShopItemOption.AddSung < 0 )
				m_ShopItemOption.AddSung = 0;
		}
		// 기본데미지  Plus_Value
		if( pItemInfo->LimitJob>0 )
		{
			m_ShopItemOption.ComboDamage += (pItemInfo->LimitJob*calc);
			if(m_ShopItemOption.ComboDamage <= 0)
				m_ShopItemOption.ComboDamage=0;
		}
		// 크리티컬 증가량   CriticalPercent
		if( pItemInfo->LimitGender>0 )
		{
			m_ShopItemOption.Critical += (pItemInfo->LimitGender*calc);
			if(m_ShopItemOption.Critical <= 0)
				m_ShopItemOption.Critical=0;

			GAMEIN->GetCharacterDialog()->SetCritical();
		}
		// 크리티컬시 스턴확률    RangeAttackMin
		if( pItemInfo->LimitLevel>0 )
		{
			m_ShopItemOption.StunByCri += (pItemInfo->LimitLevel*calc);
			if(m_ShopItemOption.StunByCri <= 0)
				m_ShopItemOption.StunByCri=0;
		}
		// 물리방어력
		if( pItemInfo->LimitGenGol>0 )
		{
			m_ShopItemOption.RegistPhys += (pItemInfo->LimitGenGol*calc);
			if(m_ShopItemOption.RegistPhys < 0)
				m_ShopItemOption.RegistPhys = 0;
			
			GAMEIN->GetCharacterDialog()->SetDefenseRate();
		}
		// 속성방어력
		if( pItemInfo->LimitMinChub>0 )
		{
			m_ShopItemOption.RegistAttr += (pItemInfo->LimitMinChub*calc);
			if(m_ShopItemOption.RegistAttr < 0)
				m_ShopItemOption.RegistAttr = 0;

			GAMEIN->GetCharacterDialog()->UpdateData();
		}
		// 내력소모감소
		if( pItemInfo->LimitCheRyuk>0 )
		{
			m_ShopItemOption.NeaRyukSpend += (pItemInfo->LimitCheRyuk*calc);
			if(m_ShopItemOption.NeaRyukSpend < 0 )
				m_ShopItemOption.NeaRyukSpend = 0;
		}
		// 최대생명력 증가치
		if( pItemInfo->Plus_MugongIdx )
		{
			m_ShopItemOption.Life += (pItemInfo->Plus_MugongIdx*calc);
			if( m_ShopItemOption.Life <= 0 )
				m_ShopItemOption.Life = 0;	
		}
		// 최대호신강기 증가치
		if( pItemInfo->Plus_Value )
		{
			m_ShopItemOption.Shield += (pItemInfo->Plus_Value*calc);
			if( m_ShopItemOption.Shield <= 0 )
				m_ShopItemOption.Shield = 0;
		}
		// 최대내력 증가치
		if( pItemInfo->AllPlus_Kind )
		{
			m_ShopItemOption.Naeryuk += (pItemInfo->AllPlus_Kind*calc);
			if( m_ShopItemOption.Naeryuk <= 0 )
				m_ShopItemOption.Naeryuk = 0;
		}
		// 경공딜레이
		if( pItemInfo->RangeAttackMin )
		{
			m_ShopItemOption.bKyungGong += (pItemInfo->RangeAttackMin*calc);
		}
		// 경공속도 상승
		if( pItemInfo->RangeAttackMax )
		{
			m_ShopItemOption.KyungGongSpeed += (pItemInfo->RangeAttackMax*calc);
		}
		// 강화 증폭 수치
		if( pItemInfo->CriticalPercent )
		{
			m_ShopItemOption.ReinforceAmp += (pItemInfo->CriticalPercent*calc);

			STATSMGR->CalcItemStats(this);
			GAMEIN->GetCharacterDialog()->RefreshInfo();
		}
		// 아이템드랍확률 
		if( pItemInfo->PhyDef )
		{
			m_ShopItemOption.AddItemDrop += (pItemInfo->PhyDef*calc);
		}
		// 크리티컬 증가량   CriticalPercent
		if( pItemInfo->NaeRyukRecover>0 )
		{
			m_ShopItemOption.Decisive += (pItemInfo->NaeRyukRecover*calc);
			if(m_ShopItemOption.Decisive <= 0)
				m_ShopItemOption.Decisive=0;

			GAMEIN->GetCharacterDialog()->SetCritical();
		}
		// 노점상 꾸미기(노점상꾸미기이면 샵아이템 옵션에 해당 아이템 인덱스를 넣는다.
		if( (DWORD)(pItemInfo->AttrRegist.GetElement_Val(ATTR_FIRE)) > 0 )
		{
			MSG_DWORD2 msg;
			msg.Category = MP_ITEMEXT;
			msg.Protocol = MP_ITEMEXT_SHOPITEM_DECORATION_ON;
			msg.dwObjectID = HEROID;
			msg.dwData2 = STREETSTALLMGR->GetStallKind();
			
			if(bAdd == FALSE)
			{
				msg.dwData1 = 0;
				m_ShopItemOption.dwStreetStallDecoration = 0;
				OBJECTMGR->SetRemoveDecorationInfo(HERO);
			}
			else
			{
				msg.dwData1 = pItemInfo->ItemIdx;
				m_ShopItemOption.dwStreetStallDecoration = pItemInfo->ItemIdx;


				if( GetState() == eObjectState_StreetStall_Owner)
                    STREETSTALLMGR->AddStreetStallDecoration(HERO, STREETSTALLMGR->GetStallKind());
			}
			
			NETWORK->Send(&msg,sizeof(msg));
		}
		
/*		// 내력회복룰 1.5배
		if( pItemInfo->NaeRyukRecoverRate>0 )
		{
			m_ShopItemOption.RecoverRate += (pItemInfo->NaeRyukRecoverRate*calc);
			if( m_ShopItemOption.RecoverRate<=0 )
				m_ShopItemOption.RecoverRate = 0;

		}*/
	}
	else if( pItemInfo->ItemKind == eSHOP_ITEM_HERB )
	{
		// 최대생명력 증가치
		if( pItemInfo->Life > 0 )
		{
			m_ShopItemOption.Life += (WORD)(pItemInfo->Life*calc);
			if( m_ShopItemOption.Life <= 0 )
				m_ShopItemOption.Life = 0;		
		}
		// 최대호신강기 증가치
		if( pItemInfo->Shield > 0 )
		{
			m_ShopItemOption.Shield += (WORD)(pItemInfo->Shield*calc);
			if( m_ShopItemOption.Shield <= 0 )
				m_ShopItemOption.Shield = 0;
		}
		// 최대내력 증가치
		if( pItemInfo->NaeRyuk > 0 )
		{
			m_ShopItemOption.Naeryuk += (pItemInfo->NaeRyuk*calc);
			if( m_ShopItemOption.Naeryuk <= 0 )
				m_ShopItemOption.Naeryuk = 0;
		}

		// 스탯 적용
		// STATSMGR->CalcCharStats( pPlayer );
	}
	else if( pItemInfo->ItemKind == eSHOP_ITEM_MAKEUP || pItemInfo->ItemKind == eSHOP_ITEM_DECORATION )
	{
//		APPEARANCEMGR->SetAvatarItem( HERO, pItemInfo->ItemIdx, bAdd );
	}
	else if( pItemInfo->ItemKind == eSHOP_ITEM_SUNDRIES )
	{
		/*
		if( pItemInfo->ItemIdx == eSundries_Shout && bAdd )
			GAMEIN->GetShoutDlg()->SetActive( TRUE );
		*/


		// 노점개설
		if( pItemInfo->CheRyuk )
			m_ShopItemOption.bStreetStall += (pItemInfo->CheRyuk*calc);
	}	
}


void CHero::ActiveOptionInfoToInterface()
{
	if( m_ShopItemOption.Gengol )
		GAMEIN->GetCharacterDialog()->SetGenGol();
	if( m_ShopItemOption.Minchub )
		GAMEIN->GetCharacterDialog()->SetMinChub();
	if( m_ShopItemOption.Cheryuk )
		GAMEIN->GetCharacterDialog()->SetCheRyuk();
	if( m_ShopItemOption.Simmek )
		GAMEIN->GetCharacterDialog()->SetSimMek();
}


void CHero::CheckShopItemUseInfoToInventory( WORD ItemIdx, WORD ItemPos )
{
	CItem*	pItem = NULL;
	
	AVATARITEM* pAvatarItem = GAMERESRCMNGR->m_AvatarEquipTable.GetData( ItemIdx );
	if(!pAvatarItem)	return;

	for(int i=0; i<eAvatar_Max; i++)
	{
		// 아바타 아이템이 빠져야 하는경우이거나
		// 같은 자리에 아바타 아이템이 있는 경우엔.
		if( (!pAvatarItem->Item[i] && m_ShopItemOption.Avatar[i]) ||
			(m_ShopItemOption.Avatar[i] && i==pAvatarItem->Position) )
		{
			for(int n=0; n<SLOT_SHOPINVEN_NUM/2; n++)
			{
				pItem = GAMEIN->GetInventoryDialog()->GetItemForPos( n+TP_SHOPINVEN_START );
				if(!pItem)		continue;

				if( pItem->GetItemIdx()==m_ShopItemOption.Avatar[i] && pItem->GetUseParam() )
				{
					pItem->SetUseParam( 0 );
					break;
				}				
			}
		}
	}
}


void CHero::RefreshLevelupPoint()
{
	if( m_ShopItemOption.StatePoint==0 )		return;

	// Hard Coding
	WORD point = m_HeroTotalInfo.LevelUpPoint + (30 - m_ShopItemOption.StatePoint);	
	GAMEIN->GetCharacterDialog()->RefreshPointInfo();//( TRUE, point );
}

void CHero::SetMunpaName(char* Name)
{
	strcpy(m_CharacterInfo.GuildName, Name);
}

void CHero::SetGuildName(char* Name)	
{ 
	SafeStrCpy(m_CharacterInfo.GuildName, Name, MAX_GUILD_NAME+1);	
}

char* CHero::GetGuildName()			
{ 
	return m_CharacterInfo.GuildName;
}

BOOL CHero::CanSocietyAction( DWORD curTime )
{
	if( GetState() != eObjectState_None && GetState() != eObjectState_Society &&
		GetState() != eObjectState_Immortal )
		return FALSE;

	DWORD dwElapsedTime = gCurTime - m_dwLastSocietyAction;
	if( dwElapsedTime > 2000 ) //2초
	{
		m_dwLastSocietyAction = gCurTime;
		return TRUE;
	}

	return FALSE;
}

void CHero::SetStage( BYTE Stage )
{
	CPlayer::SetStage( Stage );

	GAMEIN->GetCharacterDialog()->SetStage( Stage );

//	GAMEIN->GetInventoryDialog()->ref
	ABILITYMGR->UpdateAbilityState( ABILITYUPDATE_CHARACTERSTAGE_CHANGED, 0, GetAbilityGroup() );
}

#ifdef _JAPAN_LOCAL_
DWORD CHero::DoGetAttAttackPowerMax( WORD Attrib )
{
	DWORD MaxV = 0;

	WORD SimMek = GetSimMek();
	DWORD MaxLVV = ( GetLevel()+5 ) + 5;		// RateMax = (Level + 5) + 5

	if( PARTYMGR->IsHelpPartyMember() && BATTLESYSTEM->GetBattle(this)->GetBattleKind() != eBATTLE_KIND_VIMUSTREET )
		MaxLVV = MaxLVV + MaxLVV * 20 / 100;

	//아이템 + 특기의 속성공격력
	DWORD AttribPlus = GetAttribPlusPercent( Attrib );

	MaxV = MaxLVV + AttribPlus + SimMek/2;

	return MaxV;
}

DWORD CHero::DoGetAttAttackPowerMin( WORD Attrib )
{
	DWORD MinV = 0;

	WORD SimMek = GetSimMek();
	DWORD MinLVV = ( GetLevel()+5 ) - 5;		// RateMax = (Level + 5) - 5

	if( PARTYMGR->IsHelpPartyMember() && BATTLESYSTEM->GetBattle(this)->GetBattleKind() != eBATTLE_KIND_VIMUSTREET )
		MinLVV = MinLVV + MinLVV * 20 / 100;

	//아이템 + 특기의 속성공격력
	DWORD AttribPlus = GetAttribPlusPercent( Attrib );

	MinV = MinLVV + AttribPlus + SimMek/2;

	return MinV;
}

DWORD CHero::GetAttribPlusPercent( WORD Attrib )
{
	return GetItemStats()->AttributeAttack.GetElement_Val(Attrib) + 
		   GetAbilityStats()->AttAttackUp.GetElement_Val(Attrib);
}

#endif

////////////////////////////////////////////////////////
//06. 06 2차 전직 - 이영준
//이펙트 생략(무초)
BOOL CHero::IsSkipSkill()
{
	//현경 탈마가 아니면 사용못함
	if(	m_CharacterInfo.Stage != eStage_Hyun &&
		m_CharacterInfo.Stage != eStage_Tal )
		return FALSE;

	//무초상태가 걸려있는지 검사
	WORD SkipKind = 0;

	PTRLISTSEARCHSTART(m_StatusList,CStatus*,pSL)
		pSL->GetSkipEffectKind(SkipKind);
	PTRLISTSEARCHEND;

	if(SkipKind == 0)
		return FALSE;
	else
		return (BOOL)SkipKind;
}
////////////////////////////////////////////////////////

//SW070127 타이탄
BOOL CHero::CanUseTitanSkill()
{
	BOOL canUse = FALSE;
	//기획변경 //장비와 무관
	//canUse = ( HERO->InTitan() && TITANMGR->CheckUsingEquipItemNum() );
	canUse = HERO->InTitan();

	return canUse;
}

////// 2007. 6. 14. CBH - 세트아이탬 능력치 계산 함수 추가
void CHero::AddSetitemOption(SET_ITEM_OPTION* pSetItemOption)
{
	float attrvalue = 0;

	/// 근골
	m_setItemStats.wGenGol += pSetItemOption->wGenGol;	
	//민첩
	m_setItemStats.wMinChub += pSetItemOption->wMinChub;
	//체력
	m_setItemStats.wCheRyuk += pSetItemOption->wCheRyuk;
	//심맥
	m_setItemStats.wSimMek += pSetItemOption->wSimMek;
	//최대생명력
	m_setItemStats.dwLife += pSetItemOption->dwLife;
	//호신강기
	m_setItemStats.dwShield += pSetItemOption->dwShield;
	//최대내력
	m_setItemStats.dwNaeRyuk += pSetItemOption->dwNaeRyuk;
	//속성 저항력	
	m_setItemStats.AttrRegist.AddATTRIBUTE_VAL(pSetItemOption->AttrRegist,1);	
	//근거리 최소 공격력
	m_setItemStats.wMeleeAttackMin += pSetItemOption->wMeleeAttackMin;
	//근거리 최대 공격력
	m_setItemStats.wMeleeAttackMax += pSetItemOption->wMeleeAttackMax;
	//원거리 최소 공격력
	m_setItemStats.wRangeAttackMin += pSetItemOption->wRangeAttackMin;
	//원거리 최대 공격력	
	m_setItemStats.wRangeAttackMax += pSetItemOption->wRangeAttackMax;
	//크리티컬
	m_setItemStats.wCriticalPercent += pSetItemOption->wCriticalPercent;	
	//속성 공격력	
	m_setItemStats.AttrAttack.AddATTRIBUTE_VAL(pSetItemOption->AttrAttack,1);		
	//물리방어력
	m_setItemStats.wPhyDef += pSetItemOption->wPhyDef;
	//생명력 회복 (고정)	
	m_setItemStats.wLifeRecover += pSetItemOption->wLifeRecover;
	//생명력 회복
	m_setItemStats.fLifeRecoverRate += pSetItemOption->fLifeRecoverRate;
	//내력 회복 (고정)
	m_setItemStats.wNaeRyukRecover += pSetItemOption->wNaeRyukRecover;
	//내력 회복
	m_setItemStats.fNaeRyukRecoverRate += pSetItemOption->fNaeRyukRecoverRate;
}

void CHero::ClearSetitemOption()
{
	memset(&m_setItemStats, 0, sizeof(SET_ITEM_OPTION));
}
///////////////////////////////////////////////////////////////////////////////////

// magi82 - UniqueItem(070626)
void CHero::AddUniqueItemOption(UNIQUE_ITEM_OPTION_INFO* pUniqueOption)
{
	m_UniqueItemStats.nHp += pUniqueOption->nHp;
	m_UniqueItemStats.nMp += pUniqueOption->nMp;
	m_UniqueItemStats.nShield += pUniqueOption->nShield;
	m_UniqueItemStats.nPhyDamage += pUniqueOption->nPhyDamage;
	m_UniqueItemStats.nCriDamage += pUniqueOption->nCriDamage;
	m_UniqueItemStats.nCriRate += pUniqueOption->nCriRate;
	m_UniqueItemStats.nGengol += pUniqueOption->nGengol;
	m_UniqueItemStats.nMinChub += pUniqueOption->nMinChub;
	m_UniqueItemStats.nCheRyuk += pUniqueOption->nCheRyuk;
	m_UniqueItemStats.nSimMek += pUniqueOption->nSimMek;
	m_UniqueItemStats.nDefen += pUniqueOption->nDefen;
	m_UniqueItemStats.nRange += pUniqueOption->nRange;
	m_UniqueItemStats.nAttR += pUniqueOption->nAttR;
	m_UniqueItemStats.nEnemyDefen += pUniqueOption->nEnemyDefen;
}
//////////////////////////////////////////////////////////////////////////
