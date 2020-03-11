
//==============================================================================================//

#include <cuda_runtime.h> 
#include "../Utils/cudaUtil.h"
#include "../Utils/cuda_SimpleMatrixUtil.h"
#include "../Utils/RendererUtil.h"
#include "CUDABasedRasterizationGradInput.h"
#include "../Utils/CameraUtil.h"
#include "../Utils/IndexHelper.h"

//==============================================================================================//

// Cg - vertex color grad buffer
// Co - vertex color buffer 
// Al - albedo color buffer 
// Li - light component buffer
// Vc - vertex color (vertex space)
// Bc - bary centric co-ordinates
// No - normal buffer
// Nv - vertex normal 
// Nf - face normal 
// Vp - vertex position 
// Gm - sh coefficients

//==============================================================================================//

/*
Initialize gradients for lighting 
*/
__global__ void initBuffersGradDevice2(CUDABasedRasterizationGradInput input)
{
	const unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < input.numberOfCameras * 27)
	{
		input.d_shCoeffGrad[idx] = 0.f;
	}
}

//==============================================================================================//

/*
Initialize gradients for texture
*/
__global__ void initBuffersGradDevice1(CUDABasedRasterizationGradInput input)
{
	const unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < input.texHeight * input.texWidth)
	{
		input.d_textureGrad[idx] = make_float3(0.f,0.f,0.f);
	}
}

//==============================================================================================//

/*
Initialize gradients for mesh pos and color
*/
__global__ void initBuffersGradDevice0(CUDABasedRasterizationGradInput input)
{
	const unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < input.N)
	{
		input.d_vertexPosGrad[idx]	 = make_float3(0.f, 0.f, 0.f);
		input.d_vertexColorGrad[idx] = make_float3(0.f, 0.f, 0.f);
	}
}

//==============================================================================================//

/*
Get gradients for vertex color buffer
*/
__global__ void renderBuffersGradDevice(CUDABasedRasterizationGradInput input)
{
	const unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < input.numberOfCameras * input.w * input.h)
	{
		////////////////////////////////////////////////////////////////////////
		//INDEXING
		////////////////////////////////////////////////////////////////////////

		int3 index = index1DTo3D(input.numberOfCameras, input.h, input.w, idx);
		int idc = index.x;
		int idh = index.y;
		int idw = index.z;
		int4 idf = input.d_faceIDBuffer[idx];

		if (idf.w == -1)
			return;

		////////////////////////////////////////////////////////////////////////
		//INIT
		////////////////////////////////////////////////////////////////////////

		float3 bcc				= input.d_barycentricCoordinatesBuffer[idx];
		int3   faceVerticesIds  = input.d_facesVertex[idf.x];
		const float* shCoeff	= input.d_shCoeff + idc * 27;

		float3 vertexPos0 = input.d_vertices[faceVerticesIds.x];
		float3 vertexPos1 = input.d_vertices[faceVerticesIds.y];
		float3 vertexPos2 = input.d_vertices[faceVerticesIds.z];
		float3 vertexCol0 = input.d_vertexColor[faceVerticesIds.x];
		float3 vertexCol1 = input.d_vertexColor[faceVerticesIds.y];
		float3 vertexCol2 = input.d_vertexColor[faceVerticesIds.z];
		float3 vertexNor0 = input.d_vertexNormal[idc*input.N + faceVerticesIds.x];
		float3 vertexNor1 = input.d_vertexNormal[idc*input.N + faceVerticesIds.y];
		float3 vertexNor2 = input.d_vertexNormal[idc*input.N + faceVerticesIds.z];

		float3 pixNormUn	= bcc.x * vertexNor0 + bcc.y * vertexNor1 + bcc.z * vertexNor2;
		float  pixNormVal	= sqrtf(pixNormUn.x*pixNormUn.x + pixNormUn.y*pixNormUn.y + pixNormUn.z*pixNormUn.z);
		float3 pixNorm = pixNormUn / pixNormVal;

		////////////////////////////////////////////////////////////////////////
		//VERTEX COLOR AND TEXTURE GRAD
		////////////////////////////////////////////////////////////////////////

		float3 pixLight = getIllum(pixNorm, shCoeff);
		mat3x3 JCoAl;
		getJCoAl(JCoAl, pixLight);

		mat3x1 GVCBVertexColor;
		GVCBVertexColor(0, 0) = input.d_vertexColorBufferGrad[idx].x;
		GVCBVertexColor(1, 0) = input.d_vertexColorBufferGrad[idx].y;
		GVCBVertexColor(2, 0) = input.d_vertexColorBufferGrad[idx].z;

		if (input.renderMode == RenderMode::VertexColor)
		{
			mat9x3 JAlVc;
			getJAlVc(JAlVc, bcc);

			mat9x1 gradVerCol = JAlVc * JCoAl * GVCBVertexColor;

			addGradients9I(gradVerCol, input.d_vertexColorGrad, faceVerticesIds);
		}

		if (input.renderMode == RenderMode::Textured)
		{
			mat3x1 gradTexColor = JCoAl * GVCBVertexColor;

			float2 texCoord0 = make_float2(input.d_textureCoordinates[idf.x * 3 * 2 + 0 * 2 + 0], input.d_textureCoordinates[idf.x * 3 * 2 + 0 * 2 + 1]);
			float2 texCoord1 = make_float2(input.d_textureCoordinates[idf.x * 3 * 2 + 1 * 2 + 0], input.d_textureCoordinates[idf.x * 3 * 2 + 1 * 2 + 1]);
			float2 texCoord2 = make_float2(input.d_textureCoordinates[idf.x * 3 * 2 + 2 * 2 + 0], input.d_textureCoordinates[idf.x * 3 * 2 + 2 * 2 + 1]);
			float2 finalTexCoord = texCoord0* bcc.x + texCoord1* bcc.y + texCoord2* bcc.z;
			finalTexCoord.x = finalTexCoord.x * input.texWidth;
			finalTexCoord.y = (1.f - finalTexCoord.y) * input.texHeight;
			finalTexCoord.x = fmaxf(finalTexCoord.x, 0);
			finalTexCoord.x = fminf(finalTexCoord.x, input.texWidth - 1);
			finalTexCoord.y = fmaxf(finalTexCoord.y, 0);
			finalTexCoord.y = fminf(finalTexCoord.y, input.texHeight - 1);

			atomicAdd(&input.d_textureGrad[index2DTo1D(input.texHeight, input.texWidth, finalTexCoord.y, finalTexCoord.x)].x, gradTexColor(0, 0));
			atomicAdd(&input.d_textureGrad[index2DTo1D(input.texHeight, input.texWidth, finalTexCoord.y, finalTexCoord.x)].y, gradTexColor(1, 0));
			atomicAdd(&input.d_textureGrad[index2DTo1D(input.texHeight, input.texWidth, finalTexCoord.y, finalTexCoord.x)].z, gradTexColor(2, 0));
		}

		////////////////////////////////////////////////////////////////////////
		//LIGHTING GRAD
		////////////////////////////////////////////////////////////////////////

		mat1x3 GVCBLight;
		GVCBLight(0, 0) = input.d_vertexColorBufferGrad[idx].x;
		GVCBLight(0, 1) = input.d_vertexColorBufferGrad[idx].y;
		GVCBLight(0, 2) = input.d_vertexColorBufferGrad[idx].z;

		// jacobians
		mat3x3 JCoLi;
		float3 pixAlb = bcc.x * vertexCol0 + bcc.y * vertexCol1 + bcc.z * vertexCol2;
		getJCoLi(JCoLi, pixAlb);

		mat3x9 JLiGmR;
		getJLiGm(JLiGmR, 0, pixNorm);
		mat3x9 JLiGmG;
		getJLiGm(JLiGmG, 1, pixNorm);
		mat3x9 JLiGmB;
		getJLiGm(JLiGmB, 2, pixNorm);

		mat1x9 gradSHCoeffR = GVCBLight * JCoLi * JLiGmR;
		mat1x9 gradSHCoeffG = GVCBLight * JCoLi * JLiGmG;
		mat1x9 gradSHCoeffB = GVCBLight * JCoLi * JLiGmB;

		addGradients9(gradSHCoeffR, &input.d_shCoeffGrad[idc * 27]);
		addGradients9(gradSHCoeffG, &input.d_shCoeffGrad[idc * 27+9]);
		addGradients9(gradSHCoeffB, &input.d_shCoeffGrad[idc * 27+18]);

		////////////////////////////////////////////////////////////////////////
		//VERTEX POS GRAD
		////////////////////////////////////////////////////////////////////////

		mat1x3 GVCBPosition;
		GVCBPosition(0, 0) = input.d_vertexColorBufferGrad[idx].x;
		GVCBPosition(0, 1) = input.d_vertexColorBufferGrad[idx].y;
		GVCBPosition(0, 2) = input.d_vertexColorBufferGrad[idx].z;

		mat3x3 JNoNu;
		getJNoNu(JNoNu, pixNormUn, pixNormVal);

		mat3x3 JLiNo;
		getJLiNo(JLiNo, pixNorm, shCoeff);

		mat3x3 TR = getRotationMatrix(&input.d_cameraExtrinsics[3 * idc]);
	

		/////////////////////

		mat3x3 JAlBc;
		getJAlBc(JAlBc, vertexCol0, vertexCol1, vertexCol2);

		mat3x3 JNoBc;
		getJNoBc(JNoBc, vertexNor0, vertexNor1, vertexNor2);
		
		mat3x9 JBcVp;
		getJBcVp(JBcVp, vertexPos0, vertexPos1, vertexPos2, bcc);

		mat1x9 gradVerPos = GVCBPosition * JCoAl * JAlBc * JBcVp;// +GVCBPosition * JCoLi * JLiNo * JNoNu * JNoBc * JBcVp;
		//TTOOOOOOOOOOOOOODOOOOOOOOOOOOOOOOO REMNOVE THAT AGAIN
		addGradients9I(gradVerPos.getTranspose(), input.d_vertexPosGrad, faceVerticesIds);

		////////////////////
		for (int i = 0; i < 3; i++)
		{
			mat3x3 JNuNvx;
			JNuNvx.setIdentity();
			int idv = -1;

			//
			if (i == 0) 
			{ 
				idv = faceVerticesIds.x; 
				JNuNvx = bcc.x * JNuNvx; 
			}
			else if (i == 1) 
			{ 
				idv = faceVerticesIds.y; 
				JNuNvx = bcc.y * JNuNvx; 
			}
			else 
			{ 
				idv = faceVerticesIds.z; 
				JNuNvx = bcc.z * JNuNvx; 
			}

			//
			int2 verFaceId = input.d_vertexFacesId[idv];

			//
			for (int j = verFaceId.x; j < verFaceId.x + verFaceId.y; j++)
			{
				int faceId = input.d_vertexFaces[j];
			
				int3 v_index_inner = input.d_facesVertex[faceId];
				mat3x1 vi = TR * (mat3x1)input.d_vertices[v_index_inner.x];
				mat3x1 vj = TR * (mat3x1)input.d_vertices[v_index_inner.y];
				mat3x1 vk = TR * (mat3x1)input.d_vertices[v_index_inner.z];

				mat3x3 J;
				
				// gradients vi
				//getJ_vi(J, TR, vk, vj, vi);
				//mat1x3 gradVi = GVCBPosition * JCoLi * JLiNo * JNoNu * JNuNvx * J;
				//if(v_index_inner.x == 0)
				//	addGradients(gradVi, &input.d_vertexPosGrad[v_index_inner.x]);

				//// gradients vj
				//getJ_vj(J, TR, vj, vi);
				//mat1x3 gradVj = GVCBPosition * JCoLi * JLiNo * JNoNu * JNuNvx * J;
				//if (v_index_inner.y == 0)
				//	addGradients(gradVj, &input.d_vertexPosGrad[v_index_inner.y]);

				//// gradients vk
				//getJ_vk(J, TR, vk, vi);
				//mat1x3 gradVk = GVCBPosition * JCoLi * JLiNo * JNoNu * JNuNvx * J;
				//if (v_index_inner.z == 0)
				//	addGradients(gradVk, &input.d_vertexPosGrad[v_index_inner.z]);	
			}
		}
	}
}

//==============================================================================================//

extern "C" void renderBuffersGradGPU(CUDABasedRasterizationGradInput& input)
{
	initBuffersGradDevice2    << < (input.numberOfCameras * 27 + THREADS_PER_BLOCK_CUDABASEDRASTERIZER - 1) / THREADS_PER_BLOCK_CUDABASEDRASTERIZER, THREADS_PER_BLOCK_CUDABASEDRASTERIZER >> >				(input);

	initBuffersGradDevice1    << < (input.texHeight * input.texWidth + THREADS_PER_BLOCK_CUDABASEDRASTERIZER - 1) / THREADS_PER_BLOCK_CUDABASEDRASTERIZER, THREADS_PER_BLOCK_CUDABASEDRASTERIZER >> >		(input);

	initBuffersGradDevice0    << < (input.N + THREADS_PER_BLOCK_CUDABASEDRASTERIZER - 1) / THREADS_PER_BLOCK_CUDABASEDRASTERIZER, THREADS_PER_BLOCK_CUDABASEDRASTERIZER >> >								(input);

	renderBuffersGradDevice   << < (input.numberOfCameras*input.w*input.h + THREADS_PER_BLOCK_CUDABASEDRASTERIZER - 1) / THREADS_PER_BLOCK_CUDABASEDRASTERIZER, THREADS_PER_BLOCK_CUDABASEDRASTERIZER >> >	(input);
}

//==============================================================================================//
