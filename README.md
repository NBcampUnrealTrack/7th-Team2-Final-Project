# 7th-Team2-Final-Project

# Retrieve

> 잃어버린 원소의 조각을 되찾아 세계의 봉인을 다시 여는 3D 액션 RPG.

<img width="1672" height="941" alt="team2_title" src="https://github.com/user-attachments/assets/adf7d84b-f84c-4ed4-b985-8b0bcef47e6f" />
**7기 Team2 Final Project — Retrieve**
Unreal Engine 5.7 기반, 8인 팀 개발.


---

## 목차
- [소개](#소개)
- [팀 구성](#팀-구성)
- [플레이 이미지](#플레이-이미지)
- [핵심 시스템](#핵심-시스템)
- [사용 기술](#사용-기술)
- [프로젝트 실행](#프로젝트-실행)

---

## 소개

Retrieve는 원소의 힘이 사라진 세계를 배경으로 하는 3인칭 액션 어드벤처 게임입니다.
플레이어는 소버린이 되어 흩어진 원소 조각을 회수하며 지역별 수호자와 최종 보스를 마주하게 됩니다.

- **장르**: 3D 액션 RPG / 어드벤처
- **플랫폼**: Windows (PC)
- **엔진**: Unreal Engine 5.7
- **개발 기간**: 9주 (MVP 4주 + 확장 5주)

<!-- gif 배치 예정: 인트로 시네마틱 or 월드 오프닝 컷 -->

---

## 팀 구성

| 담당 파트 | 이름 |
| --- | --- |
| 전투 · GAS(Gameplay Ability System) 파이프라인 | 박하민 |
| 전투 액션 · 카메라 · 락온 · 타격 피드백 | 윤기상 |
| 원소 시스템 · 게이지 · 버스트 · 원소 공명 | 이병헌 |
| 캐릭터 애니메이션 · VFX(Visual Effects) | 정보원 |
| 게임 플로우 · 스토리 · 퀘스트 · NPC(Non-Player Character) | 장유진 |
| 몬스터 · 보스 · AI(Artificial Intelligence) | 정찬호 |
| UI(User Interface) · HUD(Heads-Up Display) · 인벤토리 · 제작 · 월드맵 | 이희원 |
| 레벨 · 환경 · 상호작용 · 라이팅 · 시네마틱 | 이현진 |

---

## 플레이 이미지
<img width="800" height="455" alt="2026-07-21220706-ezgif com-video-to-gif-converter" src="https://github.com/user-attachments/assets/2cfcd7b6-4f60-48f0-afb2-cee7b9e68a65" />

<!-- gif 배치 예정: 전투 콤보 -->
<!-- gif 배치 예정: 원소 게이지·버스트 발동 -->
<!-- gif 배치 예정: 보스전(가디언/퀸) -->
<!-- gif 배치 예정: 월드 탐험 & 월드맵 -->
<!-- gif 배치 예정: 인벤토리·제작 UI -->

---

## 핵심 시스템

### 1. 근접 액션 전투

무기별 콤보와 회피, 가드, 패리를 GAS로 엮은 리듬형 근접 전투입니다.
공격은 트레이스 판정으로 피격을 인식하고, 피격자는 방향과 강도에 맞는 반응을 재생합니다.
가드 중에는 받는 피해가 줄고, 정확한 타이밍의 튕겨내기는 반격 상태를 열어 줍니다.

플레이어가 사용하는 무기는 세 종류이며 각각 다른 리듬을 가집니다.

- **검+방패** — 표준 콤보와 가드/패리
- **쌍검** — 짧은 리치의 빠른 다단 히트
- **지팡이(Staff)** — 원거리 발사와 짧은 순간이동

<!-- gif 배치 예정: 검·쌍검·지팡이 각 콤보 -->

### 2. 원소 시스템

세 가지 속성을 게이지에 축적한 뒤, 조합에 따라 서로 다른 특수기를 폭발적으로 방출하는 자원 관리형 액션입니다.

- 세 개의 원소 슬롯(불 · 물 · 바람)을 채워 나가는 게이지 구조
- 슬롯 조합에 따라 달라지는 버스트(Burst) 결과
- 특정 지역의 시길과 가디언 처치를 통해 순차적으로 원소가 해금됨
- 장비와 결합해 발동되는 원소 공명(Elemental Resonance)

<!-- gif 배치 예정: 게이지 축적 → 버스트 발동 -->

### 3. 몬스터·보스 AI

몬스터는 상태 기반 AI로 순찰, 경계, 추적, 공격, 복귀, 사망을 오갑니다.
플레이어가 시야에 잡히면 경계 게이지가 오르고, 시선이 오래 이어지면 추적으로 전환됩니다.
지형이 시야를 가리면 다시 의심 상태로 되돌아가며, 놓친 마지막 위치를 기준으로 주변을 살펴봅니다.

- **일반 몬스터**: 늑대, 언데드, 고블린 계열
- **에픽 몬스터**: 강화 패턴을 가진 필드 정예 (예: 콜로서스)
- **보스**: 지역별 원소 가디언, 그리고 다단계 페이즈로 진행되는 최종 보스 네크로멘서/퀸

보스는 페이즈 전환 시 VFX와 몽타주가 함께 재생되며, 진행에 따라 새로운 이동·공격 루틴이 열립니다.

<!-- gif 배치 예정: 필드 몬스터 조우 -->
<!-- gif 배치 예정: 보스 페이즈 전환 -->

### 4. 월드와 탐험

세계는 튜토리얼 지역 → 물 지역 → 바람 지역 → 불 지역 → 최종 지역 순으로 이어지는 반열린 구조입니다.
지역마다 고유한 스카이박스, BGM(Background Music), 라이팅 톤이 지정되어 있으며, 스토리 트리거에 도달하면 컷씬으로 전환됩니다.

- 화톳불 저장 지점 · 리스폰 지점
- 지역별 몬스터 스포너와 조우 트리거
- 나침반 · 웨이포인트 · 전장의 안개가 걷히는 정적 미니맵과 월드맵

<!-- gif 배치 예정: 월드 이동 & 지역 전환 -->

### 5. 퀘스트와 스토리

동료 NPC 루멘(Lumen)이 플레이어를 따라다니며 대화와 각인 이벤트로 이야기를 이끌어 갑니다.
퀘스트는 스텝 기반으로 진행되며, 각 스텝의 도달 조건이 만족될 때마다 UI 트래커와 시스템 메시지가 갱신됩니다.
오프닝과 엔딩은 별도의 시네마틱 서브시스템으로 재생됩니다.

<!-- gif 배치 예정: 대화·각인 컷 -->
<!-- gif 배치 예정: 오프닝/엔딩 시네마틱 -->

### 6. 인벤토리 · 장비 · 제작

수집한 재료와 장비, 소모품을 인벤토리에서 관리하고, 화톳불 근처에서 제작이 가능합니다.
장비에는 세트 효과와 전설 진화 흐름이 있어, 특정 부위를 모으면 능력치가 크게 갱신됩니다.

- 카테고리 탭 인벤토리 (무기 · 장비 · 소모품 · 재료)
- 아이템 획득 시 커스텀 토스트와 재화 표시
- 배치 제작 요약 팝업
- 퀵슬롯 두 개, 조작키 안내와 옵션 연동

<!-- gif 배치 예정: 인벤토리 UI -->
<!-- gif 배치 예정: 장비 강화 VFX -->

### 7. UI · HUD

HUD는 MVVM(Model-View-ViewModel) 구조로 구성되어, 체력·원소 게이지·보스 체력바·퀘스트 트래커·데미지 플로터가 각각 독립된 뷰모델에 바인딩됩니다.
설정 메뉴에서는 감도, FOV, 모션 블러, HUD 표시, 활 조준 토글, 그리고 조작키 리바인딩을 지원합니다.

<!-- gif 배치 예정: HUD 오버레이 & 게이지 애니메이션 -->

---

## 사용 기술

**엔진 · 언어**
- Unreal Engine 5.7
- C++ 17 (Epic 코딩 표준 준수)

**게임플레이 프레임워크**
- GAS (Gameplay Ability System)
- Enhanced Input
- Gameplay Tags · Gameplay Messages
- StateTree · GameplayStateTree
- Modular Gameplay

**애니메이션 · 캐릭터**
- Animation Blueprint (Linked Anim Layer 구조)
- Motion Warping · Animation Warping
- Control Rig · IK Rig
- Animation Locomotion Library

**UI**
- UMG (Unreal Motion Graphics)
- MVVM (Model-View-ViewModel)

**환경 · 렌더링**
- Niagara (VFX)
- PCG (Procedural Content Generation) — 소품 배치 및 지형 채우기
- Stylized Rendering System 마이그레이션 (지역별 톤 통일)
- Lumen 라이팅

**협업 · 배포**
- GitHub (Private 작업 레포 + Public 미러)
- Git LFS (에셋 관리)
- Jira · Notion · Discord

---

## 프로젝트 실행

### 요구 사항
- Windows 10 이상
- Unreal Engine 5.7
- Visual Studio 2022 또는 Rider

### 클론 및 빌드
```bash
git clone <레포 주소>
cd 7th-Team2-Final-Project
