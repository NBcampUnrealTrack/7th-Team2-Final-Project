#include "UI/Puzzle/RetrievePuzzleGenerator.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "Math/RandomStream.h"

namespace
{
	void GetNeighbors(int32 W, int32 H, int32 Cell, int32 (&Out)[4], int32& Count)
	{
		const int32 C = Cell % W;
		const int32 R = Cell / W;
		Count = 0;
		if (C + 1 < W) { Out[Count++] = Cell + 1; }
		if (C - 1 >= 0) { Out[Count++] = Cell - 1; }
		if (R + 1 < H) { Out[Count++] = Cell + W; }
		if (R - 1 >= 0) { Out[Count++] = Cell - W; }
	}

	// 존재 셀 집합이 4방향으로 모두 연결되어 있는지.
	bool IsConnected(int32 W, int32 H, const TArray<bool>& Exists, int32 MExist)
	{
		if (MExist <= 0)
		{
			return false;
		}
		int32 Start = INDEX_NONE;
		for (int32 i = 0; i < Exists.Num(); ++i)
		{
			if (Exists[i]) { Start = i; break; }
		}
		if (Start == INDEX_NONE)
		{
			return false;
		}

		TArray<bool> Visited;
		Visited.Init(false, Exists.Num());
		TArray<int32> Stack;
		Stack.Add(Start);
		Visited[Start] = true;
		int32 Seen = 0;
		while (Stack.Num() > 0)
		{
			const int32 Cur = Stack.Pop();
			++Seen;
			int32 Nbr[4], N;
			GetNeighbors(W, H, Cur, Nbr, N);
			for (int32 i = 0; i < N; ++i)
			{
				const int32 D = Nbr[i];
				if (Exists[D] && !Visited[D]) { Visited[D] = true; Stack.Add(D); }
			}
		}
		return Seen == MExist;
	}

	// 일부 칸을 구멍으로(연결 유지 + 최소 잔여칸 보장).
	void PlaceHoles(int32 W, int32 H, int32 HoleCount, int32 MinKeep, FRandomStream& Rng, TArray<bool>& Exists, int32& MExist)
	{
		const int32 Num = W * H;
		int32 Placed = 0;
		int32 Tries = 0;
		const int32 MaxTries = Num * 4 + 16;
		while (Placed < HoleCount && MExist > MinKeep && Tries < MaxTries)
		{
			++Tries;
			const int32 Cell = Rng.RandRange(0, Num - 1);
			if (!Exists[Cell])
			{
				continue;
			}
			Exists[Cell] = false; // 임시 제거
			if (IsConnected(W, H, Exists, MExist - 1))
			{
				--MExist;
				++Placed;
			}
			else
			{
				Exists[Cell] = true; // 연결 깨지면 되돌림
			}
		}
	}

	// 멀티소스 무작위 BFS로 존재 셀을 N개 연결 영역으로 분할. 각 영역 >= MinRegionSize.
	bool PartitionBoard(int32 W, int32 H, const TArray<bool>& Exists, int32 N, int32 MinRegionSize, FRandomStream& Rng, TArray<int32>& OutRegionOf)
	{
		const int32 Num = W * H;
		OutRegionOf.Init(INDEX_NONE, Num);

		TArray<int32> Existing;
		Existing.Reserve(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			if (Exists[i]) { Existing.Add(i); }
		}
		if (N <= 0 || Existing.Num() < N * FMath::Max(1, MinRegionSize))
		{
			return false;
		}

		// 시드 N개 무작위 선택.
		TArray<int32> Pool = Existing;
		for (int32 i = Pool.Num() - 1; i > 0; --i)
		{
			const int32 J = Rng.RandRange(0, i);
			Pool.Swap(i, J);
		}

		struct FClaim { int32 Cell; int32 Region; };
		TArray<FClaim> Frontier;

		for (int32 R = 0; R < N; ++R)
		{
			OutRegionOf[Pool[R]] = R;
		}
		for (int32 R = 0; R < N; ++R)
		{
			int32 Nbr[4], NN;
			GetNeighbors(W, H, Pool[R], Nbr, NN);
			for (int32 i = 0; i < NN; ++i)
			{
				if (Exists[Nbr[i]] && OutRegionOf[Nbr[i]] == INDEX_NONE)
				{
					Frontier.Add(FClaim{ Nbr[i], R });
				}
			}
		}

		int32 Assigned = N;
		const int32 Total = Existing.Num();
		while (Assigned < Total && Frontier.Num() > 0)
		{
			const int32 Fi = Rng.RandRange(0, Frontier.Num() - 1);
			const FClaim Claim = Frontier[Fi];
			Frontier.RemoveAtSwap(Fi);
			if (OutRegionOf[Claim.Cell] != INDEX_NONE)
			{
				continue;
			}
			OutRegionOf[Claim.Cell] = Claim.Region;
			++Assigned;
			int32 Nbr[4], NN;
			GetNeighbors(W, H, Claim.Cell, Nbr, NN);
			for (int32 i = 0; i < NN; ++i)
			{
				if (Exists[Nbr[i]] && OutRegionOf[Nbr[i]] == INDEX_NONE)
				{
					Frontier.Add(FClaim{ Nbr[i], Claim.Region });
				}
			}
		}
		if (Assigned < Total)
		{
			return false; // 연결 그래프면 발생하지 않음(방어적)
		}

		TArray<int32> Sizes;
		Sizes.Init(0, N);
		for (int32 i = 0; i < Num; ++i)
		{
			if (Exists[i] && OutRegionOf[i] >= 0) { Sizes[OutRegionOf[i]]++; }
		}
		for (int32 R = 0; R < N; ++R)
		{
			if (Sizes[R] < MinRegionSize) { return false; }
		}
		return true;
	}

	// 각 영역에서 K칸을 골라 원소를 하나씩 배치.
	void PlaceElements(int32 W, int32 H, const TArray<bool>& Exists, const TArray<int32>& RegionOf, int32 N, const TArray<FGameplayTag>& Types, FRandomStream& Rng, TArray<FRetrievePuzzleElementPlacement>& OutPlacements)
	{
		const int32 K = Types.Num();
		OutPlacements.Reset();

		TArray<TArray<int32>> RegionCells;
		RegionCells.SetNum(N);
		for (int32 i = 0; i < Exists.Num(); ++i)
		{
			if (Exists[i] && RegionOf[i] >= 0) { RegionCells[RegionOf[i]].Add(i); }
		}

		for (int32 R = 0; R < N; ++R)
		{
			TArray<int32>& Cells = RegionCells[R];
			for (int32 i = Cells.Num() - 1; i > 0; --i)
			{
				const int32 J = Rng.RandRange(0, i);
				Cells.Swap(i, J);
			}
			for (int32 k = 0; k < K && k < Cells.Num(); ++k)
			{
				const int32 Cell = Cells[k];
				FRetrievePuzzleElementPlacement P;
				P.Coord = FIntPoint(Cell % W, Cell / W);
				P.ElementTag = Types[k];
				OutPlacements.Add(P);
			}
		}
	}

	// 보드를 받아 유효한 분할 해의 개수를 SolutionLimit까지 세는 백트래킹 솔버.
	struct FPuzzleSolveModel
	{
		int32 W = 0, H = 0, Num = 0;
		TArray<bool> Exists;
		TArray<int32> CellType; // -1 = 빈칸, 그 외 = 원소 타입 인덱스
		int32 K = 0;
		TArray<int32> Anchors;  // 타입 0 셀(영역당 하나), 인덱스 오름차순
		int32 MExist = 0;

		TArray<int32> RegionOf;
		int32 AssignedCount = 0;
		int64 Budget = 200000;
		int64 Nodes = 0;
		int32 SolutionLimit = 2;
		int32 Solutions = 0;
		bool bBudgetExceeded = false;

		bool Init(const FRetrievePuzzleBoardDef& Board)
		{
			W = Board.GridWidth; H = Board.GridHeight; Num = W * H;
			if (W <= 0 || H <= 0 || Num <= 0)
			{
				return false;
			}

			Exists.Init(true, Num);
			for (const FIntPoint& A : Board.AbsentCells)
			{
				if (A.X >= 0 && A.X < W && A.Y >= 0 && A.Y < H) { Exists[A.Y * W + A.X] = false; }
			}

			CellType.Init(-1, Num);
			TArray<FGameplayTag> TypeList;
			TMap<FGameplayTag, int32> TypeIndex;
			for (const FRetrievePuzzleElementPlacement& P : Board.ElementPlacements)
			{
				if (!P.ElementTag.IsValid()) { continue; }
				if (P.Coord.X < 0 || P.Coord.X >= W || P.Coord.Y < 0 || P.Coord.Y >= H) { continue; }
				const int32 Idx = P.Coord.Y * W + P.Coord.X;
				if (!Exists[Idx]) { continue; }
				int32 Ti;
				if (const int32* Found = TypeIndex.Find(P.ElementTag)) { Ti = *Found; }
				else { Ti = TypeList.Num(); TypeList.Add(P.ElementTag); TypeIndex.Add(P.ElementTag, Ti); }
				CellType[Idx] = Ti;
			}

			K = TypeList.Num();
			if (K <= 0 || K > 8)
			{
				return false;
			}

			MExist = 0;
			for (int32 i = 0; i < Num; ++i)
			{
				if (Exists[i]) { ++MExist; }
			}
			for (int32 i = 0; i < Num; ++i)
			{
				if (Exists[i] && CellType[i] == 0) { Anchors.Add(i); }
			}
			Anchors.Sort();
			return Anchors.Num() > 0;
		}

		FORCEINLINE void Nbr(int32 Cell, int32 (&Out)[4], int32& Count) const
		{
			GetNeighbors(W, H, Cell, Out, Count);
		}

		bool Done() const { return bBudgetExceeded || Solutions >= SolutionLimit; }

		void Solve()
		{
			RegionOf.Init(INDEX_NONE, Num);
			AssignedCount = 0; Nodes = 0; Solutions = 0; bBudgetExceeded = false;
			BuildRegion(0);
		}

		// anchorIdx 번째 앵커의 영역을 만든다(앵커 인덱스 순서로 진행 → 각 분할이 한 번만 생성됨).
		void BuildRegion(int32 AnchorIdx)
		{
			if (Done())
			{
				return;
			}
			if (AnchorIdx == Anchors.Num())
			{
				if (AssignedCount == MExist) { ++Solutions; } // 모든 칸이 영역에 들어간 완전 분할
				return;
			}

			const int32 Anchor = Anchors[AnchorIdx];
			if (RegionOf[Anchor] != INDEX_NONE)
			{
				return; // 방어적(앞 영역이 다른 앵커를 삼키는 일은 없음)
			}

			TArray<int32> S;
			S.Add(Anchor);
			int32 Counts[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
			Counts[0] = 1; // 앵커는 타입 0
			TArray<int32> X;
			Enumerate(AnchorIdx, S, Counts, X);
		}

		// S(연결, 앵커 포함)에서 시작해 미할당 존재셀로 확장하며,
		// 모든 타입을 정확히 하나씩 가진 연결 부분집합마다 영역으로 확정하고 다음 앵커로 재귀.
		void Enumerate(int32 AnchorIdx, TArray<int32>& S, int32* Counts, TArray<int32>& X)
		{
			if (Done())
			{
				return;
			}
			if (++Nodes > Budget)
			{
				bBudgetExceeded = true;
				return;
			}

			bool bComplete = true;
			for (int32 t = 0; t < K; ++t)
			{
				if (Counts[t] != 1) { bComplete = false; break; }
			}
			if (bComplete)
			{
				for (int32 Cell : S) { RegionOf[Cell] = AnchorIdx; }
				AssignedCount += S.Num();
				BuildRegion(AnchorIdx + 1);
				AssignedCount -= S.Num();
				for (int32 Cell : S) { RegionOf[Cell] = INDEX_NONE; }
				if (Done())
				{
					return;
				}
			}

			// 후보 = S의 이웃 중 존재·미할당·(S/X에 없음). 정렬해 결정적 순서로.
			TArray<int32> Cand;
			{
				TSet<int32> SSet; SSet.Append(S);
				TSet<int32> XSet; XSet.Append(X);
				for (int32 Cell : S)
				{
					int32 NB[4], NN;
					Nbr(Cell, NB, NN);
					for (int32 i = 0; i < NN; ++i)
					{
						const int32 D = NB[i];
						if (!Exists[D] || RegionOf[D] != INDEX_NONE) { continue; }
						if (SSet.Contains(D) || XSet.Contains(D)) { continue; }
						Cand.AddUnique(D);
					}
				}
				Cand.Sort();
			}

			TArray<int32> XLocal = X;
			for (int32 C : Cand)
			{
				if (Done())
				{
					return;
				}
				const int32 t = CellType[C];
				// 이미 채워진 타입을 또 넣으려 하면 이 영역엔 포함 불가(가지치기).
				if (!(t >= 0 && Counts[t] >= 1))
				{
					S.Add(C);
					if (t >= 0) { ++Counts[t]; }
					Enumerate(AnchorIdx, S, Counts, XLocal);
					if (t >= 0) { --Counts[t]; }
					S.Pop();
				}
				XLocal.Add(C); // 이후 형제 분기에선 C 제외(중복 부분집합 방지)
			}
		}
	};
}

bool URetrievePuzzleGeneratorLibrary::GeneratePuzzle(const FRetrievePuzzleGenParams& Params, FRetrievePuzzleBoardDef& OutBoard, FRetrievePuzzleSolution& OutSolution)
{
	const int32 W = FMath::Max(2, Params.GridWidth);
	const int32 H = FMath::Max(2, Params.GridHeight);
	const int32 Num = W * H;

	// 원소 종류 정리(중복/무효 제거, 비면 기본 3종).
	TArray<FGameplayTag> Types;
	for (const FGameplayTag& T : Params.ElementTypes)
	{
		if (T.IsValid()) { Types.AddUnique(T); }
	}
	if (Types.Num() == 0)
	{
		Types.Add(RetrieveGameplayTags::Element_Fire);
		Types.Add(RetrieveGameplayTags::Element_Water);
		Types.Add(RetrieveGameplayTags::Element_Wind);
	}
	if (Types.Num() > 8)
	{
		Types.SetNum(8);
	}
	const int32 K = Types.Num();
	if (Num < K)
	{
		return false;
	}

	int32 DesiredN = (Params.RegionCount > 0) ? Params.RegionCount : FMath::Clamp(Num / (K * 2), 1, Num / K);
	DesiredN = FMath::Clamp(DesiredN, 1, Num / K);

	FRandomStream Rng;
	if (Params.Seed < 0) { Rng.GenerateNewSeed(); }
	else { Rng.Initialize(Params.Seed); }

	const int32 MaxAttempts = FMath::Max(1, Params.MaxAttempts);
	const int32 Budget = FMath::Max(1000, Params.SolverNodeBudget);

	bool bHaveFallback = false;
	FRetrievePuzzleBoardDef FallbackBoard;
	FRetrievePuzzleSolution FallbackSol;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		TArray<bool> Exists;
		Exists.Init(true, Num);
		int32 MExist = Num;
		if (Params.bAllowHoles && Params.HoleCount > 0)
		{
			// HoleCount만큼 구멍을 우선 확보(최소 K칸은 남겨 영역 1개는 가능하게).
			// 이전엔 MinKeep=DesiredN*K라 RegionCount가 크면 홀이 0개가 됐음.
			const int32 MinKeep = FMath::Max(K, Num - Params.HoleCount);
			PlaceHoles(W, H, Params.HoleCount, MinKeep, Rng, Exists, MExist);
		}

		const int32 N = FMath::Min(DesiredN, MExist / K);
		if (N < 1)
		{
			continue;
		}

		TArray<int32> RegionOf;
		if (!PartitionBoard(W, H, Exists, N, K, Rng, RegionOf))
		{
			continue;
		}

		TArray<FRetrievePuzzleElementPlacement> Placements;
		PlaceElements(W, H, Exists, RegionOf, N, Types, Rng, Placements);

		FRetrievePuzzleBoardDef Board;
		Board.GridWidth = W;
		Board.GridHeight = H;
		for (int32 i = 0; i < Num; ++i)
		{
			if (!Exists[i]) { Board.AbsentCells.Add(FIntPoint(i % W, i / W)); }
		}
		Board.ElementPlacements = Placements;

		FRetrievePuzzleSolution Sol;
		Sol.GridWidth = W;
		Sol.GridHeight = H;
		Sol.RegionIdPerCell.Init(INDEX_NONE, Num);
		for (int32 i = 0; i < Num; ++i)
		{
			if (Exists[i]) { Sol.RegionIdPerCell[i] = RegionOf[i]; }
		}

		if (Params.Uniqueness == ERetrievePuzzleUniqueness::SolvableOnly)
		{
			OutBoard = Board;
			OutSolution = Sol;
			return true;
		}

		if (!bHaveFallback)
		{
			FallbackBoard = Board;
			FallbackSol = Sol;
			bHaveFallback = true;
		}

		const int32 NumSol = CountSolutions(Board, 2, Budget);
		if (NumSol == 1)
		{
			OutBoard = Board;
			OutSolution = Sol;
			return true;
		}
		// 여러 해 또는 미확정(-1) → 다음 시도
	}

	if (bHaveFallback && Params.Uniqueness != ERetrievePuzzleUniqueness::StrictUnique)
	{
		OutBoard = FallbackBoard;
		OutSolution = FallbackSol;
		return true;
	}
	return false;
}

int32 URetrievePuzzleGeneratorLibrary::CountSolutions(const FRetrievePuzzleBoardDef& Board, int32 SolutionLimit, int32 NodeBudget)
{
	FPuzzleSolveModel Model;
	if (!Model.Init(Board))
	{
		return 0;
	}
	Model.SolutionLimit = FMath::Max(1, SolutionLimit);
	Model.Budget = FMath::Max(static_cast<int64>(1000), static_cast<int64>(NodeBudget));
	Model.Solve();

	if (Model.Solutions >= Model.SolutionLimit)
	{
		return Model.Solutions;
	}
	if (Model.bBudgetExceeded)
	{
		return -1; // 미확정
	}
	return Model.Solutions;
}
