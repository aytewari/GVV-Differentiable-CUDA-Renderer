
#include "CudaRendererGrad.h"

//==============================================================================================//

REGISTER_OP("CudaRendererGradGpu")

.Input("render_buffer_grad: float")

.Input("vertex_pos: float")
.Input("vertex_color: float")
.Input("texture: float")
.Input("sh_coeff: float")
.Input("vertex_normal: float")
.Input("barycentric_buffer: float")
.Input("face_buffer: int32")

.Output("vertex_pos_grad: float")
.Output("vertex_color_grad: float")
.Output("texture_grad: float")
.Output("sh_coeff_grad: float")

.Attr("faces: list(int)")
.Attr("texture_coordinates: list(float)")
.Attr("number_of_vertices: int")
.Attr("extrinsics: list(float)")
.Attr("intrinsics: list(float)")
.Attr("render_resolution_u: int = 512")
.Attr("render_resolution_v: int = 512")
.Attr("render_mode: string");

//==============================================================================================//

CudaRendererGrad::CudaRendererGrad(OpKernelConstruction* context)
	: 
	OpKernel(context) 
{
	std::vector<int> faces;
	OP_REQUIRES_OK(context, context->GetAttr("faces", &faces));

	std::vector<float> textureCoordinates;
	OP_REQUIRES_OK(context, context->GetAttr("texture_coordinates", &textureCoordinates));

	OP_REQUIRES_OK(context, context->GetAttr("number_of_vertices", &numberOfPoints));
	OP_REQUIRES(context, numberOfPoints > 0, errors::InvalidArgument("number_of_vertices not set!", numberOfPoints));

	std::vector<float> extrinsics;
	OP_REQUIRES_OK(context, context->GetAttr("extrinsics", &extrinsics));
	if (extrinsics.size() % 12 == 0)
		numberOfCameras = extrinsics.size() / 12;
	else
		std::cout << "Camera extrinsics have wrong dimensionality!" << std::endl;

	std::vector<float> intrinsics;
	OP_REQUIRES_OK(context, context->GetAttr("intrinsics", &intrinsics));

	OP_REQUIRES_OK(context, context->GetAttr("render_resolution_u", &renderResolutionU));
	OP_REQUIRES(context, renderResolutionU > 0, errors::InvalidArgument("render_resolution_u not set!", renderResolutionU));

	OP_REQUIRES_OK(context, context->GetAttr("render_resolution_v", &renderResolutionV));
	OP_REQUIRES(context, renderResolutionV > 0, errors::InvalidArgument("render_resolution_v not set!", renderResolutionV));

	OP_REQUIRES_OK(context, context->GetAttr("render_mode", &renderMode));
	if (renderMode != "vertexColor" && renderMode != "textured")
	{
		std::cout << "INVALID RENDER MODE" << std::endl;
		return;
	}

	cudaBasedRasterizationGrad = new CUDABasedRasterizationGrad(faces, textureCoordinates, numberOfPoints, extrinsics, intrinsics, renderResolutionU, renderResolutionV, renderMode);
}

//==============================================================================================//

void CudaRendererGrad::setupInputOutputTensorPointers(OpKernelContext* context)
{
	//---INPUT---

	//[0]
	//Grab the vertec color buffer gradients 
	const Tensor& inputTensorVertexColorBufferGrad = context->input(0);
	Eigen::TensorMap<Eigen::Tensor< const float, 1, 1, Eigen::DenseIndex>, 16> inputTensorVertexColorBufferGradFlat = inputTensorVertexColorBufferGrad.flat_inner_dims<float, 1>();
	d_inputRenderBufferGrad = inputTensorVertexColorBufferGradFlat.data();

	//[1]
	//Grab the 3D vertex position
	const Tensor& inputTensorVertexPos = context->input(1);
	Eigen::TensorMap<Eigen::Tensor< const float, 1, 1, Eigen::DenseIndex>, 16> inputTensorVertexPosFlat = inputTensorVertexPos.flat_inner_dims<float, 1>();
	d_inputVertexPos = inputTensorVertexPosFlat.data();

	//[2]
	//Grab the vertex color
	const Tensor& inputTensorVertexColor = context->input(2);
	Eigen::TensorMap<Eigen::Tensor< const float, 1, 1, Eigen::DenseIndex>, 16> inputTensorVertexColorFlat = inputTensorVertexColor.flat_inner_dims<float, 1>();
	d_inputVertexColor = inputTensorVertexColorFlat.data();

	//[3]
	//Grab the texture
	const Tensor& inputTensorTexture = context->input(3);
	Eigen::TensorMap<Eigen::Tensor< const float, 1, 1, Eigen::DenseIndex>, 16> inputTensorTextureFlat = inputTensorTexture.flat_inner_dims<float, 1>();
	d_inputTexture = inputTensorTextureFlat.data();

	//[4]
	//Grab the sh coeffs 
	const Tensor& inputTensorSHCoeff = context->input(4);
	Eigen::TensorMap<Eigen::Tensor< const float, 1, 1, Eigen::DenseIndex>, 16> inputTensorSHCoeffFlat = inputTensorSHCoeff.flat_inner_dims<float, 1>();
	d_inputSHCoeff = inputTensorSHCoeffFlat.data();

	//[5]
	//Grab the vertex normals 
	const Tensor& inputTensorVertexNormal = context->input(5);
	Eigen::TensorMap<Eigen::Tensor< const float, 1, 1, Eigen::DenseIndex>, 16> inputTensorVertexNormalFlat = inputTensorVertexNormal.flat_inner_dims<float, 1>();
	d_inputVertexNormal = inputTensorVertexNormalFlat.data();

	//[6]
	//Grab the barycentric co-ordinates 
	const Tensor& inputTensorBaryCentricBuffer= context->input(6);
	Eigen::TensorMap<Eigen::Tensor< const float, 1, 1, Eigen::DenseIndex>, 16> inputTensorBaryCentricBufferFlat = inputTensorBaryCentricBuffer.flat_inner_dims<float, 1>();
	d_inputBaryCentricBuffer = inputTensorBaryCentricBufferFlat.data();

	//[7]
	//Grab the face id buffer  
	const Tensor& inputTensorFaceBuffer = context->input(7);
	Eigen::TensorMap<Eigen::Tensor< const int, 1, 1, Eigen::DenseIndex>, 16> inputTensorFaceBufferFlat = inputTensorFaceBuffer.flat_inner_dims<int, 1>();
	d_inputFaceBuffer= inputTensorFaceBufferFlat.data();

	//---MISC---

	numberOfBatches      = inputTensorVertexPos.dim_size(0); 
	textureResolutionV   = inputTensorTexture.dim_size(1);
	textureResolutionU   = inputTensorTexture.dim_size(2);

	//---OUTPUT---

	//determine the output dimensions
	std::vector<tensorflow::int64> vertexDim;
	vertexDim.push_back(numberOfBatches);
	vertexDim.push_back(numberOfPoints);
	vertexDim.push_back(3);
	tensorflow::gtl::ArraySlice<tensorflow::int64> vertexDimSize(vertexDim);

	std::vector<tensorflow::int64> texDim;
	texDim.push_back(numberOfBatches);
	texDim.push_back(textureResolutionV);
	texDim.push_back(textureResolutionU);
	texDim.push_back(3);
	tensorflow::gtl::ArraySlice<tensorflow::int64> texDimSize(texDim);

	std::vector<tensorflow::int64> shDim;
	shDim.push_back(numberOfBatches);
	shDim.push_back(numberOfCameras);
	shDim.push_back(27);
	tensorflow::gtl::ArraySlice<tensorflow::int64> shDimSize(shDim);

	//[0]
	//vertex position gradients
	tensorflow::Tensor* outputTensorVertexPosGrad;
	OP_REQUIRES_OK(context, context->allocate_output(0, tensorflow::TensorShape(vertexDimSize), &outputTensorVertexPosGrad));
	Eigen::TensorMap<Eigen::Tensor<float, 1, 1, Eigen::DenseIndex>, 16> outputTensorVertexPosGradFlat = outputTensorVertexPosGrad->flat<float>();
	d_outputVertexPosGrad = outputTensorVertexPosGradFlat.data();

	//[1]
	//vertex color gradients
	tensorflow::Tensor* outputTensorVertexColorGrad;
	OP_REQUIRES_OK(context, context->allocate_output(1, tensorflow::TensorShape(vertexDimSize), &outputTensorVertexColorGrad));
	Eigen::TensorMap<Eigen::Tensor<float, 1, 1, Eigen::DenseIndex>, 16> outputTensorVertexColorGradFlat = outputTensorVertexColorGrad->flat<float>();
	d_outputVertexColorGrad = outputTensorVertexColorGradFlat.data();

	//[2]
	//texture gradients
	tensorflow::Tensor* outputTensorTextureGrad;
	OP_REQUIRES_OK(context, context->allocate_output(2, tensorflow::TensorShape(texDimSize), &outputTensorTextureGrad));
	Eigen::TensorMap<Eigen::Tensor<float, 1, 1, Eigen::DenseIndex>, 16> outputTensorTextureGradFlat = outputTensorTextureGrad->flat<float>();
	d_outputTextureGrad = outputTensorTextureGradFlat.data();

	//[3]
	//sh coeff gradients
	tensorflow::Tensor* outputTensorSHCoeffGrad;
	OP_REQUIRES_OK(context, context->allocate_output(3, tensorflow::TensorShape(shDimSize), &outputTensorSHCoeffGrad));
	Eigen::TensorMap<Eigen::Tensor<float, 1, 1, Eigen::DenseIndex>, 16> outputTensorSHCoeffGradFlat= outputTensorSHCoeffGrad->flat<float>();
	d_outputSHCoeffGrad = outputTensorSHCoeffGradFlat.data();
}

//==============================================================================================//

void CudaRendererGrad::Compute(OpKernelContext* context)
{
	try
	{
		//setup the input and output pointers of the tensor because they change from compute to compute call
		setupInputOutputTensorPointers(context);

		for (int b = 0; b < numberOfBatches; b++)
		{
			//set input 
			cudaBasedRasterizationGrad->setTextureWidth(textureResolutionU);
			cudaBasedRasterizationGrad->setTextureHeight(textureResolutionV);
			cudaBasedRasterizationGrad->set_D_RenderBufferGrad(				(float3*)			d_inputRenderBufferGrad					+ b * numberOfCameras * renderResolutionV * renderResolutionU * 3);
			
			cudaBasedRasterizationGrad->set_D_vertices(						(float3*)			d_inputVertexPos						+ b * numberOfPoints * 3);
			cudaBasedRasterizationGrad->set_D_vertexColors(					(float3*)			d_inputVertexColor						+ b * numberOfPoints * 3);
			cudaBasedRasterizationGrad->set_D_textureMap(										d_inputTexture							+ b * textureResolutionV * textureResolutionU * 3);
	
			cudaBasedRasterizationGrad->set_D_shCoeff(											d_inputSHCoeff							+ b * numberOfCameras * 27);
			cudaBasedRasterizationGrad->set_D_vertexNormal(					(float3*)			d_inputVertexNormal						+ b * numberOfCameras * numberOfPoints * 3);
			cudaBasedRasterizationGrad->set_D_barycentricCoordinatesBuffer( (float3 *)			d_inputBaryCentricBuffer				+ b * numberOfCameras * renderResolutionV * renderResolutionU * 3);
			
			cudaBasedRasterizationGrad->set_D_faceIDBuffer(					(int4*)				d_inputFaceBuffer						+ b * numberOfCameras * renderResolutionV * renderResolutionU * 4);
			
			//set output
			cudaBasedRasterizationGrad->set_D_vertexPosGrad(				(float3*)			d_outputVertexPosGrad					+ b * numberOfPoints * 3);
			cudaBasedRasterizationGrad->set_D_vertexColorGrad(				(float3*)			d_outputVertexColorGrad					+ b * numberOfPoints * 3);
			cudaBasedRasterizationGrad->set_D_textureGrad(					(float3*)			d_outputTextureGrad						+ b * textureResolutionU * textureResolutionV * 3);
			cudaBasedRasterizationGrad->set_D_shCoeffGrad(					(float*)			d_outputSHCoeffGrad						+ b * numberOfCameras * 27);

			//get gradients
			cudaBasedRasterizationGrad->renderBuffersGrad();
		}
	}
	catch (std::exception e)
	{
		std::cerr << "Computed gradients error!" << std::endl;
	}
}

//==============================================================================================//

REGISTER_KERNEL_BUILDER(Name("CudaRendererGradGpu").Device(DEVICE_GPU), CudaRendererGrad);
