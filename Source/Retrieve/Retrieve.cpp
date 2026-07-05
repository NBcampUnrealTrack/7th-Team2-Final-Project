// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retrieve.h"
#include "CoreGlobals.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

class FRetrieveGameModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

#if UE_BUILD_SHIPPING
		// UE 5.7의 Landscape render-asset streaming 경로에서 발생하는 Shipping 전용
		// access violation을 피한다. 현재 패키지에서 -NoTextureStreaming으로 검증된 설정이다.
		SetConsoleVariable(TEXT("r.TextureStreaming"), 0);

		// Shipping에서는 C++/Blueprint/플러그인에서 요청한 디버그 도형과 화면 메시지를
		// 전역 차단한다. Development/Editor 빌드의 디버깅 기능은 그대로 유지된다.
		SetConsoleVariable(TEXT("r.EnableDrawDebugHelpers"), 0);
		GAreScreenMessagesEnabled = false;
#endif
	}

private:
	static void SetConsoleVariable(const TCHAR* Name, const int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByCode);
		}
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FRetrieveGameModule, Retrieve, "Retrieve");
