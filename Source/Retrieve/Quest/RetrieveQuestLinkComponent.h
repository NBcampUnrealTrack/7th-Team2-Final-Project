#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RetrieveQuestLinkComponent.generated.h"

/** 레벨에 수동 배치한 액터가 어떤 퀘스트 역할을 맡는지 나타내는 역할 종류. */
UENUM(BlueprintType)
enum class ERetrieveQuestLinkRole : uint8
{
	/** 의뢰/보상 대화를 하는 NPC. 인카운터 DialogueNPCRole("NPC")로 해석. */
	NPC          UMETA(DisplayName = "대화 NPC"),
	/** 회수 대상 퀘스트 물건(CollectWorldItem). 여러 개면 RoleIndex 로 구분. */
	QuestItem    UMETA(DisplayName = "퀘스트 물건"),
	/** 위치 도달 목표(ReachLocation)의 목적지. 여러 개면 RoleIndex 로 구분. */
	Destination  UMETA(DisplayName = "목적지"),
	/** 완료 시 해방되는 상인. 인카운터 "Merchant" 역할로 해석. */
	Merchant     UMETA(DisplayName = "해방 상인"),
	/** Role 이름을 직접 입력(CustomRole). */
	Custom       UMETA(DisplayName = "직접 입력")
};

/**
 * 레벨에 직접 배치한 액터를 특정 퀘스트 인카운터의 역할에 "지정"하는 표식 컴포넌트.
 *
 * 이 컴포넌트가 붙은 액터는 ARetrieveQuestEncounter 가 BeginPlay 에서 스캔해
 * TargetEncounterId 가 일치하면 해당 Role 로 등록한다. 그러면 인카운터는 그 역할을
 * 런타임 스폰하지 않고 이 액터를 그대로 사용하므로, 배치 위치와 메시를 에디터에서
 * 직접 제어할 수 있다(자동 스폰/랜덤 메시 없음).
 *
 * 사용법:
 *  1) NPC/물건 BP 를 레벨에 배치하고 위치·메시를 원하는 대로 설정
 *  2) 이 컴포넌트를 추가하고 TargetEncounterId 를 대상 인카운터의 EncounterId 로,
 *     Role 을 알맞게 선택
 */
UCLASS(ClassGroup = (Retrieve), meta = (BlueprintSpawnableComponent, DisplayName = "Quest Link (수동 배치 연결)"))
class RETRIEVE_API URetrieveQuestLinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveQuestLinkComponent();

	/** 연결할 인카운터의 EncounterId. 인카운터 디테일 패널의 EncounterId 와 동일하게 설정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Link")
	FName TargetEncounterId;

	/** 이 액터가 담당하는 퀘스트 역할. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Link")
	ERetrieveQuestLinkRole Role = ERetrieveQuestLinkRole::QuestItem;

	/**
	 * 물건/목적지가 여러 개인 퀘스트에서 몇 번째인지(0부터).
	 * 정의의 목표 순서와 일치해야 한다(첫 물건=0, 두 번째=1 …).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Link",
		meta = (ClampMin = "0",
			EditCondition = "Role==ERetrieveQuestLinkRole::QuestItem||Role==ERetrieveQuestLinkRole::Destination",
			EditConditionHides))
	int32 RoleIndex = 0;

	/** Role==직접 입력 일 때 사용할 Role 이름(인카운터/목표에서 참조하는 이름). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Quest|Link",
		meta = (EditCondition = "Role==ERetrieveQuestLinkRole::Custom", EditConditionHides))
	FName CustomRole;

	/**
	 * 이 컴포넌트가 실제로 등록될 Role 이름을 반환.
	 * NPC→"NPC", QuestItem→"QuestItem_<Index>", Destination→"Destination_<Index>",
	 * Merchant→"Merchant", Custom→CustomRole.
	 * NpcRoleName 은 인카운터의 DialogueNPCRole(기본 "NPC")을 넘겨 받는다.
	 */
	FName ResolveRoleName(FName NpcRoleName) const;
};
