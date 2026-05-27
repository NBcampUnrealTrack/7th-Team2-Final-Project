#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "RetrieveMapIconComponent.generated.h"

/**
 * 미니맵·월드맵에 표시될 아이콘을 액터에 부착하는 컴포넌트.
 *
 * IconType에 따라 WBP_Minimap의 IconRegistry에서 텍스처/색상/크기를 가져온다.
 * 개별 오버라이드가 필요하면 bOverrideIcon을 true로 설정한다.
 */
UCLASS(ClassGroup=(Minimap), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveMapIconComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveMapIconComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	ERetrieveMapIconType IconType = ERetrieveMapIconType::POI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	bool bShowOnMinimap = true;

	// 월드맵에 표시할 위치 이름 (비어 있으면 레이블 미표시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	FText MapLabel;

	// false면 월드맵에서도 레이블 숨김
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	bool bShowLabelOnWorldMap = true;

	// true면 IconRegistry 대신 아래 값을 직접 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap")
	bool bOverrideIcon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap", meta=(EditCondition="bOverrideIcon"))
	TObjectPtr<UTexture2D> OverrideTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap", meta=(EditCondition="bOverrideIcon"))
	FLinearColor OverrideColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|Minimap", meta=(EditCondition="bOverrideIcon", ClampMin="8"))
	float OverrideSize = 16.0f;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RegisterWithMapSubsystem();
	void UnregisterFromMapSubsystem();
};
