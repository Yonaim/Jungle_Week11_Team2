#include "PCH/LunaticPCH.h"
#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Proxy/BillboardSceneProxy.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Platform/Paths.h"
#include "Render/Types/EditorViewportScale.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>

#include <cstring>

IMPLEMENT_CLASS(UBillboardComponent, UPrimitiveComponent)

namespace
{
FString ResolveLegacyBillboardTexturePath(const FString& InPath)
{
	const FString FileName = std::filesystem::path(InPath).filename().string();

	if (FileName == "AmbientLight.uasset")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.AmbientLight"));
	}
	if (FileName == "DirectionalLight.uasset")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.DirectionalLight"));
	}
	if (FileName == "PointLight.uasset")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.PointLight"));
	}
	if (FileName == "SpotLight.uasset")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.SpotLight"));
	}
	if (FileName == "HeightFog.uasset")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.HeightFog"));
	}
	if (FileName == "Decal.uasset")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.Decal"));
	}

	return InPath;
}

const char* GetDefaultBillboardFallbackIconKey(const AActor* Actor)
{
	if (!Actor)
	{
		return "Editor.Icon.Actor";
	}

	const FString ClassName = Actor->GetClass()->GetName();
	if (ClassName.find("Character") != FString::npos)
	{
		return "Editor.Icon.Character";
	}
	if (ClassName.find("Pawn") != FString::npos)
	{
		return "Editor.Icon.Pawn";
	}
	if (ClassName.find("Camera") != FString::npos)
	{
		return "Editor.Icon.Camera";
	}
	if (ClassName.find("SpotLight") != FString::npos)
	{
		return "Editor.Icon.SpotLight";
	}
	if (ClassName.find("PointLight") != FString::npos)
	{
		return "Editor.Icon.PointLight";
	}
	if (ClassName.find("DirectionalLight") != FString::npos)
	{
		return "Editor.Icon.DirectionalLight";
	}
	if (ClassName.find("AmbientLight") != FString::npos)
	{
		return "Editor.Icon.AmbientLight";
	}
	if (ClassName.find("HeightFog") != FString::npos)
	{
		return "Editor.Icon.HeightFog";
	}
	if (ClassName.find("Decal") != FString::npos)
	{
		return "Editor.Icon.Decal";
	}

	return "Editor.Icon.Actor";
}
}

FPrimitiveSceneProxy* UBillboardComponent::CreateSceneProxy()
{
	return new FBillboardSceneProxy(this);
}

void UBillboardComponent::UpdateWorldAABB() const
{
	const FVector WorldScale = GetWorldScale();
	const float HalfHeight = std::abs(WorldScale.Z * Height) * 0.5f;
	const float HalfWidth = std::abs(WorldScale.Y * Width) * 0.5f;
	const float Radius = std::sqrt((HalfWidth * HalfWidth) + (HalfHeight * HalfHeight));
	const FVector Extent(Radius, Radius, Radius);
	const FVector WorldCenter = GetWorldLocation();

	WorldAABBMinLocation = WorldCenter - Extent;
	WorldAABBMaxLocation = WorldCenter + Extent;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void UBillboardComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << bIsBillboard;
	Ar << TextureSlot.Path;
	Ar << Width;
	Ar << Height;
}

void UBillboardComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	if (!TextureSlot.Path.empty() && TextureSlot.Path != "None")
	{
		ResolveTextureFromPath(TextureSlot.Path);
	}

	EnsureVisibleFallbackTexture();
}

void UBillboardComponent::SetTexture(UTexture2D* InTexture)
{
	Texture = InTexture;
	if (Texture)
	{
		TextureSlot.Path = Texture->GetSourcePath();
	}
	else
	{
		TextureSlot.Path = "None";
	}
	// 텍스처 변경 시 렌더 상태와 프록시를 함께 갱신합니다.
	MarkProxyDirty(EDirtyFlag::Material);
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UBillboardComponent::SetMaterial(UMaterial* InMaterial)
{
	UTexture2D* PreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(InMaterial);
	if (PreviewTexture)
	{
		SetTexture(PreviewTexture);
	}
	else
	{
		SetTexture(nullptr);
		if (InMaterial)
		{
			TextureSlot.Path = InMaterial->GetAssetPathFileName();
		}
	}
}

void UBillboardComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Billboard", EPropertyType::Bool, &bIsBillboard });
	OutProps.push_back({ "Texture", EPropertyType::TextureSlot, &TextureSlot });
	OutProps.push_back({ "Width",  EPropertyType::Float, &Width,  0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Height", EPropertyType::Float, &Height, 0.1f, 100.0f, 0.1f });
}

void UBillboardComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0)
	{
		if (TextureSlot.Path == "None" || TextureSlot.Path.empty())
		{
			SetTexture(nullptr);
		}
		else
		{
			ResolveTextureFromPath(TextureSlot.Path);
		}
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Width") == 0 || strcmp(PropertyName, "Height") == 0)
	{
		// Width/Height는 빌보드 쿼드 크기를 결정하므로 트랜스폼과 월드 바운드를 모두 갱신해야 합니다.
		MarkProxyDirty(EDirtyFlag::Transform);
		MarkWorldBoundsDirty();
	}
	else if (strcmp(PropertyName, "Billboard") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Transform);
	}

	EnsureVisibleFallbackTexture();
}

void UBillboardComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	if (!GetOwner() || !GetOwner()->GetWorld()) return;

	const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();
	if (!ActiveCamera) return;

	FVector WorldLocation = GetWorldLocation();
	FVector CameraForward = ActiveCamera->GetForwardVector().Normalized();
	FVector Forward = CameraForward * -1;
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f); // 임시 Up축 변경
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

	UpdateWorldAABB();
}

FMatrix UBillboardComponent::ComputeBillboardMatrix(const FVector& CameraForward) const
{
	// TickComponent와 동일한 로직
	FVector Forward = (CameraForward * -1.0f).Normalized();
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f);
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	return FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(GetWorldLocation());
}

float UBillboardComponent::ComputeRenderedScreenScale(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth,
	float BillboardScale, float ViewportHeight) const
{
	if (!IsEditorOnly())
	{
		return 1.0f;
	}

	const float Scale = FEditorViewportScale::ComputeWorldScale(
		CameraLocation,
		GetWorldLocation(),
		bIsOrtho,
		OrthoWidth,
		ViewportHeight,
		FEditorViewportScaleRules::Billboard);

	return Scale * BillboardScale;
}

bool UBillboardComponent::LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult)
{
	if (!IsVisible())
	{
		return false;
	}

	const float ScreenScale = IsEditorOnly()
		? (std::max)(0.0001f, LastRenderedEditorScreenScale)
		: 1.0f;

	return IntersectBillboard(Ray, OutHitResult, true, ScreenScale);
}

bool UBillboardComponent::IntersectBillboard(const FRay& Ray, FRayHitResult& OutHitResult, bool bRespectTextureAlpha,
	float ScreenScale) const
{
	FMatrix BillboardWorldMatrix = ComputeBillboardMatrix(Ray.Direction);
	const float ScaledWidth = Width * ScreenScale;
	const float ScaledHeight = Height * ScreenScale;
	BillboardWorldMatrix.M[1][0] *= ScaledWidth;
	BillboardWorldMatrix.M[1][1] *= ScaledWidth;
	BillboardWorldMatrix.M[1][2] *= ScaledWidth;
	BillboardWorldMatrix.M[2][0] *= ScaledHeight;
	BillboardWorldMatrix.M[2][1] *= ScaledHeight;
	BillboardWorldMatrix.M[2][2] *= ScaledHeight;
	const FMatrix InvWorldMatrix = BillboardWorldMatrix.GetInverse();

	FRay LocalRay;
	LocalRay.Origin = InvWorldMatrix.TransformPositionWithW(Ray.Origin);
	LocalRay.Direction = InvWorldMatrix.TransformVector(Ray.Direction).Normalized();

	if (std::abs(LocalRay.Direction.X) < 0.00111f)
	{
		return false;
	}

	const float T = -LocalRay.Origin.X / LocalRay.Direction.X;
	if (T < 0.0f)
	{
		return false;
	}

	const FVector LocalHitPos = LocalRay.Origin + LocalRay.Direction * T;
	if (LocalHitPos.Y < -0.5f || LocalHitPos.Y > 0.5f ||
		LocalHitPos.Z < -0.5f || LocalHitPos.Z > 0.5f)
	{
		return false;
	}

	if (bRespectTextureAlpha && Texture)
	{
		const float U = LocalHitPos.Y + 0.5f;
		const float V = 0.5f - LocalHitPos.Z;
		float Alpha = 1.0f;
		if (Texture->SampleAlpha(U, V, Alpha) && Alpha < 0.5f)
		{
			return false;
		}
	}

	const FVector WorldHitPos = BillboardWorldMatrix.TransformPositionWithW(LocalHitPos);
	OutHitResult.Distance = (WorldHitPos - Ray.Origin).Length();
	OutHitResult.WorldHitLocation = WorldHitPos;
	OutHitResult.HitComponent = const_cast<UBillboardComponent*>(this);
	OutHitResult.bHit = true;
	return true;
}

bool UBillboardComponent::ResolveTextureFromPath(const FString& InPath)
{
	if (InPath.empty() || InPath == "None")
	{
		SetTexture(nullptr);
		return EnsureVisibleFallbackTexture();
	}

	const FString ResolvedPath = ResolveLegacyBillboardTexturePath(InPath);
	const std::filesystem::path Path(FPaths::ToWide(ResolvedPath));
	std::wstring Ext = Path.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), towlower);

	if (Ext == L".uasset")
	{
		UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(ResolvedPath);
		if (!LoadedMat)
		{
			return EnsureVisibleFallbackTexture();
		}

		UTexture2D* PreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(LoadedMat);
		if (!PreviewTexture)
		{
			SetTexture(nullptr);
			TextureSlot.Path = ResolvedPath;
			return EnsureVisibleFallbackTexture();
		}

		SetTexture(PreviewTexture);
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	UTexture2D* LoadedTexture = UTexture2D::LoadFromFile(ResolvedPath, Device);
	if (!LoadedTexture)
	{
		return EnsureVisibleFallbackTexture();
	}

	SetTexture(LoadedTexture);
	return true;
}

FString UBillboardComponent::GetFallbackTexturePath() const
{
	const AActor* OwnerActor = GetOwner();
	const char* const PreferredKeys[] = {
		GetDefaultBillboardFallbackIconKey(OwnerActor),
		"Editor.Billboard.Decal",
		"Editor.Icon.Actor"
	};

	for (const char* Key : PreferredKeys)
	{
		const FString ResolvedPath = FResourceManager::Get().ResolvePath(FName(Key));
		if (!ResolvedPath.empty())
		{
			return ResolvedPath;
		}
	}

	return FString();
}

bool UBillboardComponent::EnsureVisibleFallbackTexture()
{
	if (Texture && Texture->GetSRV())
	{
		return true;
	}

	const FString FallbackPath = GetFallbackTexturePath();
	if (FallbackPath.empty())
	{
		return false;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (UTexture2D* FallbackTexture = UTexture2D::LoadFromFile(FallbackPath, Device))
	{
		SetTexture(FallbackTexture);
		return true;
	}

	return false;
}
