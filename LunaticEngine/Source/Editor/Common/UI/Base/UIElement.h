#pragma once

#include "Core/CoreTypes.h"

class UEditorEngine;

class FUIElement
{
public:
	virtual ~FUIElement() = default;

	virtual void Init(UEditorEngine* InEditorEngine);
	virtual void Render(float DeltaTime) = 0;

protected:
	UEditorEngine* EditorEngine = nullptr;
};


