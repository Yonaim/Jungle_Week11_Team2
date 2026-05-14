#include "PCH/LunaticPCH.h"
#include "Mesh/FbxImporter.h"
#include "Mesh/FbxCommon.h"

#include "Mesh/SkeletalMeshCommon.h"
#include "Mesh/StaticMeshCommon.h"

#include <algorithm>
#include <cmath>

#if defined(_WIN64)
namespace
{
	// FBX SkeletalMesh 변환:
	// 1) 메시 정점/인덱스 추출
	// 2) Skin/Cluster에서 본 영향 수집
	// 3) 본 계층/InverseBindPose 구성
	// 4) 머티리얼 섹션 재구성
	using FControlPointVertexMap = std::unordered_map<FbxMesh*, std::unordered_map<int32, TArray<uint32>>>;
	using FMeshNodeVertexMap = std::unordered_map<FbxNode*, TArray<uint32>>;

	// Import 과정에서만 사용되는 임시 데이터 구조체
	struct FSkeletalImportScratch
	{
		TArray<FbxNode*> MeshNodes;						// FBX Scene에서 Mesh가 있는 노드
		FControlPointVertexMap ControlPointToVertices;	// FBX Mesh의 Control Point가 영향을 미치는 메시 정점 인덱스 매핑
		FMeshNodeVertexMap MeshNodeVertices;			// FBX Node가 영향을 미치는 메시 정점 인덱스 매핑
		TArray<FMatrix> BindGlobalMatrices;				// Bone Bind Pose Global Matrix 
	};

	bool IsSkeletonNode(FbxNode* Node)
	{
		if (!Node || !Node->GetNodeAttribute())
		{
			return false;
		}

		return Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton;
	}

	FbxNode* FindParentSkeletonNode(FbxNode* Node)
	{
		FbxNode* ParentNode = Node ? Node->GetParent() : nullptr;
		while (ParentNode)
		{
			if (IsSkeletonNode(ParentNode))
			{
				return ParentNode;
			}
			ParentNode = ParentNode->GetParent();
		}
		return nullptr;
	}

	FMatrix ConvertFbxAMatrixToFMatrix(const FbxAMatrix& Matrix)
	{
		// Points can be converted as Remap(Source * M), but matrices must operate
		// on already-remapped engine vectors. Sample the source vectors that map
		// to each engine basis axis to build P^-1 * M * P.
		const FVector Origin = FFbxCommon::RemapVector(Matrix.MultT(FbxVector4(0.0, 0.0, 0.0, 1.0)));
		const FVector AxisX = FFbxCommon::RemapVector(Matrix.MultT(FbxVector4(1.0, 0.0, 0.0, 1.0))) - Origin;
		const FVector AxisY = FFbxCommon::RemapVector(Matrix.MultT(FbxVector4(0.0, 1.0, 0.0, 1.0))) - Origin;
		const FVector AxisZ = FFbxCommon::RemapVector(Matrix.MultT(FbxVector4(0.0, 0.0, 1.0, 1.0))) - Origin;

		return FMatrix(
			AxisX.X, AxisX.Y, AxisX.Z, 0.0f,
			AxisY.X, AxisY.Y, AxisY.Z, 0.0f,
			AxisZ.X, AxisZ.Y, AxisZ.Z, 0.0f,
			Origin.X, Origin.Y, Origin.Z, 1.0f);
	}

	FMatrix GetBoneBindGlobalMatrix(FbxNode* BoneNode, FbxCluster* Cluster)
	{
		if (Cluster)
		{
			FbxAMatrix TransformLinkMatrix;
			Cluster->GetTransformLinkMatrix(TransformLinkMatrix);
			return ConvertFbxAMatrixToFMatrix(TransformLinkMatrix);
		}

		return ConvertFbxAMatrixToFMatrix(BoneNode->EvaluateGlobalTransform());
	}

	FTransform MakeTransformFromMatrix(const FMatrix& Matrix)
	{
		FVector AxisX(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
		FVector AxisY(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
		FVector AxisZ(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

		FVector Scale(AxisX.Length(), AxisY.Length(), AxisZ.Length());
		if (Scale.X > 1e-6f)
		{
			AxisX /= Scale.X;
		}
		if (Scale.Y > 1e-6f)
		{
			AxisY /= Scale.Y;
		}
		if (Scale.Z > 1e-6f)
		{
			AxisZ /= Scale.Z;
		}

		FMatrix RotationMatrix(
			AxisX.X, AxisX.Y, AxisX.Z, 0.0f,
			AxisY.X, AxisY.Y, AxisY.Z, 0.0f,
			AxisZ.X, AxisZ.Y, AxisZ.Z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);

		if (RotationMatrix.GetBasisDeterminant3x3() < 0.0f)
		{
			// Preserve FBX axis-remap reflection through FTransform's signed scale.
			Scale.X = -Scale.X;
			RotationMatrix.M[0][0] = -RotationMatrix.M[0][0];
			RotationMatrix.M[0][1] = -RotationMatrix.M[0][1];
			RotationMatrix.M[0][2] = -RotationMatrix.M[0][2];
		}

		return FTransform(
			Matrix.GetLocation(),
			RotationMatrix.ToQuat(),
			Scale);
	}

	void ApplyBoneBindGlobal(FBoneInfo& Bone, const FMatrix& BindGlobalMatrix)
	{
		Bone.InverseBindPose = BindGlobalMatrix.GetInverse();
		Bone.LocalBindTransform = MakeTransformFromMatrix(BindGlobalMatrix);
	}

	void RebuildLocalBindTransforms(FSkeletalMesh& OutMesh, const TArray<FMatrix>& BindGlobalMatrices)
	{
		TArray<FMatrix> RebuiltGlobalMatrices(OutMesh.Bones.size(), FMatrix::Identity);

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(OutMesh.Bones.size()); ++BoneIndex)
		{
			if (BoneIndex >= static_cast<int32>(BindGlobalMatrices.size()))
			{
				continue;
			}

			FBoneInfo& Bone = OutMesh.Bones[BoneIndex];
			const FMatrix& BindGlobalMatrix = BindGlobalMatrices[BoneIndex];

			if (Bone.ParentIndex != InvalidBoneIndex && Bone.ParentIndex < static_cast<int32>(BindGlobalMatrices.size()))
			{
				const FMatrix LocalBindMatrix = BindGlobalMatrix * BindGlobalMatrices[Bone.ParentIndex].GetInverse();
				Bone.LocalBindTransform = MakeTransformFromMatrix(LocalBindMatrix);
			}
			else
			{
				Bone.LocalBindTransform = MakeTransformFromMatrix(BindGlobalMatrix);
			}

			const FMatrix LocalBindMatrix = Bone.LocalBindTransform.ToMatrix();
			if (Bone.ParentIndex != InvalidBoneIndex && Bone.ParentIndex < static_cast<int32>(RebuiltGlobalMatrices.size()))
			{
				RebuiltGlobalMatrices[BoneIndex] = LocalBindMatrix * RebuiltGlobalMatrices[Bone.ParentIndex];
			}
			else
			{
				RebuiltGlobalMatrices[BoneIndex] = LocalBindMatrix;
			}
		}

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(OutMesh.Bones.size()); ++BoneIndex)
		{
			// Current bind pose is reconstructed from FTransform. Make the inverse
			// bind pose match that exact runtime representation so bind-pose
			// skinning is identity and does not tilt skinned-only sections.
			OutMesh.Bones[BoneIndex].InverseBindPose = RebuiltGlobalMatrices[BoneIndex].GetInverse();
		}
	}

	void AddControlPointVertex(FControlPointVertexMap& ControlPointToVertices, FbxMesh* Mesh, int32 ControlPointIndex, uint32 VertexIndex)
	{
		TArray<uint32>& Vertices = ControlPointToVertices[Mesh][ControlPointIndex];
		if (std::find(Vertices.begin(), Vertices.end(), VertexIndex) == Vertices.end())
		{
			Vertices.push_back(VertexIndex);
		}
	}

	void AddMeshNodeVertex(FMeshNodeVertexMap& MeshNodeVertices, FbxNode* Node, uint32 VertexIndex)
	{
		TArray<uint32>& Vertices = MeshNodeVertices[Node];
		if (std::find(Vertices.begin(), Vertices.end(), VertexIndex) == Vertices.end())
		{
			Vertices.push_back(VertexIndex);
		}
	}

	bool TryGetMeshBindGlobalMatrix(FbxMesh* Mesh, FbxAMatrix& OutMatrix)
	{
		if (!Mesh)
		{
			return false;
		}

		const int32 DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
		for (int32 DeformerIndex = 0; DeformerIndex < DeformerCount; ++DeformerIndex)
		{
			FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(DeformerIndex, FbxDeformer::eSkin));
			if (!Skin)
			{
				continue;
			}

			const int32 ClusterCount = Skin->GetClusterCount();
			for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
			{
				FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
				if (!Cluster)
				{
					continue;
				}

				Cluster->GetTransformMatrix(OutMatrix);
				return true;
			}
		}

		return false;
	}

	void ConvertMeshNode(
		const FString& FbxFilePath,
		FbxNode* Node,
		FFbxInfo& Context,
		FSkeletalMesh& OutMesh,
		FSkeletalImportScratch& Scratch)
	{
		FbxMesh* Mesh = Node ? Node->GetMesh() : nullptr;
		if (!Mesh)
		{
			return;
		}

		Scratch.MeshNodes.push_back(Node);

		const char* UVSetName = nullptr;
		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		if (UVSetNames.GetCount() > 0)
		{
			UVSetName = UVSetNames[0];
		}

		FbxAMatrix GlobalTransform = Node->EvaluateGlobalTransform();
		FbxAMatrix MeshBindGlobalTransform;
		if (TryGetMeshBindGlobalMatrix(Mesh, MeshBindGlobalTransform))
		{
			// Skinned vertices must be baked into the same bind-time global space
			// used by cluster link matrices, otherwise debug bones and mesh drift apart.
			GlobalTransform = MeshBindGlobalTransform;
		}
		std::unordered_map<FFbxVertexKey, uint32, FFbxVertexKeyHash> VertexMap;
		const int32 PolygonCount = Mesh->GetPolygonCount();

		for (int32 PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
		{
			if (Mesh->GetPolygonSize(PolygonIndex) != 3)
			{
				continue;
			}

			int32 LocalMaterialIndex = FFbxCommon::GetPolygonMaterialIndex(Mesh, PolygonIndex);
			if (LocalMaterialIndex < 0)
			{
				LocalMaterialIndex = 0;
			}

			const int32 MaterialIndex = FFbxCommon::FindOrAddMaterial(FbxFilePath, Node, LocalMaterialIndex, Context);
			if (static_cast<size_t>(MaterialIndex) >= Context.FacesPerMaterial.size())
			{
				Context.FacesPerMaterial.resize(static_cast<size_t>(MaterialIndex) + 1);
			}

			uint32 TriangleIndices[3] = {};
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const int32 ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);
				const FFbxVertexKey Key{ ControlPointIndex, PolygonIndex, CornerIndex, MaterialIndex };

				auto It = VertexMap.find(Key);
				if (It != VertexMap.end())
				{
					TriangleIndices[CornerIndex] = It->second;
					AddControlPointVertex(Scratch.ControlPointToVertices, Mesh, ControlPointIndex, It->second);
					AddMeshNodeVertex(Scratch.MeshNodeVertices, Node, It->second);
					continue;
				}

				FNormalVertex Vertex;
				Vertex.pos = FFbxCommon::RemapVector(GlobalTransform.MultT(Mesh->GetControlPointAt(ControlPointIndex)));

				FbxVector4 FbxNormal(0.0, 0.0, 1.0, 0.0);
				FFbxCommon::ReadNormal(Mesh, ControlPointIndex, PolygonIndex, CornerIndex, FbxNormal);
				Vertex.normal = FFbxCommon::TransformNormalByMatrix(GlobalTransform, FbxNormal);

				FbxVector2 FbxUV(0.0, 0.0);
				bool bUnmapped = false;
				Vertex.tex = UVSetName && Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetName, FbxUV, bUnmapped) && !bUnmapped
					? FFbxCommon::RemapUV(FbxUV)
					: FVector2(0.0f, 0.0f);
				Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

				const uint32 NewVertexIndex = static_cast<uint32>(OutMesh.Vertices.size());
				OutMesh.Vertices.push_back(Vertex);
				VertexMap.emplace(Key, NewVertexIndex);
				TriangleIndices[CornerIndex] = NewVertexIndex;
				AddControlPointVertex(Scratch.ControlPointToVertices, Mesh, ControlPointIndex, NewVertexIndex);
				AddMeshNodeVertex(Scratch.MeshNodeVertices, Node, NewVertexIndex);
			}

			const uint32 FaceStart = static_cast<uint32>(OutMesh.Indices.size());
			OutMesh.Indices.push_back(TriangleIndices[0]);
			OutMesh.Indices.push_back(TriangleIndices[2]);
			OutMesh.Indices.push_back(TriangleIndices[1]);
			Context.FacesPerMaterial[MaterialIndex].push_back(FaceStart);
		}
	}

	void TraverseNodes(
		const FString& FbxFilePath,
		FbxNode* Node,
		FFbxInfo& Context,
		FSkeletalMesh& OutMesh,
		FSkeletalImportScratch& Scratch)
	{
		if (!Node)
		{
			return;
		}

		ConvertMeshNode(FbxFilePath, Node, Context, OutMesh, Scratch);

		const int32 ChildCount = Node->GetChildCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			TraverseNodes(FbxFilePath, Node->GetChild(ChildIndex), Context, OutMesh, Scratch);
		}
	}

	int32 FindOrAddBone(FSkeletalMesh& OutMesh, FbxNode* BoneNode, FbxCluster* Cluster, TArray<FMatrix>& BindGlobalMatrices)
	{
		if (!BoneNode)
		{
			return InvalidBoneIndex;
		}

		std::string BoneName = BoneNode->GetName();

		// 이미 등록된 뼈인지 확인
		for (int32 i = 0; i < (int32)OutMesh.Bones.size(); ++i)
		{
			if (OutMesh.Bones[i].Name == BoneName)
			{
				if (Cluster)
				{
					const FMatrix BindGlobalMatrix = GetBoneBindGlobalMatrix(BoneNode, Cluster);
					ApplyBoneBindGlobal(OutMesh.Bones[i], BindGlobalMatrix);
					if (i < static_cast<int32>(BindGlobalMatrices.size()))
					{
						BindGlobalMatrices[i] = BindGlobalMatrix;
					}
				}
				return i;
			}
		}

		FBoneInfo NewBone;
		NewBone.Name = BoneName;

		FbxNode* ParentNode = FindParentSkeletonNode(BoneNode);
		if (ParentNode)
		{
			NewBone.ParentIndex = FindOrAddBone(OutMesh, ParentNode, nullptr, BindGlobalMatrices);
		}
		else
		{
			NewBone.ParentIndex = InvalidBoneIndex;
		}

		const FMatrix BindGlobalMatrix = GetBoneBindGlobalMatrix(BoneNode, Cluster);
		ApplyBoneBindGlobal(NewBone, BindGlobalMatrix);

		OutMesh.Bones.push_back(NewBone);
		BindGlobalMatrices.push_back(BindGlobalMatrix);
		return static_cast<int32>(OutMesh.Bones.size() - 1);
	}

	void RegisterSkeletonHierarchyBones(FbxNode* Node, FSkeletalMesh& OutMesh, TArray<FMatrix>& BindGlobalMatrices)
	{
		if (!Node)
		{
			return;
		}

		if (IsSkeletonNode(Node))
		{
			FindOrAddBone(OutMesh, Node, nullptr, BindGlobalMatrices);
		}

		const int32 ChildCount = Node->GetChildCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			RegisterSkeletonHierarchyBones(Node->GetChild(ChildIndex), OutMesh, BindGlobalMatrices);
		}
	}

	void AddBoneInfluence(FSkinWeight& SkinWeight, int32 BoneIndex, float Weight)
	{
		if (Weight <= 0.0f || BoneIndex == InvalidBoneIndex)
		{
			return;
		}

		for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
		{
			if (SkinWeight.BoneIndices[InfluenceIndex] == BoneIndex)
			{
				SkinWeight.BoneWeights[InfluenceIndex] += Weight;
				return;
			}
		}

		for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
		{
			if (SkinWeight.BoneIndices[InfluenceIndex] == InvalidBoneIndex)
			{
				SkinWeight.BoneIndices[InfluenceIndex] = BoneIndex;
				SkinWeight.BoneWeights[InfluenceIndex] = Weight;
				return;
			}
		}

		int32 SmallestInfluenceIndex = 0;
		for (int32 InfluenceIndex = 1; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
		{
			if (SkinWeight.BoneWeights[InfluenceIndex] < SkinWeight.BoneWeights[SmallestInfluenceIndex])
			{
				SmallestInfluenceIndex = InfluenceIndex;
			}
		}

		if (Weight > SkinWeight.BoneWeights[SmallestInfluenceIndex])
		{
			SkinWeight.BoneIndices[SmallestInfluenceIndex] = BoneIndex;
			SkinWeight.BoneWeights[SmallestInfluenceIndex] = Weight;
		}
	}

	void NormalizeSkinWeights(FSkeletalMesh& OutMesh)
	{
		// 정점별 총합을 1로 맞춰 스키닝 시 가중치 에너지 보존을 유지한다.
		for (FSkinWeight& SkinWeight : OutMesh.SkinWeights)
		{
			float TotalWeight = 0.0f;
			for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
			{
				TotalWeight += SkinWeight.BoneWeights[InfluenceIndex];
			}

			if (TotalWeight <= 0.0f)
			{
				continue;
			}

			const float InvTotalWeight = 1.0f / TotalWeight;
			for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
			{
				SkinWeight.BoneWeights[InfluenceIndex] *= InvTotalWeight;
			}
		}
	}

	bool HasSkinDeformer(FbxMesh* Mesh)
	{
		return Mesh && Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
	}

	void AssignRigidBoneInfluence(FSkinWeight& SkinWeight, int32 BoneIndex)
	{
		for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
		{
			SkinWeight.BoneIndices[InfluenceIndex] = InvalidBoneIndex;
			SkinWeight.BoneWeights[InfluenceIndex] = 0.0f;
		}

		SkinWeight.BoneIndices[0] = BoneIndex;
		SkinWeight.BoneWeights[0] = 1.0f;
	}

	void BindRigidMeshesToParentBones(FSkeletalImportScratch& Scratch, FSkeletalMesh& OutMesh)
	{
		for (FbxNode* Node : Scratch.MeshNodes)
		{
			FbxMesh* Mesh = Node ? Node->GetMesh() : nullptr;
			if (!Mesh || HasSkinDeformer(Mesh))
			{
				continue;
			}

			FbxNode* ParentBoneNode = FindParentSkeletonNode(Node);
			if (!ParentBoneNode)
			{
				continue;
			}

			const int32 BoneIndex = FindOrAddBone(OutMesh, ParentBoneNode, nullptr, Scratch.BindGlobalMatrices);
			if (BoneIndex == InvalidBoneIndex)
			{
				continue;
			}

			auto VertexListIt = Scratch.MeshNodeVertices.find(Node);
			if (VertexListIt == Scratch.MeshNodeVertices.end())
			{
				continue;
			}

			for (uint32 VertexIndex : VertexListIt->second)
			{
				if (VertexIndex < OutMesh.SkinWeights.size())
				{
					AssignRigidBoneInfluence(OutMesh.SkinWeights[VertexIndex], BoneIndex);
				}
			}
		}
	}

	void FillSkinWeights(FFbxInfo& Context, FSkeletalImportScratch& Scratch, FSkeletalMesh& OutMesh)
	{
		OutMesh.SkinWeights.resize(OutMesh.Vertices.size());

		// Cluster에 weight가 없는 leaf/end bone은 Cluster->GetLink()로 등장하지 않는다.
		// Skin weight 처리 전에 scene skeleton hierarchy 전체를 등록해 head_end 같은 말단 본도 보존한다.
		RegisterSkeletonHierarchyBones(Context.Scene ? Context.Scene->GetRootNode() : nullptr, OutMesh, Scratch.BindGlobalMatrices);

		for (FbxNode* Node : Scratch.MeshNodes)
		{
			FbxMesh* Mesh = Node->GetMesh();
			if (!Mesh)
			{
				continue;
			}

			auto MeshMapIt = Scratch.ControlPointToVertices.find(Mesh);
			if (MeshMapIt == Scratch.ControlPointToVertices.end())
			{
				continue;
			}

			int DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

			for (int DeformerIndex = 0; DeformerIndex < DeformerCount; ++DeformerIndex)
			{
				FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(DeformerIndex, FbxDeformer::eSkin));
				if (!Skin)
				{
					continue;
				}

				int ClusterCount = Skin->GetClusterCount();

				for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
				{
					FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
					if (!Cluster)
					{
						continue;
					}

					FbxNode* BoneNode = Cluster->GetLink();
					if (!BoneNode)
					{
						continue;
					}

					// FBoneInfo 정보 저장(임시값: 아래 Rebuild 함수에서 Hierarchy 반영해서 재계산)
					// Cluster는 "해당 본이 어떤 Control Point에 얼마만큼 영향 주는지"를 제공한다.
					// 본 이름 기반으로 엔진 Bone 배열 인덱스를 정규화한다.
					int32 BoneIndex = FindOrAddBone(OutMesh, BoneNode, Cluster, Scratch.BindGlobalMatrices);

					// FSkinWeight 정보 저장
					// Control Point 영향치를 코너 기반 엔진 정점으로 전파한다.
					// (하나의 Control Point가 여러 엔진 정점으로 분화될 수 있음)
					int CPCount = Cluster->GetControlPointIndicesCount();
					int* CPIndices = Cluster->GetControlPointIndices();
					double* CPWeights = Cluster->GetControlPointWeights();
					if (!CPIndices || !CPWeights)
					{
						continue;
					}

					for (int i = 0; i < CPCount; ++i)
					{
						int32 ControlPointIndex = CPIndices[i];
						float Weight = static_cast<float>(CPWeights[i]);
						if (Weight <= 0.0f)
						{
							continue;
						}

						// CP -> 엔진 정점 매핑을 통해 실제 렌더 정점 SkinWeight를 누적한다.
						auto VertexListIt = MeshMapIt->second.find(ControlPointIndex);
						if (VertexListIt != MeshMapIt->second.end())
						{
							for (uint32 VertexIndex : VertexListIt->second)
							{
								if (VertexIndex < OutMesh.SkinWeights.size())
								{
									AddBoneInfluence(OutMesh.SkinWeights[VertexIndex], BoneIndex, Weight);
								}
							}
						}
					}
				}
			}
		}

		// Skin이 없는 Rigid Mesh는 부모 본에 100% 영향되도록 처리한다.
		BindRigidMeshesToParentBones(Scratch, OutMesh);
		// BindGlobalMatrices를 이용해 LocalBindTransform과 InverseBindPose 재구성
		RebuildLocalBindTransforms(OutMesh, Scratch.BindGlobalMatrices);
		// Weight 합이 1이 되도록 정규화
		NormalizeSkinWeights(OutMesh);
	}

	void BuildSections(FSkeletalMesh& OutMesh, const TArray<FStaticMaterial>& OutMaterials, const TArray<TArray<uint32>>& FacesPerMaterial)
	{
		TArray<uint32> OldIndices = std::move(OutMesh.Indices);
		OutMesh.Indices.clear();
		OutMesh.Sections.clear();

		for (size_t MaterialIndex = 0; MaterialIndex < FacesPerMaterial.size() && MaterialIndex < OutMaterials.size(); ++MaterialIndex)
		{
			const TArray<uint32>& FaceStarts = FacesPerMaterial[MaterialIndex];
			if (FaceStarts.empty())
			{
				continue;
			}

			FSkeletalMeshSection Section;
			Section.MaterialIndex = static_cast<int32>(MaterialIndex);
			Section.IndexStart = static_cast<uint32>(OutMesh.Indices.size());
			Section.IndexCount = static_cast<uint32>(FaceStarts.size() * 3);
			Section.VertexStart = 0;
			Section.VertexCount = static_cast<uint32>(OutMesh.Vertices.size());

			for (uint32 FaceStart : FaceStarts)
			{
				OutMesh.Indices.push_back(OldIndices[FaceStart + 0]);
				OutMesh.Indices.push_back(OldIndices[FaceStart + 1]);
				OutMesh.Indices.push_back(OldIndices[FaceStart + 2]);
			}

			OutMesh.Sections.push_back(Section);
		}
	}

	void AccumulateTangents(FSkeletalMesh& Mesh)
	{
		TArray<FVector> TangentSums(Mesh.Vertices.size(), FVector(0.0f, 0.0f, 0.0f));
		TArray<FVector> BitangentSums(Mesh.Vertices.size(), FVector(0.0f, 0.0f, 0.0f));

		for (size_t i = 0; i + 2 < Mesh.Indices.size(); i += 3)
		{
			const uint32 I0 = Mesh.Indices[i + 0];
			const uint32 I1 = Mesh.Indices[i + 1];
			const uint32 I2 = Mesh.Indices[i + 2];

			const FNormalVertex& V0 = Mesh.Vertices[I0];
			const FNormalVertex& V1 = Mesh.Vertices[I1];
			const FNormalVertex& V2 = Mesh.Vertices[I2];

			const FVector Edge1 = V1.pos - V0.pos;
			const FVector Edge2 = V2.pos - V0.pos;
			const FVector2 DeltaUV1 = V1.tex - V0.tex;
			const FVector2 DeltaUV2 = V2.tex - V0.tex;

			const float Det = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
			if (std::abs(Det) < 1e-8f)
			{
				continue;
			}

			const float InvDet = 1.0f / Det;
			const FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDet;
			const FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * InvDet;

			TangentSums[I0] += Tangent;
			TangentSums[I1] += Tangent;
			TangentSums[I2] += Tangent;
			BitangentSums[I0] += Bitangent;
			BitangentSums[I1] += Bitangent;
			BitangentSums[I2] += Bitangent;
		}

		for (size_t i = 0; i < Mesh.Vertices.size(); ++i)
		{
			FNormalVertex& Vertex = Mesh.Vertices[i];
			FVector Normal = Vertex.normal.Normalized();
			FVector Tangent = TangentSums[i];
			Tangent = (Tangent - Normal * Normal.Dot(Tangent)).Normalized();
			if (Tangent.IsNearlyZero())
			{
				Tangent = FVector(1.0f, 0.0f, 0.0f);
			}

			const float Handedness = Normal.Cross(Tangent).Dot(BitangentSums[i]) < 0.0f ? -1.0f : 1.0f;
			Vertex.tangent = FVector4(Tangent, Handedness);
		}
	}

	bool ConvertSkeletalMesh(const FString& FbxFilePath, FFbxInfo& Context, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
	{
		OutMesh = FSkeletalMesh();
		OutMaterials.clear();

		// 1. Vertex, Index Data
		FSkeletalImportScratch Scratch;
		TraverseNodes(FbxFilePath, Context.Scene ? Context.Scene->GetRootNode() : nullptr, Context, OutMesh, Scratch);

		// 2. Skin Weights, Bone Data
		FillSkinWeights(Context, Scratch, OutMesh);
		// BoneChildren / RootBoneIndices 캐시 구성
		OutMesh.BuildBoneHierarchyCache();

		// 3. Materials & Sections
		if (Context.Materials.empty() && !OutMesh.Indices.empty())
		{
			FFbxMaterialInfo DefaultMaterial;
			Context.Materials.push_back(DefaultMaterial);
			Context.FacesPerMaterial.resize(1);
			for (size_t FaceStart = 0; FaceStart + 2 < OutMesh.Indices.size(); FaceStart += 3)
			{
				Context.FacesPerMaterial[0].push_back(static_cast<uint32>(FaceStart));
			}
		}

		for (const FFbxMaterialInfo& MaterialInfo : Context.Materials)
		{
			FStaticMaterial StaticMaterial;
			StaticMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(FFbxCommon::ConvertMaterialInfoToMaterialAsset(FbxFilePath, MaterialInfo));
			StaticMaterial.MaterialSlotName = MaterialInfo.MaterialSlotName;
			OutMaterials.push_back(StaticMaterial);
		}

		BuildSections(OutMesh, OutMaterials, Context.FacesPerMaterial);
		OutMesh.PathFileName = FbxFilePath;
		//OutMesh.CacheBounds();
		AccumulateTangents(OutMesh);

		return !OutMesh.Vertices.empty() && !OutMesh.Indices.empty();
	}
}
bool FFbxSkeletalMeshImporter::Import(const FString& FbxFilePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	FFbxInfo Context;
	if (!FFbxCommon::ParseFbx(FbxFilePath, Context))
	{
		return false;
	}

	const bool bImported = ConvertSkeletalMesh(FbxFilePath, Context, OutMesh, OutMaterials);
	FFbxCommon::Destroy(Context);
	return bImported;
}
#endif
