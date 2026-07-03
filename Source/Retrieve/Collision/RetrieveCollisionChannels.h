#pragma once

#include "CoreMinimal.h"

// Config/DefaultEngine.ini의 +DefaultChannelResponses Name="Gatherable" 채널 번호와 반드시 일치해야 한다.
namespace RetrieveCollisionChannels
{
	constexpr ECollisionChannel Gatherable = ECC_GameTraceChannel3;
}
