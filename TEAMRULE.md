# ⚙️ Retrieve 코딩 컨벤션

> 기본: [Epic UE 코딩 표준](https://dev.epicgames.com/documentation/ko-kr/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) · 아래는 팀 합의 추가 사항

---

## 네이밍

**클래스 접두사** `A` Actor · `U` UObject/Interface · `F` 구조체 · `E` 열거형 · `I` 인터페이스 · `T` 템플릿

**함수 접두사** `Do/Apply/Start/Stop` 동작 · `Try…` 실패 가능 · `Get/Is/Has/Can` 쿼리 · `On…` 델리게이트 · `Handle…` 핸들러 · `Notify…` 애님 노티파이

**에셋** `DA_` `DT_` `GE_` `GA_` `IMC_` `IA_` `BP_` `WB_` `ABP_` `AM_` `NS_` `M_/MI_`

- 불리언 변수: `bSomething` / 쿼리 함수에 `const` 필수
- 컴포넌트 → `…Component` · 서브시스템 → `…Subsystem`

---

## 포인터

| 상황 | 타입 |
|------|------|
| UObject 멤버 소유 | `UPROPERTY() TObjectPtr<UX>` |
| 비소유 참조 | `TWeakObjectPtr<UX>` |
| 에셋 지연 로딩 | `TSoftObjectPtr / TSoftClassPtr` |
| 함수 내 로컬 | 일반 `UX*` 가능 |

> `UPROPERTY()` 없는 `UX* RawPtr` 멤버 선언 금지

---

## RPC 네이밍

```cpp
Server_RequestEquipWeapon()   // UFUNCTION(Server, ...)
Multicast_PlayHitFX()         // UFUNCTION(NetMulticast, ...)
Client_ShowQuestPrompt()      // UFUNCTION(Client, ...)
```
- `_Implementation` 은 private, 직접 호출 금지
- 게임플레이 상태 → **Reliable** / 시각 효과 → **Unreliable**

---

## 헤더 인클루드 순서

```
1. CoreMinimal.h
2. 엔진 헤더 (알파벳 순)
3. 프로젝트 헤더 (알파벳 순)
4. *.generated.h  ← 반드시 마지막
```

---

## 클래스 선언 순서

```
생성자 → 엔진 오버라이드 → public API → 델리게이트 → protected UPROPERTY → private 헬퍼 → private 상태
```

---

## 게임플레이 태그

```cpp
RetrieveCombatTags::Ability_Player_Attack          // ✅
FGameplayTag::RequestGameplayTag(FName("..."))     // ❌ 문자열 직접 사용 금지
```

---

## 로깅

```cpp
UE_LOG(LogRetrieveCombat, Log, ...)   // ✅
UE_LOG(LogTemp, Warning, ...)         // ❌ 커밋 금지
```

카테고리: `LogRetrieveCombat` · `LogRetrieveElement` · `LogRetrieveAI` · `LogRetrieveUI` · `LogRetrieveSave`

---

## 포맷

- 중괄호: **Allman** (여는 중괄호 별도 줄)
- 들여쓰기: **탭**
- 단일 구문 `if` 에도 중괄호 필수
- `.clang-format` 저장 시 자동 적용

---

## 안티패턴 금지

| ❌ | ✅ |
|----|-----|
| 비대한 Character 클래스 | 컴포넌트로 분리 |
| Blueprint에 로직 작성 | C++로. BP는 콘텐츠·시각 효과만 |
| 매직 넘버 하드코딩 | DataTable / `UDeveloperSettings` |
| Tick으로 상태 폴링 | 델리게이트 / GameplayMessageSubsystem |
| 태그 문자열 직접 사용 | 네이티브 태그 핸들 |
| `UGameManager` 같은 매니저 | `UWorldSubsystem` / `UGameInstanceSubsystem` |
| 전역 객체에 에셋 하드 참조 | `TSoftObjectPtr` |
