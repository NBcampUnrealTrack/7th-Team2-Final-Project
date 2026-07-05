#pragma once

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"

// 임시 진단용 헬퍼. Shipping 빌드는 UE_LOG가 전부 스트립되어(NO_LOGGING) 일반 로그를 남길 수 없으므로,
// 파일에 직접 텍스트를 기록해 크래시 직전 마지막으로 도달한 체크포인트를 확인하기 위한 용도.
// 원인 파악 후 호출부와 이 파일을 제거할 것.
inline void RetrieveDiagCheckpoint(const TCHAR* Tag)
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("RetrieveDiag.txt");
	const FString Line = FString::Printf(TEXT("[%s] %s\r\n"), *FDateTime::Now().ToString(), Tag);
	FFileHelper::SaveStringToFile(
		Line, *Path,
		FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(),
		FILEWRITE_Append);
}
