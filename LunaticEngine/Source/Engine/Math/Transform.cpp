#include "PCH/LunaticPCH.h"
#include "Transform.h"

FMatrix FTransform::ToMatrix() const
{
	FMatrix translateMatrix = FMatrix::MakeTranslationMatrix(Location);

	FMatrix rotationMatrix = Rotation.ToMatrix();

	FMatrix scaleMatrix = FMatrix::MakeScaleMatrix(Scale);

	return scaleMatrix * rotationMatrix * translateMatrix;
}


FTransform FTransform::FromMatrix(const FMatrix& Matrix)
{
	FVector RawAxisX(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
	FVector RawAxisY(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
	FVector RawAxisZ(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

	FVector Scale(RawAxisX.Length(), RawAxisY.Length(), RawAxisZ.Length());
	FVector AxisX = (Scale.X > 1.0e-6f) ? (RawAxisX / Scale.X) : FVector(1.0f, 0.0f, 0.0f);
	FVector AxisY = (Scale.Y > 1.0e-6f) ? (RawAxisY / Scale.Y) : FVector(0.0f, 1.0f, 0.0f);
	FVector AxisZ = (Scale.Z > 1.0e-6f) ? (RawAxisZ / Scale.Z) : FVector(0.0f, 0.0f, 1.0f);

	FMatrix BasisMatrix = FMatrix::Identity;
	BasisMatrix.M[0][0] = AxisX.X; BasisMatrix.M[0][1] = AxisX.Y; BasisMatrix.M[0][2] = AxisX.Z;
	BasisMatrix.M[1][0] = AxisY.X; BasisMatrix.M[1][1] = AxisY.Y; BasisMatrix.M[1][2] = AxisY.Z;
	BasisMatrix.M[2][0] = AxisZ.X; BasisMatrix.M[2][1] = AxisZ.Y; BasisMatrix.M[2][2] = AxisZ.Z;

	// Matrix -> Quat 변환은 reflection이 섞인 basis에서 한 축이 뒤집힐 수 있다.
	// determinant가 음수면 reflection은 signed scale에 보존하고 rotation basis는 right-handed로 보정한다.
	if (BasisMatrix.GetBasisDeterminant3x3() < 0.0f)
	{
		Scale.X = -Scale.X;
		AxisX *= -1.0f;
	}

	// Scale World를 행렬로 적용하면 rotated target에는 shear가 생길 수 있다.
	// 현재 FTransform은 shear를 저장하지 못하므로, rotation 추출 전 basis를 Gram-Schmidt로
	// orthonormalize해서 Quat 변환이 비직교 basis 때문에 튀지 않도록 한다.
	AxisX = AxisX.Normalized();
	AxisY = (AxisY - AxisX * AxisY.Dot(AxisX)).Normalized();
	if (AxisY.IsNearlyZero())
	{
		AxisY = FVector(0.0f, 1.0f, 0.0f);
	}
	AxisZ = AxisX.Cross(AxisY).Normalized();
	if (AxisZ.IsNearlyZero())
	{
		AxisZ = FVector(0.0f, 0.0f, 1.0f);
	}

	FMatrix RotationMatrix = FMatrix::Identity;
	RotationMatrix.M[0][0] = AxisX.X; RotationMatrix.M[0][1] = AxisX.Y; RotationMatrix.M[0][2] = AxisX.Z;
	RotationMatrix.M[1][0] = AxisY.X; RotationMatrix.M[1][1] = AxisY.Y; RotationMatrix.M[1][2] = AxisY.Z;
	RotationMatrix.M[2][0] = AxisZ.X; RotationMatrix.M[2][1] = AxisZ.Y; RotationMatrix.M[2][2] = AxisZ.Z;

	return FTransform(Matrix.GetLocation(), RotationMatrix.ToQuat(), Scale);
}
