#include "Settings/RetrieveAudioRoutingAsset.h"

#if WITH_EDITOR

#include "Settings/RetrieveSettingsConfig.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "Misc/ScopedSlowTask.h"

void URetrieveAudioRoutingAsset::ApplyRoutingToAssets()
{
	const URetrieveSettingsConfig* Cfg = GetDefault<URetrieveSettingsConfig>();
	if (!Cfg)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AudioRouting] RetrieveSettingsConfig를 찾을 수 없습니다."));
		return;
	}

	// 채널 → SoundClass(Config의 SoftObject).
	auto ResolveClass = [Cfg](ERetrieveAudioChannel Channel) -> USoundClass*
	{
		switch (Channel)
		{
		case ERetrieveAudioChannel::Master:   return Cfg->MasterSoundClass.LoadSynchronous();
		case ERetrieveAudioChannel::Music:    return Cfg->MusicSoundClass.LoadSynchronous();
		case ERetrieveAudioChannel::Sfx:      return Cfg->SfxSoundClass.LoadSynchronous();
		case ERetrieveAudioChannel::Ambience: return Cfg->AmbienceSoundClass.LoadSynchronous();
		case ERetrieveAudioChannel::UI:       return Cfg->UISoundClass.LoadSynchronous();
		case ERetrieveAudioChannel::Voice:    return Cfg->VoiceSoundClass.LoadSynchronous();
		default:                              return nullptr;
		}
	};

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	// 최종 할당을 모은다(같은 사운드는 뒤 규칙이 덮어써 last-write-wins, 저장도 1회만).
	TMap<USoundBase*, USoundClass*> FinalAssignments;
	for (const FRetrieveAudioRoutingRule& Rule : Rules)
	{
		USoundClass* TargetClass = ResolveClass(Rule.Channel);
		if (!TargetClass)
		{
			continue;
		}

		// 폴더 규칙(하위 포함).
		if (Rule.Folders.Num() > 0)
		{
			FARFilter Filter;
			Filter.bRecursivePaths = true;
			Filter.bRecursiveClasses = true;
			Filter.ClassPaths.Add(USoundBase::StaticClass()->GetClassPathName());
			for (const FDirectoryPath& Dir : Rule.Folders)
			{
				if (!Dir.Path.IsEmpty())
				{
					Filter.PackagePaths.Add(FName(*Dir.Path));
				}
			}
			if (Filter.PackagePaths.Num() > 0)
			{
				TArray<FAssetData> Assets;
				AssetRegistry.GetAssets(Filter, Assets);
				for (const FAssetData& AssetData : Assets)
				{
					if (USoundBase* Sound = Cast<USoundBase>(AssetData.GetAsset()))
					{
						FinalAssignments.Add(Sound, TargetClass);
					}
				}
			}
		}

		// 개별 사운드 규칙.
		for (const TSoftObjectPtr<USoundBase>& SoftSound : Rule.Sounds)
		{
			if (USoundBase* Sound = SoftSound.LoadSynchronous())
			{
				FinalAssignments.Add(Sound, TargetClass);
			}
		}
	}

	int32 Changed = 0;
	int32 Saved = 0;
	FScopedSlowTask SlowTask(static_cast<float>(FinalAssignments.Num()), NSLOCTEXT("Retrieve", "ApplyAudioRouting", "사운드 클래스 일괄 할당 중..."));
	SlowTask.MakeDialog();

	for (const TPair<USoundBase*, USoundClass*>& Pair : FinalAssignments)
	{
		SlowTask.EnterProgressFrame(1.f);
		USoundBase* Sound = Pair.Key;
		USoundClass* TargetClass = Pair.Value;
		if (!Sound)
		{
			continue;
		}

		if (Sound->SoundClassObject != TargetClass)
		{
			Sound->Modify();
			Sound->SoundClassObject = TargetClass;
			Sound->MarkPackageDirty();
			++Changed;
		}

		// 이미 같은 클래스여도 저장은 보장(처음 적용/재적용 모두 디스크 반영).
		if (UEditorAssetLibrary::SaveLoadedAsset(Sound, /*bOnlyIfIsDirty*/ false))
		{
			++Saved;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[AudioRouting] 일괄 할당 완료: 대상 %d개, 변경 %d개, 저장 %d개."),
		FinalAssignments.Num(), Changed, Saved);
}

#endif // WITH_EDITOR
