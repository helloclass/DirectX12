#pragma once
#include "../../Common/d3dUtil.h"

class Particle
{
public:
	float mTime;

public:
	typedef struct ScaleAnimation {
		ScaleAnimation() = delete;
		ScaleAnimation (float mTime, DirectX::XMFLOAT3 mScale) :
			time(mTime),
			scale(mScale)
		{};

		float time;
		DirectX::XMFLOAT3 scale;
	} ScaleAnimation;

	// The Change Animation of Scale over time.
	std::vector<ScaleAnimation> mScaleAnimation;
	std::vector<DirectX::XMVECTOR> mErrorScale;
	int mScaleAnimIndex = 0;
	bool isUseErrorScale = false;

	DirectX::XMFLOAT3 mPreScale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f };
	// Error Value of Scale
	DirectX::XMFLOAT3 mScaleError = { 0.0f, 0.0f, 0.0f };

public:
	typedef struct DiffuseAnimation {
		DiffuseAnimation() = delete;
		DiffuseAnimation(float mTime, DirectX::XMFLOAT4 mColor)
		{
			this->time = mTime;
			this->color = mColor;
		};

		float time;
		DirectX::XMFLOAT4 color;
	} DiffuseAnimation;

	// The Change Animation of Diffuse over time.
	std::vector<DiffuseAnimation>	mDiffuseAnimation;
	int mDiffuseAnimIndex = 0;

	DirectX::XMFLOAT4 mPreDiffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 mDiffuse = { 1.0f, 1.0f, 1.0f, 1.0f };

private:
	int mInstanceCount;

	float mDurationTime;
	bool  mIsLoop;
	float mStartDelay;
	float mStartLifeTime;
	bool  mPlayOnAwake;

	bool mIsFilled;

	DirectX::XMVECTOR mRotation = {0.0f, 0.0f, 0.0f, 1.0f};

	DirectX::XMFLOAT3 mMinAcc;
	DirectX::XMFLOAT3 mMaxAcc;

	DirectX::XMFLOAT3 mMinVelo;
	DirectX::XMFLOAT3 mMaxVelo;

	std::vector<DirectX::XMVECTOR> mBeginAcc;
	std::vector<DirectX::XMVECTOR> mBeginVelo;

public:
	std::vector<DirectX::XMVECTOR> mDist;
	std::vector<DirectX::XMVECTOR> mDeltaDist;

// TextureSheetAnimation
private:
	UINT x, y;
	float mFrame;

public:
	UINT mCurrFrame;

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
	void setRotation(DirectX::XMVECTOR rotation);
	void setMinAcc(DirectX::XMFLOAT3 minAcc);
	void setMaxAcc(DirectX::XMFLOAT3 maxAcc);
	void setMinVelo(DirectX::XMFLOAT3 minVelo);
	void setMaxVelo(DirectX::XMFLOAT3 maxVelo);

	float getDurationTime();
	bool getIsLoop();
	float getStartDelay();
	float getStartLifeTime();
	bool getOnPlayAwake();
	DirectX::XMVECTOR getRotation();
	DirectX::XMFLOAT3 getMinVelo();
	DirectX::XMFLOAT3 getMaxVelo();

	void Generator();
	void ParticleUpdate(float delta);

public:
	inline void setTextureSheetAnimationXY(UINT x, UINT y)
	{
		this->x = x;
		this->y = y;
	}

	inline void setTextureSheetAnimationFrame(float mFrame)
	{
		this->mFrame = mFrame;
	}

	inline float getTextureSheetAnimationFrame()
	{
		return this->mFrame;
	}

	inline DirectX::XMFLOAT4 getSprite()
	{
		int xPos = mCurrFrame / y;
		int yPos = mCurrFrame % y;

		float xDelta = 1.0f / x;
		float yDelta = 1.0f / y;

		return DirectX::XMFLOAT4(
			// Top Left
			yPos * yDelta,
			xPos * xDelta, 
			// Bottom Right
			(yPos + 1.0f) * yDelta,
			(xPos + 1.0f) * xDelta
		);
	}
};