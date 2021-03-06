#include "Camera.h"

using namespace DirectX;

void Camera::SetLens(const float fovY, const float aspect, const float zn, const float zf) noexcept {
	const XMMATRIX proj{ XMMatrixPerspectiveFovLH(fovY, aspect, zn, zf) };
	XMStoreFloat4x4(&mProj, proj);
	XMStoreFloat4x4(&mInvProj, DirectX::XMMatrixInverse(nullptr, proj));
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp) noexcept {
	const XMVECTOR L( XMVector3Normalize(XMVectorSubtract(target, pos)) );
	const XMVECTOR R( XMVector3Normalize(XMVector3Cross(worldUp, L)) );
	const XMVECTOR U( XMVector3Cross(L, R) );

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up) noexcept {
	const XMVECTOR P( XMLoadFloat3(&pos) );
	const XMVECTOR T( XMLoadFloat3(&target) );
	const XMVECTOR U( XMLoadFloat3(&up) );

	LookAt(P, T, U);
}


void Camera::GetView4x4f(DirectX::XMFLOAT4X4& m) const noexcept {
	m = mView;
}

void Camera::GetInvView4x4f(DirectX::XMFLOAT4X4& m) const noexcept {
	DirectX::XMMATRIX v = DirectX::XMLoadFloat4x4(&mView);
	DirectX::XMMatrixInverse(nullptr, v);
	XMStoreFloat4x4(&m, DirectX::XMMatrixInverse(nullptr, v));
}

void Camera::GetProj4x4f(DirectX::XMFLOAT4X4& m) const noexcept {
	m = mProj;
}

void Camera::GetInvProj4x4f(DirectX::XMFLOAT4X4& m) const noexcept {
	m = mInvProj;
}

void Camera::Strafe(const float d) noexcept {
	// velocity += right * d 
	XMVECTOR r(XMLoadFloat3(&mRight));
	r = DirectX::XMVectorScale(r, d);
	DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(&mVelocity);
	vel = DirectX::XMVectorAdd(vel, r);
	DirectX::XMStoreFloat3(&mVelocity, vel);
}

void Camera::Walk(const float d) noexcept {
	// velocity += look * d 
	XMVECTOR l(XMLoadFloat3(&mLook));
	l = DirectX::XMVectorScale(l, d);
	DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(&mVelocity);
	vel = DirectX::XMVectorAdd(vel, l);
	DirectX::XMStoreFloat3(&mVelocity, vel);
}

void Camera::Pitch(const float angle) noexcept {
	// Rotate up and look vector about the right vector.
	const XMMATRIX R(XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
}

void Camera::RotateY(const float angle) noexcept {
	// Rotate the basis vectors about the world y-axis.
	const XMMATRIX R(XMMatrixRotationY(angle));
	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
}

void Camera::UpdateViewMatrix(const float deltaTime) noexcept {
	static float maxVelocitySpeed{ 100.0f }; // speed = velocity magnitude
	static float velocityDamp{ 0.1f }; // fraction of velocity retained per second

	// Clamp velocity
	XMVECTOR vel = XMLoadFloat3(&mVelocity);
	const float velLen = XMVectorGetX(XMVector3Length(vel));
	const float velocitySpeed = MathUtils::Clamp(velLen, 0.0f, maxVelocitySpeed);
	if (velocitySpeed > 0.0f) {
		vel = XMVector3Normalize(vel) * velocitySpeed;
	}

	// Apply velocity and pitch-way-roll
	XMVECTOR pos = XMLoadFloat3(&mPosition);
	pos = pos + vel * deltaTime;
	
	// Damp velocity
	vel = vel * static_cast<float>(pow(velocityDamp, deltaTime));
	
	// Keep camera's axes orthogonal to each other and of unit length.
	XMVECTOR right(XMLoadFloat3(&mRight));
	XMVECTOR look(XMLoadFloat3(&mLook));
	look = XMVector3Normalize(look);
	XMVECTOR up = XMVector3Normalize(XMVector3Cross(look, right));

	// U, L already orthonormal, so no need to normalize cross product.
	right = XMVector3Cross(up, look);

	XMStoreFloat3(&mRight, right);
	XMStoreFloat3(&mUp, up);
	XMStoreFloat3(&mLook, look);

	XMMATRIX viewMatrix = XMMatrixLookToLH(pos, look, up);
	XMStoreFloat4x4(&mView, viewMatrix);

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mVelocity, vel);
}