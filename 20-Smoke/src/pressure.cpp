#include "pressure.h"

#include <SIM/SIM_FieldUtils.h>

static int To1DIdx(const UT_Vector3I &idx, const UT_Vector3I &res) { return idx.x() + res.x() * (idx.y() + res.y() * idx.z()); }
static int To1DIdx(const UT_Vector2I &idx, const UT_Vector2I &res) { return idx.x() + res.x() * idx.y(); }
static UT_Vector3I To3DIdx(int idx, const UT_Vector3I &res)
{
	UT_Vector3I ret;
	ret.z() = idx / (res.x() * res.y());
	idx -= ret.z() * res.x() * res.y();
	ret.y() = idx / res.x();
	ret.x() = idx % res.x();
	return ret;
}
static UT_Vector2I To2DIdx(int idx, const UT_Vector2I &res)
{
	UT_Vector2I ret;
	ret.y() = idx / res.x();
	ret.x() = idx % res.x();
	return ret;
}

float HinaPE::PoissonSolver::PCG(UT_VoxelArrayF *PRESSURE, const UT_VoxelArrayF *DIVERGENCE, const UT_VoxelArrayI *MARKER)
{
	exint size = PRESSURE->numVoxels();
	UT_SparseMatrixF A(size, size);
	UT_VectorF x(0, size);
	UT_VectorF b(0, size);

	BuildLHS(&A, MARKER);
	BuildRHS(&b, DIVERGENCE);
	x = b;

	A.negate(); // make diagonal positive
	A.compile();

	UT_SparseMatrixRowF AImpl;
	AImpl.buildFrom(A);
	float error = AImpl.solveConjugateGradient(x, b, nullptr);
	WriteResult(PRESSURE, &x);
	return error;
}
float HinaPE::PoissonSolver::GaussSeidel(UT_VoxelArrayF *PRESSURE, const UT_VoxelArrayF *DIVERGENCE, const UT_VoxelArrayI *MARKER)
{
	// TODO: Implement Gauss-Seidel
	return 0;
}
float HinaPE::PoissonSolver::Jacobi(UT_VoxelArrayF *PRESSURE, const UT_VoxelArrayF *DIVERGENCE, const UT_VoxelArrayI *MARKER)
{
	// TODO: Implement Jacobi
	return 0;
}
void HinaPE::PoissonSolver::BuildLHSPartial(UT_SparseMatrixF *A, const UT_VoxelArrayI *MARKER, const UT_JobInfo &info)
{
	UT_VoxelArrayIteratorI vit;
	vit.setConstArray(MARKER);
	vit.setCompressOnExit(true);
	vit.setPartialRange(info.job(), info.numJobs());

	for (vit.rewind(); !vit.atEnd(); vit.advance())
	{
		UT_Vector3I cell(vit.x(), vit.y(), vit.z());
		int idx = To1DIdx(cell, MARKER->getVoxelRes());
		switch ((*MARKER)(cell))
		{
			case 1: // Fluid
			{
				A->addToElement(idx, idx, -1.0f);
				for (int axis: {0, 1, 2})
				{
					constexpr int dir0 = 0, dir1 = 1;
					UT_Vector3I cell0 = SIM::FieldUtils::cellToCellMap(cell, axis, dir0);
					UT_Vector3I cell1 = SIM::FieldUtils::cellToCellMap(cell, axis, dir1);
					int idx0 = To1DIdx(cell0, MARKER->getVoxelRes());
					int idx1 = To1DIdx(cell1, MARKER->getVoxelRes());
					if (cell0[axis] >= 0)
					{
						A->addToElement(idx, idx0, 1.0f);
						A->addToElement(idx, idx, -1.0f);
					}
					if (cell1[axis] < MARKER->getVoxelRes()[axis])
					{
						A->addToElement(idx, idx1, 1.0f);
						A->addToElement(idx, idx, -1.0f);
					}
				}
			}
				break;
			default:
				A->addToElement(idx, idx, -1.0f);
				break;
		}
	}
}
void HinaPE::PoissonSolver::BuildRHSPartial(UT_VectorF *b, const UT_VoxelArrayF *DIVERGENCE, const UT_JobInfo &info)
{
	UT_VoxelArrayIteratorF vit;
	vit.setConstArray(DIVERGENCE);
	vit.setCompressOnExit(true);
	vit.setPartialRange(info.job(), info.numJobs());

	const float h = DIVERGENCE->getXRes();

	for (vit.rewind(); !vit.atEnd(); vit.advance())
	{
		UT_Vector3I cell(vit.x(), vit.y(), vit.z());
		int idx = To1DIdx(cell, DIVERGENCE->getVoxelRes());
		(*b)(idx) = h * h * vit.getValue();
	}
}
void HinaPE::PoissonSolver::WriteResultPartial(UT_VoxelArrayF *PRESSURE, const UT_VectorF *x, const UT_JobInfo &info)
{
	UT_VoxelArrayIteratorF vit;
	vit.setArray(PRESSURE);
	vit.setCompressOnExit(true);
	vit.setPartialRange(info.job(), info.numJobs());

	for (vit.rewind(); !vit.atEnd(); vit.advance())
	{
		UT_Vector3I cell(vit.x(), vit.y(), vit.z());
		int idx = To1DIdx(cell, PRESSURE->getVoxelRes());
		vit.setValue((*x)(idx));
	}
}
