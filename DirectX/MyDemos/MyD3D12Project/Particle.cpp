#include "Particle.h"

Particle::Particle() :
	mInstanceCount(0),
	mDurationTime(0.0f),
	mIsLoop(false),
	mStartDelay(0.0f),
	mStartLifeTime(0.0f),
	mPlayOnAwake(false),
	mIsFilled(true),
	x(1), y(1),
	mFrame(0),
	mCurrFrame(0)
{}

Particle::~Particle()
{
	this->mBeginAcc.clear();
	this->mBeginVelo.clear();
	this->mDeltaDist.clear();
	this->mDist.clear();
}

void Particle::setInstanceCount(UINT instCount)
{
	this->mInstanceCount = instCount;

	this->mBeginAcc.resize(instCount);
	this->mBeginVelo.resize(instCount);
	this->mDeltaDist.resize(instCount);
	this->mDist.resize(instCount);
	this->mErrorScale.resize(instCount);
}

void Particle::setDurationTime(float duration)
{
	this->mDurationTime = duration;
}
void Particle::setIsLoop(bool isLoop)
{
	this->mIsLoop = isLoop;
}
void Particle::setIsFilled(bool isFilled)
{
	this->mIsFilled = isFilled;
}
void Particle::setStartDelay(float startDelay)
{
	this->mStartDelay = startDelay;
}
void Particle::setStartLifeTime(float startLifeTime)
{
	this->mStartLifeTime = startLifeTime;
}
void Particle::setOnPlayAwake(bool onPlayAwake)
{
	this->mPlayOnAwake = onPlayAwake;
}
void Particle::setRotation(DirectX::XMVECTOR rotation)
{
	this->mRotation = rotation;
}
void Particle::setMinAcc(DirectX::XMFLOAT3 minAcc)
{
	this->mMinAcc = minAcc;
}
void Particle::setMaxAcc(DirectX::XMFLOAT3 maxAcc)
{
	this->mMaxAcc = maxAcc;
}
void Particle::setMinVelo(DirectX::XMFLOAT3 minVelo)
{
	this->mMinVelo = minVelo;
}
void Particle::setMaxVelo(DirectX::XMFLOAT3 maxVelo)
{
	this->mMaxVelo = maxVelo;
}

float Particle::getDurationTime()
{
	return this->mDurationTime;
}
bool Particle::getIsLoop()
{
	return this->mIsLoop;
}
float Particle::getStartDelay()
{
	return this->mStartDelay;
}
float Particle::getStartLifeTime()
{
	return this->mStartLifeTime;
}
bool Particle::getOnPlayAwake()
{
	return this->mPlayOnAwake;
}
DirectX::XMVECTOR Particle::getRotation()
{
	return this->mRotation;
}
DirectX::XMFLOAT3 Particle::getMinVelo()
{
	return this->mMinVelo;
}
DirectX::XMFLOAT3 Particle::getMaxVelo()
{
	return this->mMaxVelo;
}

void Particle::Generator()
{
	std::random_device rd;
	std::mt19937 gen(rd());

	// Accumulator, Velocity Vector에 로테이션을 적용.
	DirectX::XMVECTOR mAdMin = DirectX::XMLoadFloat3(&this->mMinAcc);
	mAdMin = DirectX::XMVector3Rotate(
		mAdMin,
		DirectX::XMQuaternionRotationRollPitchYawFromVector(this->mRotation)
	);

	DirectX::XMVECTOR mAdMax = DirectX::XMLoadFloat3(&this->mMaxAcc);
	mAdMax = DirectX::XMVector3Rotate(
		mAdMax,
		DirectX::XMQuaternionRotationRollPitchYawFromVector(this->mRotation)
	);

	float swap;
	for (int i = 0; i < 3; i++)
	{
		if (mAdMin.m128_f32[i] > mAdMax.m128_f32[i])
		{
			swap = mAdMin.m128_f32[i];
			mAdMin.m128_f32[i] = mAdMax.m128_f32[i];
			mAdMax.m128_f32[i] = swap;
		}
	}

	std::uniform_real_distribution<float> accX(mAdMin.m128_f32[0], mAdMax.m128_f32[0]);
	std::uniform_real_distribution<float> accY(mAdMin.m128_f32[1], mAdMax.m128_f32[1]);
	std::uniform_real_distribution<float> accZ(mAdMin.m128_f32[2], mAdMax.m128_f32[2]);

	mAdMin = DirectX::XMLoadFloat3(&this->mMinVelo);
	mAdMin = DirectX::XMVector3Rotate(
		mAdMin,
		DirectX::XMQuaternionRotationRollPitchYawFromVector(this->mRotation)
	);

	mAdMax = DirectX::XMLoadFloat3(&this->mMaxVelo);
	mAdMax = DirectX::XMVector3Rotate(
		mAdMax,
		DirectX::XMQuaternionRotationRollPitchYawFromVector(this->mRotation)
	);

	for (int i = 0; i < 3; i++)
	{
		if (mAdMin.m128_f32[i] > mAdMax.m128_f32[i])
		{
			swap = mAdMin.m128_f32[i];
			mAdMin.m128_f32[i] = mAdMax.m128_f32[i];
			mAdMax.m128_f32[i] = swap;
		}
	}

	std::uniform_real_distribution<float> veloX(mAdMin.m128_f32[0], mAdMax.m128_f32[0]);
	std::uniform_real_distribution<float> veloY(mAdMin.m128_f32[1], mAdMax.m128_f32[1]);
	std::uniform_real_distribution<float> veloZ(mAdMin.m128_f32[2], mAdMax.m128_f32[2]);

	if (this->mIsFilled)
	{
		for (int i = 0; i < this->mInstanceCount; i++)
		{
			this->mBeginAcc[i].m128_f32[0] = accX(gen);
			this->mBeginAcc[i].m128_f32[1] = accY(gen);
			this->mBeginAcc[i].m128_f32[2] = accZ(gen);
			this->mBeginAcc[i].m128_f32[3] = 0.0f;

			this->mBeginVelo[i].m128_f32[0] = veloX(gen);
			this->mBeginVelo[i].m128_f32[1] = veloY(gen);
			this->mBeginVelo[i].m128_f32[2] = veloZ(gen);
			this->mBeginVelo[i].m128_f32[3] = 0.0f;
		}
	}
	else
	{
		int quarter = this->mInstanceCount * 0.25f;

		for (int i = 0; i < quarter; i++)
		{
			this->mBeginAcc[i].m128_f32[0] = accX(gen);
			this->mBeginAcc[i].m128_f32[1] = accY(gen);
			this->mBeginAcc[i].m128_f32[2] = accZ(gen);
			this->mBeginAcc[i].m128_f32[3] = 0.0f;

			this->mBeginVelo[i].m128_f32[0] = this->mMinVelo.x;
			this->mBeginVelo[i].m128_f32[1] = veloY(gen);
			this->mBeginVelo[i].m128_f32[2] = veloZ(gen);
			this->mBeginVelo[i].m128_f32[3] = 0.0f;
		}

		for (int i = quarter; i < quarter * 2; i++)
		{
			this->mBeginAcc[i].m128_f32[0] = accX(gen);
			this->mBeginAcc[i].m128_f32[1] = accY(gen);
			this->mBeginAcc[i].m128_f32[2] = accZ(gen);
			this->mBeginAcc[i].m128_f32[3] = 0.0f;

			this->mBeginVelo[i].m128_f32[0] = this->mMaxVelo.x;
			this->mBeginVelo[i].m128_f32[1] = veloY(gen);
			this->mBeginVelo[i].m128_f32[2] = veloZ(gen);
			this->mBeginVelo[i].m128_f32[3] = 0.0f;
		}

		for (int i = quarter * 2; i < quarter * 3; i++)
		{
			this->mBeginAcc[i].m128_f32[0] = accX(gen);
			this->mBeginAcc[i].m128_f32[1] = accY(gen);
			this->mBeginAcc[i].m128_f32[2] = accZ(gen);
			this->mBeginAcc[i].m128_f32[3] = 0.0f;

			this->mBeginVelo[i].m128_f32[0] = veloX(gen);
			this->mBeginVelo[i].m128_f32[1] = veloY(gen);
			this->mBeginVelo[i].m128_f32[2] = this->mMinVelo.z;
			this->mBeginVelo[i].m128_f32[3] = 0.0f;
		}

		for (int i = quarter * 3; i < quarter * 4; i++)
		{
			this->mBeginAcc[i].m128_f32[0] = accX(gen);
			this->mBeginAcc[i].m128_f32[1] = accY(gen);
			this->mBeginAcc[i].m128_f32[2] = accZ(gen);
			this->mBeginAcc[i].m128_f32[3] = 0.0f;

			this->mBeginVelo[i].m128_f32[0] = veloX(gen);
			this->mBeginVelo[i].m128_f32[1] = veloY(gen);
			this->mBeginVelo[i].m128_f32[2] = this->mMaxVelo.z;
			this->mBeginVelo[i].m128_f32[3] = 0.0f;
		}
	}
}

void Particle::ParticleUpdate(float delta)
{
	for (int i = 0; i < this->mInstanceCount; i++)
	{
		this->mBeginVelo[i].m128_f32[0] += this->mBeginAcc[i].m128_f32[0] * delta;
		this->mBeginVelo[i].m128_f32[1] += this->mBeginAcc[i].m128_f32[1] * delta;
		this->mBeginVelo[i].m128_f32[2] += this->mBeginAcc[i].m128_f32[2] * delta;
		this->mBeginVelo[i].m128_f32[3] += this->mBeginAcc[i].m128_f32[3] * delta;

		this->mDeltaDist[i].m128_f32[0] = this->mBeginVelo[i].m128_f32[0] * delta;
		this->mDeltaDist[i].m128_f32[1] = this->mBeginVelo[i].m128_f32[1] * delta;
		this->mDeltaDist[i].m128_f32[2] = this->mBeginVelo[i].m128_f32[2] * delta;
		this->mDeltaDist[i].m128_f32[3] = this->mBeginVelo[i].m128_f32[3] * delta;

		this->mDist[i].m128_f32[0] += this->mDeltaDist[i].m128_f32[0];
		this->mDist[i].m128_f32[1] += this->mDeltaDist[i].m128_f32[1];
		this->mDist[i].m128_f32[2] += this->mDeltaDist[i].m128_f32[2];
		this->mDist[i].m128_f32[3] += this->mDeltaDist[i].m128_f32[3];
	}
}