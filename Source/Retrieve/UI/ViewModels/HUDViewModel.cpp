#include "UI/ViewModels/HUDViewModel.h"

#include "BossStatusViewModel.h"
#include "ElementGaugeViewModel.h"
#include "PlayerStatusViewModel.h"
#include "QuestTrackerViewModel.h"
#include "ReticleViewModel.h"

UHUDViewModel::UHUDViewModel()
{
	PlayerStatus = CreateDefaultSubobject<UPlayerStatusViewModel>(TEXT("PlayerStatus"));
	ElementGauge = CreateDefaultSubobject<UElementGaugeViewModel>(TEXT("ElementGauge"));
	BossStatus = CreateDefaultSubobject<UBossStatusViewModel>(TEXT("BossStatus"));
	QuestTracker = CreateDefaultSubobject<UQuestTrackerViewModel>(TEXT("QuestTracker"));
	Reticle = CreateDefaultSubobject<UReticleViewModel>(TEXT("Reticle"));
}
