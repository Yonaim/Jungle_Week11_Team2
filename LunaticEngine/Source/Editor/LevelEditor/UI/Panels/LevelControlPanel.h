#pragma once

#include "Common/UI/Base/UIElement.h"

class FLevelControlPanel : public FUIElement
{
  public:
    virtual void Render(float DeltaTime) override;
};
