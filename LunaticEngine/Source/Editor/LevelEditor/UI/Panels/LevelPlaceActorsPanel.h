#pragma once

#include "Common/UI/Base/UIElement.h"
#include "LevelEditor/Viewport/LevelViewportLayout.h"

class FLevelPlaceActorsPanel : public FUIElement
{
  public:
    enum class EPlaceActorCategory : uint8
    {
        Basic,
        Text,
        UI,
        Lights,
        Shapes,
        VFX
    };

    struct FPlaceActorEntry
    {
        const char *Label = "";
        const char *SearchKey = "";
        const char *IconKey = "";
        FLevelViewportLayout::EViewportPlaceActorType Type = FLevelViewportLayout::EViewportPlaceActorType::Cube;
        EPlaceActorCategory Category = EPlaceActorCategory::Basic;
    };

    void Render(float DeltaTime) override;

  private:
    void RenderCategorySidebar();
    void RenderActorGrid();
    bool MatchesSearch(const FPlaceActorEntry &Entry) const;
    void SpawnActor(const FPlaceActorEntry &Entry);

    EPlaceActorCategory ActiveCategory = EPlaceActorCategory::Basic;
    char SearchBuffer[128] = {};
};
