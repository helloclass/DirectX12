#include "Particle.h"

Particle::Particle(UINT mInstanceCount) :
	mInstanceCount(mInstanceCount),
	mDurationTime(0.0f),
	mIsLoop(false),
	mStartDelay(0.0f),
	mStartLifeTime(0.0f),
	mPlayOnAwake(false)
{
	this->mBeginVelo.resize(mInstanceCount);
}

Particle::~Particle()
{
	try {
		this->mBeginVelo.clear();
	}
	catch (std::exception& e)
	{
		throw std::runtime_error("소멸 실패");
	}
}

void Particle::setDurationTime(float duration)
{
	this->mDurationTime = duration;
}
void Particle::setIsLoop(bool isLoop)
{
	this->mIsLoop = isLoop;
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

	std::uniform_real_distribution<float> veloX(this->mMinVelo.x, this->mMaxVelo.x);
	std::uniform_real_distribution<float> veloY(this->mMinVelo.y, this->mMaxVelo.y);
	std::uniform_real_distribution<float> veloZ(this->mMinVelo.z, this->mMaxVelo.z);

	// 난수
	for (int i = 0; i < this->mInstanceCount; i++)
	{
		this->mBeginVelo[i].x = veloX(gen);
		this->mBeginVelo[i].y = veloY(gen);
		this->mBeginVelo[i].z = veloZ(gen);
	}
}