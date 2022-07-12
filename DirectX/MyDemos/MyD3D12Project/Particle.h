#pragma once
#include "../../Common/d3dUtil.h"

class Particle
{
private:
	UINT mInstanceCount;

	float mDurationTime;
	bool  mIsLoop;
	float mStartDelay;
	float mStartLifeTime;
	bool  mPlayOnAwake;

	bool mIsFilled;

	DirectX::XMFLOAT3 mMinAcc;
	DirectX::XMFLOAT3 mMaxAcc;

	DirectX::XMFLOAT3 mMinVelo;
	DirectX::XMFLOAT3 mMaxVelo;

	// 초기 속도, 가속도 
	std::vector<DirectX::XMVECTOR> mBeginAcc;
	std::vector<DirectX::XMVECTOR> mBeginVelo;

public:
	std::vector<DirectX::XMVECTOR> mDeltaDist;

public:
	Particle();

	Particle(Particle&) = delete;

	~Particle();

	void setInstanceCount(UINT instCount);
	void setDurationTime(float duration);
	void setIsLoop(bool isLoop);
	void setIsFilled(bool isFilled);
	void setStartDelay(float startDelay);
	void setStartLifeTime(float startLifeTime);
	void setOnPlayAwake(bool onPlayAwake);
	void setMinAcc(DirectX::XMFLOAT3 minAcc);
	void setMaxAcc(DirectX::XMFLOAT3 maxAcc);
	void setMinVelo(DirectX::XMFLOAT3 minVelo);
	void setMaxVelo(DirectX::XMFLOAT3 maxVelo);

	float getDurationTime();
	bool getIsLoop();
	float getStartDelay();
	float getStartLifeTime();
	bool getOnPlayAwake();
	DirectX::XMFLOAT3 getMinVelo();
	DirectX::XMFLOAT3 getMaxVelo();

	void Generator();
	void ParticleUpdate(float delta);
};