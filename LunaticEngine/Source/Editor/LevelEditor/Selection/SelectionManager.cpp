#include "PCH/LunaticPCH.h"
#include "LevelEditor/Selection/SelectionManager.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SkinnedMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Render/Scene/FScene.h"
#include "Common/Gizmo/TransformGizmoTargets.h"

namespace
{
USceneComponent *ResolveLevelEditorSelectableComponent(USceneComponent *Component)
{
    if (!Component)
    {
        return nullptr;
    }

    USceneComponent *Target = Component;
    if (Target->IsEditorOnlyComponent())
    {
        if (Target->GetParent())
        {
            Target = Target->GetParent();
        }
        else if (AActor *Owner = Target->GetOwner())
        {
            Target = Owner->GetRootComponent();
        }
    }

    for (USceneComponent *Current = Target; Current; Current = Current->GetParent())
    {
        if (Current->IsA<USkinnedMeshComponent>())
        {
            return Current;
        }
    }

    return Target;
}
}


void FSelectionManager::Init()
{
    // SelectionManager는 선택 상태만 소유한다.
    // 시각적 기즈모의 생명주기는 각 FEditorViewportClient/FGizmoManager가 소유한다.
}

void FSelectionManager::SetWorld(UWorld *InWorld)
{
    if (World != InWorld)
    {
        // Selection/gizmo targets are only valid inside the currently edited world.
        // Keeping actors/components from a previous world can make an invisible
        // object appear selectable/transformable although it is not listed in the
        // current Outliner.
        ClearSelection();
    }

    World = InWorld;
    if (World)
    {
        World->GetScene().SetSelectedComponent(SelectedComponent);
    }
}

void FSelectionManager::Shutdown()
{
    ClearSelection();
    World = nullptr;
}

void FSelectionManager::Select(AActor *Actor)
{
    if (Actor && World && Actor->GetWorld() != World)
    {
        return;
    }

    if (SelectedActors.size() == 1 && SelectedActors.front() == Actor &&
        (!Actor || SelectedComponent == Actor->GetRootComponent()))
    {
        return;
    }

    // 기존 선택 해제
    for (AActor *Prev : SelectedActors)
        SetActorProxiesSelected(Prev, false);

    for (auto *Actor : SelectedActors)
    {
        Actor->SetActorSelected(false);
    }
    SelectedActors.clear();
    SelectedComponent = nullptr;

    if (Actor)
    {
        Actor->SetActorSelected(true);
        SelectedActors.push_back(Actor);
        SetActorProxiesSelected(Actor, true);
        SelectedComponent = Actor->GetRootComponent();
    }
    if (World)
    {
        World->GetScene().SetSelectedComponent(SelectedComponent);
    }
    SyncGizmo();
}

void FSelectionManager::SelectActors(const TArray<AActor *> &Actors)
{
    for (AActor *Prev : SelectedActors)
    {
        SetActorProxiesSelected(Prev, false);
        if (Prev)
        {
            Prev->SetActorSelected(false);
        }
    }

    SelectedActors.clear();
    SelectedComponent = nullptr;

    for (AActor *Actor : Actors)
    {
        if (!Actor || (World && Actor->GetWorld() != World))
        {
            continue;
        }

        if (std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end())
        {
            continue;
        }

        Actor->SetActorSelected(true);
        SelectedActors.push_back(Actor);
        SetActorProxiesSelected(Actor, true);
    }

    if (!SelectedActors.empty())
    {
        SelectedComponent = SelectedActors.front()->GetRootComponent();
    }
    if (World)
    {
        World->GetScene().SetSelectedComponent(SelectedComponent);
    }

    SyncGizmo();
}

void FSelectionManager::AddSelect(AActor *Actor)
{
    if (!Actor || (World && Actor->GetWorld() != World))
    {
        return;
    }

    if (std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end())
    {
        return;
    }

    Actor->SetActorSelected(true);
    SelectedActors.push_back(Actor);
    SetActorProxiesSelected(Actor, true);

    if (!SelectedComponent)
    {
        SelectedComponent = Actor->GetRootComponent();
    }

    if (World)
    {
        World->GetScene().SetSelectedComponent(SelectedComponent);
    }

    SyncGizmo();
}

void FSelectionManager::SelectRange(AActor *ClickedActor, const TArray<AActor *> &ActorList)
{
    if (!ClickedActor)
        return;

    // 클릭된 액터의 인덱스를 찾는다.
    int32 ClickedIdx = -1;
    for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
    {
        if (ActorList[i] == ClickedActor)
        {
            ClickedIdx = i;
            break;
        }
    }
    if (ClickedIdx == -1)
        return;

    // 이미 선택된 액터 중 ActorList에서 가장 가까운 인덱스를 찾는다.
    int32 AnchorIdx = ClickedIdx;
    int32 MinDist = INT_MAX;
    for (AActor *Sel : SelectedActors)
    {
        for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
        {
            if (ActorList[i] == Sel)
            {
                int32 Dist = std::abs(i - ClickedIdx);
                if (Dist < MinDist)
                {
                    MinDist = Dist;
                    AnchorIdx = i;
                }
                break;
            }
        }
    }

    // 선택 범위를 [min, max] 구간으로 교체한다.
    int32 Lo = std::min(AnchorIdx, ClickedIdx);
    int32 Hi = std::max(AnchorIdx, ClickedIdx);

    // 기존 선택 해제
    for (AActor *Prev : SelectedActors)
    {
        SetActorProxiesSelected(Prev, false);
        if (Prev)
        {
            Prev->SetActorSelected(false);
        }
    }

    SelectedActors.clear();
    SelectedComponent = nullptr;

    for (int32 i = Lo; i <= Hi; ++i)
    {
        if (ActorList[i])
        {
            ActorList[i]->SetActorSelected(true);
            SelectedActors.push_back(ActorList[i]);
            SetActorProxiesSelected(ActorList[i], true);
        }
    }

    if (!SelectedActors.empty())
    {
        SelectedComponent = SelectedActors.front()->GetRootComponent();
    }
    if (World)
    {
        World->GetScene().SetSelectedComponent(SelectedComponent);
    }

    SyncGizmo();
}

void FSelectionManager::ToggleSelect(AActor *Actor)
{
    if (!Actor || (World && Actor->GetWorld() != World))
        return;

    auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (It != SelectedActors.end())
    {
        SetActorProxiesSelected(Actor, false);
        Actor->SetActorSelected(false);
        SelectedActors.erase(It);
        if (SelectedComponent && SelectedComponent->GetOwner() == Actor)
        {
            SelectedComponent = SelectedActors.empty() ? nullptr : SelectedActors.front()->GetRootComponent();
        }
    }
    else
    {
        Actor->SetActorSelected(true);
        SelectedActors.push_back(Actor);
        SetActorProxiesSelected(Actor, true);
        if (SelectedActors.size() == 1)
        {
            SelectedComponent = Actor->GetRootComponent();
        }
    }
    if (World)
    {
        World->GetScene().SetSelectedComponent(SelectedComponent);
    }
    SyncGizmo();
}

void FSelectionManager::Deselect(AActor *Actor)
{
    auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (It != SelectedActors.end())
    {
        SetActorProxiesSelected(Actor, false);
        Actor->SetActorSelected(false);
        SelectedActors.erase(It);
        if (SelectedComponent && SelectedComponent->GetOwner() == Actor)
        {
            SelectedComponent = SelectedActors.empty() ? nullptr : SelectedActors.front()->GetRootComponent();
        }
    }
    if (World)
    {
        World->GetScene().SetSelectedComponent(SelectedComponent);
    }
    SyncGizmo();
}

void FSelectionManager::ClearSelection()
{
    if (SelectedActors.empty() && SelectedComponent == nullptr)
    {
        return;
    }

    for (AActor *Actor : SelectedActors)
    {
        SetActorProxiesSelected(Actor, false);
        if (Actor)
        {
            Actor->SetActorSelected(false);
        }
    }

    SelectedActors.clear();
    SelectedComponent = nullptr;
    if (World)
    {
        World->GetScene().SetSelectedComponent(nullptr);
    }
    SyncGizmo();
}

int32 FSelectionManager::DeleteSelectedActors()
{
    if (!World || SelectedActors.empty())
    {
        return 0;
    }

    TArray<AActor *> ActorsToDelete = SelectedActors;
    const int32 DeletedCount = static_cast<int32>(ActorsToDelete.size());

    // 파괴 전에 선택/기즈모 참조를 먼저 끊어 dangling target을 방지한다.
    ClearSelection();

    World->BeginDeferredPickingBVHUpdate();
    for (AActor *Actor : ActorsToDelete)
    {
        if (!Actor)
        {
            continue;
        }

        World->DestroyActor(Actor);
    }
    World->EndDeferredPickingBVHUpdate();

    return DeletedCount;
}

void FSelectionManager::Tick()
{
    // 여기서는 선택 상태만 다룬다. 기즈모의 변환/표시는 FGizmoManager가 동기화한다.
}

void FSelectionManager::SetGizmoEnabled(bool bEnabled)
{
    if (bGizmoEnabled == bEnabled)
    {
        return;
    }

    bGizmoEnabled = bEnabled;
    SyncGizmo();
}

void FSelectionManager::SelectComponent(USceneComponent *Component)
{
    if (!Component)
    {
        return;
    }
    if (World && Component->GetWorld() != World)
    {
        return;
    }

    // [버그 수정] 에디터 전용 컴포넌트(광원 아이콘 등)는 개별 조작 대상이 아니므로,
    // 부모 컴포넌트로 리다이렉트하여 함께 움직이도록 합니다.
    USceneComponent *Target = ResolveLevelEditorSelectableComponent(Component);

    if (SelectedComponent == Target)
    {
        return;
    }

    AActor *TargetOwner = Target ? Target->GetOwner() : nullptr;
    const bool bNeedsActorSelectionSync =
        TargetOwner &&
        (SelectedActors.size() != 1 || SelectedActors.front() != TargetOwner || !IsSelected(TargetOwner));
    if (bNeedsActorSelectionSync)
    {
        Select(TargetOwner);
    }

    SelectedComponent = Target;
    if (World)
    {
        World->GetScene().SetSelectedComponent(SelectedComponent);
    }

    SyncGizmo();
}

std::shared_ptr<ITransformGizmoTarget> FSelectionManager::MakeTransformGizmoTarget(
    const FEditorViewportClient* OwnerViewportClient,
    const bool* OwnerContextActiveFlag) const
{
    if (!bGizmoEnabled || !SelectedComponent)
    {
        return nullptr;
    }

    if (SelectedComponent->SupportsUIScreenPicking() || !SelectedComponent->SupportsWorldGizmo())
    {
        return nullptr;
    }

    // The gizmo target must come from the currently selected component itself.
    // Do not overwrite it with GetPrimarySelection()->RootComponent here: after
    // tab switches or multi-selection edits, the primary actor can lag behind the
    // actual current transform target and make the gizmo keep driving an older actor.
    USceneComponent* TargetComponent = ResolveLevelEditorSelectableComponent(SelectedComponent);
    if (!TargetComponent)
    {
        return nullptr;
    }

    return std::make_shared<FSceneComponentTransformGizmoTarget>(
        TargetComponent,
        OwnerViewportClient,
        OwnerContextActiveFlag,
        OwnerViewportClient ? OwnerViewportClient->GetEditorContextEpoch() : 0);
}

void FSelectionManager::SetActorProxiesSelected(AActor *Actor, bool bSelected)
{
    if (!Actor || !World)
        return;

    FScene &Scene = World->GetScene();
    for (UPrimitiveComponent *Prim : Actor->GetPrimitiveComponents())
    {
        if (FPrimitiveSceneProxy *Proxy = Prim->GetSceneProxy())
        {
            Scene.SetProxySelected(Proxy, bSelected);
        }
    }
}

void FSelectionManager::SyncGizmo()
{
    // 의도된 no-op이다. SelectionManager는 더 이상 UGizmoVisualComponent를 소유하지 않는다.
    // FLevelEditorViewportClient가 MakeTransformGizmoTarget()을 읽어 FGizmoManager를 갱신한다.
}
