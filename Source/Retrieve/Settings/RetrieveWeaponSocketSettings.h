#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "RetrieveWeaponSocketSettings.generated.h"

// 한 무기 타입의 손 소켓(DrawnSocket) → 등/허리(수납) 소켓 매핑.
USTRUCT()
struct FRetrieveSheathedSocketMap
{
	GENERATED_BODY()

	// 키가 없으면 그 파트는 납검 시 이동하지 않는다(예: 항상 손에 있는 무기).
	UPROPERTY(EditAnywhere, Category = "Sockets")
	TMap<FName, FName> DrawnToSheathed;
};

/**
 * 무기 타입별 손 소켓 → 등(수납) 소켓 매핑. 캐릭터 스켈레톤 기준 전역 규칙.
 * Project Settings > Retrieve > Weapon Sockets.
 * 무기 데이터(DT)는 손 소켓(AttachSocketName)만 정의하고, 납검 위치는 여기서 무기 타입 태그로 해석한다
 * (DT에 소켓을 흩뿌리지 않기 위해 분리 — 무기 추가 시 팀원이 납검 소켓을 신경 쓸 필요 없음).
 * 같은 손 소켓이라도 무기 타입마다 다른 등 소켓에 수납할 수 있다(검≠도끼).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Retrieve Weapon Sockets"))
class RETRIEVE_API URetrieveWeaponSocketSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Retrieve"); }

	// 무기 타입 태그 → (손소켓 → 등소켓).
	UPROPERTY(Config, EditAnywhere, Category = "Sockets", meta = (Categories = "Weapon.Type"))
	TMap<FGameplayTag, FRetrieveSheathedSocketMap> SocketsByWeaponType;
};