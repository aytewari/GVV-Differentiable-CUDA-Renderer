//==============================================================================================//
// Classname:
//      GradUtil 
//
//==============================================================================================//
// Description:
//      Basic operations for gradients for rendering 
//
//==============================================================================================//

#pragma once 

//==============================================================================================//

#include <cutil_inline.h>
#include <cutil_math.h>

//==============================================================================================//

__inline__ __device__ void getJCoAl(mat3x3 &JCoAl, float3 pixLight)
{
	JCoAl.setZero();
	JCoAl(0, 0) = pixLight.x;
	JCoAl(1, 1) = pixLight.y;
	JCoAl(2, 2) = pixLight.z;
}

__inline__ __device__ void getJAlVc(mat9x3 &JAlVc, float3 bcc)
{
	JAlVc.setZero();
	JAlVc(0, 0) = bcc.x;
	JAlVc(1, 1) = bcc.x;
	JAlVc(2, 2) = bcc.x;

	JAlVc(3, 0) = bcc.y;
	JAlVc(4, 1) = bcc.y;
	JAlVc(5, 2) = bcc.y;

	JAlVc(6, 0) = bcc.z;
	JAlVc(7, 1) = bcc.z;
	JAlVc(8, 2) = bcc.z;
}

__inline__ __device__ void getJCoLi(mat3x3 &JCoLi, float3 pixAlb)
{
	JCoLi.setZero();
	JCoLi(0, 0) = pixAlb.x;
	JCoLi(1, 1) = pixAlb.y;
	JCoLi(2, 2) = pixAlb.z;
}

__inline__ __device__ float3 getIllum(float3 dir, const float *shCoeffs)
{
	float3 dirSq = dir * dir;
	float3 light;

	light.x = shCoeffs[0];
	light.x += shCoeffs[1] * dir.y;
	light.x += shCoeffs[2] * dir.z;
	light.x += shCoeffs[3] * dir.x;
	light.x += shCoeffs[4] * (dir.x * dir.y);
	light.x += shCoeffs[5] * (dir.z * dir.y);
	light.x += shCoeffs[6] * (3 * dirSq.z - 1);
	light.x += shCoeffs[7] * (dir.x * dir.z);
	light.x += shCoeffs[8] * (dirSq.x - dirSq.y);

	light.y = shCoeffs[9 + 0];
	light.y += shCoeffs[9 + 1] * dir.y;
	light.y += shCoeffs[9 + 2] * dir.z;
	light.y += shCoeffs[9 + 3] * dir.x;
	light.y += shCoeffs[9 + 4] * (dir.x * dir.y);
	light.y += shCoeffs[9 + 5] * (dir.z * dir.y);
	light.y += shCoeffs[9 + 6] * (3 * dirSq.z - 1);
	light.y += shCoeffs[9 + 7] * (dir.x * dir.z);
	light.y += shCoeffs[9 + 8] * (dirSq.x - dirSq.y);

	light.z = shCoeffs[18 + 0];
	light.z += shCoeffs[18 + 1] * dir.y;
	light.z += shCoeffs[18 + 2] * dir.z;
	light.z += shCoeffs[18 + 3] * dir.x;
	light.z += shCoeffs[18 + 4] * (dir.x * dir.y);
	light.z += shCoeffs[18 + 5] * (dir.z * dir.y);
	light.z += shCoeffs[18 + 6] * (3 * dirSq.z - 1);
	light.z += shCoeffs[18 + 7] * (dir.x * dir.z);
	light.z += shCoeffs[18 + 8] * (dirSq.x - dirSq.y);
	return light;
}

__inline__ __device__ void getJLiGm(mat3x9 &JLiGm, int rgb, float3 pixNorm)
{
	JLiGm.setZero();

	JLiGm(rgb, 0) = 1;
	JLiGm(rgb, 1) = pixNorm.y;
	JLiGm(rgb, 2) = pixNorm.z;
	JLiGm(rgb, 3) = pixNorm.x;
	JLiGm(rgb, 4) = pixNorm.x * pixNorm.y;
	JLiGm(rgb, 5) = pixNorm.z * pixNorm.y;
	JLiGm(rgb, 6) = 3 * pixNorm.z*pixNorm.z - 1;
	JLiGm(rgb, 7) = pixNorm.x * pixNorm.z;
	JLiGm(rgb, 8) = ((pixNorm.x * pixNorm.x) - (pixNorm.y*pixNorm.y));
}

__inline__ __device__ void getJLiNo(mat3x3 &JLiNo, float3 dir, const float* shCoeff)
{
	JLiNo.setZero();
	for (int i = 0; i < 3; i++) {
		JLiNo(i, 0) =    shCoeff[(i * 9) + 3] +
						(shCoeff[(i * 9) + 4] * dir.y) +
						(shCoeff[(i * 9) + 7] * dir.z) +
						(shCoeff[(i * 9) + 8] * 2 * dir.x);

		JLiNo(i, 1) =    shCoeff[(i * 9) + 1] +
						(shCoeff[(i * 9) + 4] * dir.x) +
						(shCoeff[(i * 9) + 5] * dir.z) +
						(shCoeff[(i * 9) + 8] * -2.f * dir.y);

		JLiNo(i, 2) =    shCoeff[(i * 9) + 2] +
						(shCoeff[(i * 9) + 5] * dir.y) +
						(shCoeff[(i * 9) + 6] * 6 * dir.z) +
						(shCoeff[(i * 9) + 7] * dir.x);
	}
}

__inline__ __device__ void getJNoNu(mat3x3 &JNoNu, float3 un_vec, float norm)
{
	float norm_p2 = norm * norm;
	float norm_p3 = norm_p2 * norm;

	JNoNu(0, 0) = (norm_p2 - (un_vec.x*un_vec.x)) / (norm_p3);
	JNoNu(1, 1) = (norm_p2 - (un_vec.y*un_vec.y)) / (norm_p3);
	JNoNu(2, 2) = (norm_p2 - (un_vec.z*un_vec.z)) / (norm_p3);

	JNoNu(0, 1) = -(un_vec.x*un_vec.y) / norm_p3;
	JNoNu(1, 0) = JNoNu(0, 1);

	JNoNu(0, 2) = -(un_vec.x*un_vec.z) / norm_p3;
	JNoNu(2, 0) = JNoNu(0, 2);

	JNoNu(1, 2) = -(un_vec.y*un_vec.z) / norm_p3;
	JNoNu(2, 1) = JNoNu(1, 2);
}
__inline__ __device__ void getJ_vk(mat3x3 &J, mat3x3 TR, mat3x1 vk, mat3x1 vi)
{
	float3 temp3;

	mat3x1 Ix(make_float3(1.f, 0.f, 0.f));
	mat3x1 Iy(make_float3(0.f, 1.f, 0.f));
	mat3x1 Iz(make_float3(0.f, 0.f, 1.f));
	
	//J2
	mat3x3 J2;
	mat3x1 diff2 = vk - vi;

	temp3 = cross(Ix, diff2);
	J2(0, 0) = -temp3.x;
	J2(1, 0) = -temp3.y;
	J2(2, 0) = -temp3.z;

	temp3 = cross(Iy, diff2);
	J2(0, 1) = -temp3.x;
	J2(1, 1) = -temp3.y;
	J2(2, 1) = -temp3.z;

	temp3 = cross(Iz, diff2);
	J2(0, 2) = -temp3.x;
	J2(1, 2) = -temp3.y;
	J2(2, 2) = -temp3.z;

	J = J2 * TR;
}

__inline__ __device__ void getJ_vj(mat3x3 &J, mat3x3 TR, mat3x1 vj, mat3x1 vi)
{
	float3 temp3;

	mat3x1 Ix(make_float3(1.f, 0.f, 0.f));
	mat3x1 Iy(make_float3(0.f, 1.f, 0.f));
	mat3x1 Iz(make_float3(0.f, 0.f, 1.f));

	//J1
	mat3x3 J1;
	mat3x1 diff1 = vj - vi;

	temp3 = cross(Ix, diff1);
	J1(0, 0) = temp3.x;
	J1(1, 0) = temp3.y;
	J1(2, 0) = temp3.z;

	temp3 = cross(Iy, diff1);
	J1(0, 1) = temp3.x;
	J1(1, 1) = temp3.y;
	J1(2, 1) = temp3.z;

	temp3 = cross(Iz, diff1);
	J1(0, 2) = temp3.x;
	J1(1, 2) = temp3.y;
	J1(2, 2) = temp3.z;

	J = J1  * TR;
}

__inline__ __device__ void getJ_vi(mat3x3 &J, mat3x3 TR,mat3x1 vk, mat3x1 vj, mat3x1 vi)
{
	float3 temp3;
	mat3x3 identity;
	identity.setIdentity();
	mat3x3 negIdentity = identity * -1.f;

	mat3x1 Ix(make_float3(1.f, 0.f, 0.f));
	mat3x1 Iy(make_float3(0.f, 1.f, 0.f));
	mat3x1 Iz(make_float3(0.f, 0.f, 1.f));

	//J1
	mat3x3 J1;
	mat3x1 diff1 = vj - vi;

	temp3 = cross(Ix, diff1);
	J1(0, 0) = temp3.x;		
	J1(1, 0) = temp3.y;
	J1(2, 0) = temp3.z;

	temp3 = cross(Iy, diff1);
	J1(0, 1) = temp3.x;
	J1(1, 1) = temp3.y;
	J1(2, 1) = temp3.z;

	temp3 = cross(Iz, diff1);
	J1(0, 2) = temp3.x;
	J1(1, 2) = temp3.y;
	J1(2, 2) = temp3.z;

	//J2
	mat3x3 J2;
	mat3x1 diff2 = vk - vi;

	temp3 = cross(Ix, diff2);
	J2(0, 0) = -temp3.x;
	J2(1, 0) = -temp3.y;
	J2(2, 0) = -temp3.z;

	temp3 = cross(Iy, diff2);
	J2(0, 1) = -temp3.x;
	J2(1, 1) = -temp3.y;
	J2(2, 1) = -temp3.z;

	temp3 = cross(Iz, diff2);
	J2(0, 2) = -temp3.x;
	J2(1, 2) = -temp3.y;
	J2(2, 2) = -temp3.z;

	J = J1 * negIdentity * TR + J2 *negIdentity * TR;
}

__inline__ __device__ void addGradients(mat1x3 grad, float3* d_grad)
{
	float* d_gradFloat0 = (float*)d_grad;
	float* d_gradFloat1 = (float*)d_grad + 1;
	float* d_gradFloat2 = (float*)d_grad + 2;
	atomicAdd(d_gradFloat0, grad(0, 0));
	atomicAdd(d_gradFloat1, grad(0, 1));
	atomicAdd(d_gradFloat2, grad(0, 2));
}

__inline__ __device__ void addGradients9(mat1x9 grad, float* d_grad)
{
	for (int ii = 0; ii < 9; ii++)
		atomicAdd(&d_grad[ii], grad(0, ii));
}

__inline__ __device__ void addGradients9I(mat9x1 grad, float3* d_grad, int3 index)
{
	if (index.x == 0)
	{
		atomicAdd(&d_grad[index.x].x, grad(0, 0));
		atomicAdd(&d_grad[index.x].y, grad(1, 0));
		atomicAdd(&d_grad[index.x].z, grad(2, 0));
	}
	if (index.y == 0)
	{
		atomicAdd(&d_grad[index.y].x, grad(3, 0));
		atomicAdd(&d_grad[index.y].y, grad(4, 0));
		atomicAdd(&d_grad[index.y].z, grad(5, 0));
	}
	if (index.z == 0)
	{
		atomicAdd(&d_grad[index.z].x, grad(6, 0));
		atomicAdd(&d_grad[index.z].y, grad(7, 0));
		atomicAdd(&d_grad[index.z].z, grad(8, 0));
	}
}

__device__ inline mat3x3 getRotationMatrix(float4* d_T)
{
	mat3x3 TE;
	TE(0, 0) = d_T[0].x;
	TE(0, 1) = d_T[0].y;
	TE(0, 2) = d_T[0].z;
	TE(1, 0) = d_T[1].x;
	TE(1, 1) = d_T[1].y;
	TE(1, 2) = d_T[1].z;
	TE(2, 0) = d_T[2].x;
	TE(2, 1) = d_T[2].y;
	TE(2, 2) = d_T[2].z;
	return TE;
}


__inline__ __device__ void getJAlBc(mat3x3 &JAlBc, float3 vertexCol0, float3 vertexCol1, float3 vertexCol2)
{
	JAlBc.setZero();

	JAlBc(0, 0) = vertexCol0.x;
	JAlBc(0, 1) = vertexCol1.x;
	JAlBc(0, 2) = vertexCol2.x;
	JAlBc(1, 0) = vertexCol0.y;
	JAlBc(1, 1) = vertexCol1.y;
	JAlBc(1, 2) = vertexCol2.y;
	JAlBc(2, 0) = vertexCol0.z;
	JAlBc(2, 1) = vertexCol1.z;
	JAlBc(2, 2) = vertexCol2.z;
}

__inline__ __device__ void getJNoBc(mat3x3 &JNoBc, float3 N0, float3 N1, float3 N2)
{
	JNoBc(0, 0) = N0.x;
	JNoBc(0, 1) = N1.x;
	JNoBc(0, 2) = N2.x;

	JNoBc(1, 0) = N0.y;
	JNoBc(1, 1) = N1.y;
	JNoBc(1, 2) = N2.y;


	JNoBc(2, 0) = N0.z;
	JNoBc(2, 1) = N1.z;
	JNoBc(2, 2) = N2.z;
}
__inline__ __device__ void getJBcVp(mat3x9 &JBcVp, float3 v0, float3 v1, float3 v2, float3 bcc)
{
	JBcVp.setZero();
	mat3x1 BCC = (mat3x1)bcc;
	mat3x1 Pixel = (mat3x1) (bcc.x * v0 + bcc.y * v1 + bcc.z * v2 );
	mat3x3 VP;
	VP(0, 0) = v0.x;
	VP(1, 0) = v0.y;
	VP(2, 0) = v0.z;
	VP(0, 1) = v1.x;
	VP(1, 1) = v1.y;
	VP(2, 1) = v1.z;
	VP(0, 2) = v2.x;
	VP(1, 2) = v2.y;
	VP(2, 2) = v2.z;
	float D = VP.det();
	mat3x3 Adj = VP.getInverse()*D;


	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			for (int k = 0; k < 3; k++)
			{
				for (int m = 0; m < 3; m++)
				{
					if (i != j && m != k)
						JBcVp(i, k * 3 + j) += (1 - 2 * ((i + m) % 2))*(1 - 2 * (k < 3 - k - m))*(1 - 2 * (j < 3 - i - j))*VP(3 - i - j, 3 - m - k)*Pixel(m, 0) / D;
				}
				JBcVp(i, k * 3 + j) += -BCC(i, 0)*Adj(j, k) / D;
			}
		}
	}
}