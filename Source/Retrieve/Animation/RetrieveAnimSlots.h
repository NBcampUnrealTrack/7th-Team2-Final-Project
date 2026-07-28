#pragma once

#include "CoreMinimal.h"

namespace RetrieveAnimSlots
{
	// F5 피드백 반영: 전역 변수 다중 정의(ODR 위반) 및 Pool 오버헤드 방지를 위해 inline const 구조로 전환
	inline const FName UpperBody(TEXT("UpperBody"));
	inline const FName FullBody(TEXT("FullBody"));
	inline const FName LowerBody(TEXT("LowerBody"));
}
