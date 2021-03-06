#include "AmbientOcclussionScene.h"

#include <tbb/parallel_for.h>

#include <GeometryPass/Recorders/ColorCmdListRecorder.h>
#include <GeometryPass/Recorders/NormalCmdListRecorder.h>
#include <GlobalData/D3dData.h>
#include <Material/Material.h>
#include <MathUtils\MathUtils.h>
#include <ModelManager\Mesh.h>
#include <ModelManager\ModelManager.h>
#include <ResourceManager\ResourceManager.h>
#include <Scene/SceneUtils.h>

namespace {
	SceneUtils::ResourceContainer sResourceContainer;

	enum Textures {
		// Metal
		METAL = 0U,
		METAL_NORMAL,

		// Wood
		WOOD,
		WOOD_NORMAL,

		// Environment
		SKY_BOX,
		DIFFUSE_CUBE_MAP,
		SPECULAR_CUBE_MAP,

		TEXTURES_COUNT
	};

	// Textures to load
	std::vector<std::string> sTexFiles =
	{
		// Metal
		"textures/metal/metal.dds",
		"textures/metal/metal_normal.dds",

		// Wood
		"textures/wood/wood.dds",
		"textures/wood/wood_normal.dds",

		// Environment
		"textures/cubeMaps/milkmill_cube_map.dds",
		"textures/cubeMaps/milkmill_diffuse_cube_map.dds",
		"textures/cubeMaps/milkmill_specular_cube_map.dds",
	};

	enum Models {
		UNREAL,
		FLOOR,

		MODELS_COUNT
	};

	// Models to load
	std::vector<std::string> sModelFiles =
	{
		"models/unreal.obj",
		"models/floor.obj",
	};

	const float sFloorScale{ 1.0f };
	const float sFloorTx{ -150.0f };
	const float sFloorTy{ -20.5f };
	const float sFloorTz{ 150.0f };

	const float sModel{ 0.05f };

	const float sTx{ 0.0f };
	const float sTy{ sFloorTy };
	const float sOffsetX{ 15.0f };

	const float sTz0{ 0.0f };
	const float sTz1{ -15.0f };
	const float sTz2{ -30.0f };
	const float sTz3{ -45.0f };
	const float sTz4{ -60.0f };
	const float sTz5{ -75.0f };
	const float sTz6{ -90.0f };
	const float sTz7{ -105.0f };
	const float sTz8{ -120.0f };
	const float sTz9{ -135.0f };

	void GenerateRecorder(
		const float initX,
		const float initY,
		const float initZ,
		const float offsetX,
		const float offsetY,
		const float offsetZ,
		const float scaleFactor,
		const std::vector<Mesh>& meshes,
		ID3D12Resource** textures,
		ID3D12Resource** normals,
		Material* materials,
		const std::size_t numMaterials,
		NormalCmdListRecorder* &recorder) {

		ASSERT(textures != nullptr);
		ASSERT(normals != nullptr);

		// Fill geometry data
		const std::size_t numMeshes{ meshes.size() };
		ASSERT(numMeshes > 0UL);
		std::vector<GeometryPassCmdListRecorder::GeometryData> geomDataVec;
		geomDataVec.resize(numMeshes);
		for (std::size_t i = 0UL; i < numMeshes; ++i) {
			GeometryPassCmdListRecorder::GeometryData& geomData{ geomDataVec[i] };
			const Mesh& mesh{ meshes[i] };
			geomData.mVertexBufferData = mesh.VertexBufferData();
			geomData.mIndexBufferData = mesh.IndexBufferData();
			geomData.mWorldMatrices.reserve(numMaterials);
		}

		// Fill material and textures
		std::vector<Material> materialsVec;
		materialsVec.resize(numMaterials * numMeshes);
		std::vector<ID3D12Resource*> texturesVec;
		texturesVec.resize(numMaterials * numMeshes);
		std::vector<ID3D12Resource*> normalsVec;
		normalsVec.resize(numMaterials * numMeshes);

		float tx{ initX };
		float ty{ initY };
		float tz{ initZ };
		for (std::size_t i = 0UL; i < numMaterials; ++i) {
			DirectX::XMFLOAT4X4 w;
			MathUtils::ComputeMatrix(w, tx, ty, tz, scaleFactor, scaleFactor, scaleFactor);

			Material& mat(materials[i]);
			ID3D12Resource* texture{ textures[i] };
			ID3D12Resource* normal{ normals[i] };
			for (std::size_t j = 0UL; j < numMeshes; ++j) {
				const std::size_t index{ i + j * numMaterials };
				materialsVec[index] = mat;
				texturesVec[index] = texture;
				normalsVec[index] = normal;
				GeometryPassCmdListRecorder::GeometryData& geomData{ geomDataVec[j] };
				geomData.mWorldMatrices.push_back(w);
			}

			tx += offsetX;
			ty += offsetY;
			tz += offsetZ;
		}

		// Create recorder
		recorder = new NormalCmdListRecorder(D3dData::Device());
		recorder->Init(
			geomDataVec.data(),
			static_cast<std::uint32_t>(geomDataVec.size()),
			materialsVec.data(),
			texturesVec.data(),
			normalsVec.data(),
			static_cast<std::uint32_t>(materialsVec.size()));
	}

	void GenerateRecorder(
		const float initX,
		const float initY,
		const float initZ,
		const float offsetX,
		const float offsetY,
		const float offsetZ,
		const float scaleFactor,
		const std::vector<Mesh>& meshes,
		Material* materials,
		const std::size_t numMaterials,
		ColorCmdListRecorder* &recorder) {

		// Fill geometry data
		const std::size_t numMeshes{ meshes.size() };
		ASSERT(numMeshes > 0UL);
		std::vector<GeometryPassCmdListRecorder::GeometryData> geomDataVec;
		geomDataVec.resize(numMeshes);
		for (std::size_t i = 0UL; i < numMeshes; ++i) {
			GeometryPassCmdListRecorder::GeometryData& geomData{ geomDataVec[i] };
			const Mesh& mesh{ meshes[i] };
			geomData.mVertexBufferData = mesh.VertexBufferData();
			geomData.mIndexBufferData = mesh.IndexBufferData();
			geomData.mWorldMatrices.reserve(numMaterials);
		}

		// Fill material and textures
		std::vector<Material> materialsVec;
		materialsVec.resize(numMaterials * numMeshes);

		float tx{ initX };
		float ty{ initY };
		float tz{ initZ };
		for (std::size_t i = 0UL; i < numMaterials; ++i) {
			DirectX::XMFLOAT4X4 w;
			MathUtils::ComputeMatrix(w, tx, ty, tz, scaleFactor, scaleFactor, scaleFactor);

			Material& mat(materials[i]);
			for (std::size_t j = 0UL; j < numMeshes; ++j) {
				const std::size_t index{ i + j * numMaterials };
				materialsVec[index] = mat;
				GeometryPassCmdListRecorder::GeometryData& geomData{ geomDataVec[j] };
				geomData.mWorldMatrices.push_back(w);
			}

			tx += offsetX;
			ty += offsetY;
			tz += offsetZ;
		}

		// Create recorder
		recorder = new ColorCmdListRecorder(D3dData::Device());
		recorder->Init(
			geomDataVec.data(),
			static_cast<std::uint32_t>(geomDataVec.size()),
			materialsVec.data(),
			static_cast<std::uint32_t>(materialsVec.size()));
	}

	void GenerateFloorRecorder(
		const std::vector<Mesh>& meshes,
		ID3D12Resource* texture,
		ID3D12Resource* normal,
		NormalCmdListRecorder* &recorder) {

		ASSERT(texture != nullptr);
		ASSERT(normal != nullptr);

		// Compute world matrix
		DirectX::XMFLOAT4X4 w;
		MathUtils::ComputeMatrix(w, sFloorTx, sFloorTy, sFloorTz, sFloorScale, sFloorScale, sFloorScale);

		const std::size_t numMeshes{ meshes.size() };
		ASSERT(numMeshes > 0UL);

		// Fill geometry data
		std::vector<GeometryPassCmdListRecorder::GeometryData> geomDataVec;
		geomDataVec.resize(numMeshes);
		for (std::size_t i = 0UL; i < numMeshes; ++i) {
			GeometryPassCmdListRecorder::GeometryData& geomData{ geomDataVec[i] };
			const Mesh& mesh{ meshes[i] };

			geomData.mVertexBufferData = mesh.VertexBufferData();
			geomData.mIndexBufferData = mesh.IndexBufferData();

			geomData.mWorldMatrices.push_back(w);
		}

		// Fill material
		Material material{ 1.0f, 1.0f, 1.0f, 0.0f, 0.85f };

		// Build recorder
		recorder = new NormalCmdListRecorder(D3dData::Device());
		recorder->Init(
			geomDataVec.data(),
			static_cast<std::uint32_t>(geomDataVec.size()),
			&material,
			&texture,
			&normal,
			1);
	}
}

void AmbientOcclussionScene::Init(ID3D12CommandQueue& cmdQueue) noexcept {
	Scene::Init(cmdQueue);

	// Load textures
	sResourceContainer.LoadTextures(sTexFiles, cmdQueue, *mCmdAlloc, *mCmdList, *mFence);

	// Load models
	sResourceContainer.LoadModels(sModelFiles, cmdQueue, *mCmdAlloc, *mCmdList, *mFence);
}

void AmbientOcclussionScene::GenerateGeomPassRecorders(
	std::vector<std::unique_ptr<GeometryPassCmdListRecorder>>& tasks) noexcept {

	ASSERT(tasks.empty());
	ASSERT(ValidateData());

	std::vector<ID3D12Resource*>& textures = sResourceContainer.GetResources();
	ASSERT(textures.empty() == false);
	Model& model = sResourceContainer.GetModel(UNREAL);
	Model& floor = sResourceContainer.GetModel(FLOOR);

	//
	// Generate floor
	//
	NormalCmdListRecorder* recorder{ nullptr };
	GenerateFloorRecorder(
		floor.Meshes(),
		textures[WOOD],
		textures[WOOD_NORMAL],
		recorder);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	//
	// Generate metals
	//	
	std::vector<Material> materials =
	{
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.95f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.85f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.75f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.65f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.55f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.45f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.35f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.25f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.15f },
		{ 1.0f, 1.0f, 1.0f, 1.0f, 0.05f },
	};

	std::vector<ID3D12Resource*> diffuses =
	{
		textures[METAL],
		textures[METAL],
		textures[METAL],
		textures[METAL],
		textures[METAL],
		textures[METAL],
		textures[METAL],
		textures[METAL],
		textures[METAL],
		textures[METAL],
	};

	std::vector<ID3D12Resource*> normals =
	{
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
		textures[METAL_NORMAL],
	};

	ASSERT(materials.size() == diffuses.size());
	ASSERT(normals.size() == diffuses.size());

	GenerateRecorder(
		sTx,
		sTy,
		sTz0,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz1,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz2,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz3,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz4,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz5,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz6,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz7,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz8,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));

	GenerateRecorder(
		sTx,
		sTy,
		sTz9,
		sOffsetX,
		0.0f,
		0.0f,
		sModel,
		model.Meshes(),
		diffuses.data(),
		normals.data(),
		materials.data(),
		materials.size(),
		recorder);
	ASSERT(recorder != nullptr);
	tasks.push_back(std::unique_ptr<GeometryPassCmdListRecorder>(recorder));
}

void AmbientOcclussionScene::GenerateLightingPassRecorders(
	Microsoft::WRL::ComPtr<ID3D12Resource>*,
	const std::uint32_t,
	ID3D12Resource&,
	std::vector<std::unique_ptr<LightingPassCmdListRecorder>>&) noexcept
{
}

void AmbientOcclussionScene::GenerateCubeMaps(
	ID3D12Resource* &skyBoxCubeMap,
	ID3D12Resource* &diffuseIrradianceCubeMap,
	ID3D12Resource* &specularPreConvolvedCubeMap) noexcept
{
	skyBoxCubeMap = &sResourceContainer.GetResource(SKY_BOX);
	diffuseIrradianceCubeMap = &sResourceContainer.GetResource(DIFFUSE_CUBE_MAP);
	specularPreConvolvedCubeMap = &sResourceContainer.GetResource(SPECULAR_CUBE_MAP);
}

