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

	DirectX::XMFLOAT3 mMinVelo;
	DirectX::XMFLOAT3 mMaxVelo;

	// 초기 속도, 가속도 
	std::vector<DirectX::XMFLOAT3> mBeginVelo;

public:
	Particle() = delete;
	Particle(UINT mInstanceCount);

	Particle(Particle&) = delete;

	~Particle();

	void setDurationTime(float duration);
	void setIsLoop(bool isLoop);
	void setStartDelay(float startDelay);
	void setStartLifeTime(float startLifeTime);
	void setOnPlayAwake(bool onPlayAwake);
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
};