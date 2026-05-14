#pragma once
#include "Common/UI/DragDrop/DragSource.h"
#include "LevelEditor/UI/Panels/ContentBrowser/ContentBrowserElement.h"
#include "Render/Types/RenderTypes.h"
#include "ImGui/imgui.h"


class PrefabDragSource final : public FDragSource
{
  public:
    ~PrefabDragSource()
    {
        delete ActorImage;
    }

    ID3D11Texture2D *GetImage() const
    {
        return ActorImage;
    }
    void SetImage(ID3D11Texture2D *InImage)
    {
        ActorImage = InImage;
    }

  private:
    void RenderSource(ImVec2 InSize) override
    {
        ImGui::Image(ActorImage, InSize);
    }

  private:
    ImVec2 Size;
    ID3D11Texture2D *ActorImage;

    // TODO: Renderer에서 ActorImage를 직접 생성
};

class FPrefabElement : public ContentBrowserElement
{
  public:
    virtual ~FPrefabElement()
    {
        delete DragSource;
    }

    virtual void Render(ContentBrowserContext &Context) override
    {
        DragSource->Render(Context.ContentSize);
    }

  private:
    PrefabDragSource *DragSource;
};
