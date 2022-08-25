#include "Animation.h"

bool AnimationClip::CurrentClipIsNULL()
{
	return !mCurrentClip;
}

const std::string AnimationClip::getCurrentClipName() const
{
	return mCurrentClip->mName;
}

const int AnimationClip::getCurrentClip(
	_Out_ float& mBeginTime,
	_Out_ float& mEndTime,
	_In_  bool isCompression
) const
{
	if (!mCurrentClip)
		return 1;

	if (isCompression)
	{
		mBeginTime = 0;
		mEndTime = 0;
		for (int idx = 0; idx < mClips.size(); idx++)
		{
			mEndTime += (mClips[idx].mEndTime - mClips[idx].mStartTime);

			if (mClips[idx].mName == mCurrentClip->mName)
			{
				break;
			}

			mBeginTime += (mClips[idx].mEndTime - mClips[idx].mStartTime);
		}
	}
	else
	{
		mBeginTime	= mCurrentClip->mStartTime;
		mEndTime	= mCurrentClip->mEndTime;
	}

	return 0;
}

int AnimationClip::setCurrentClip(
	_In_ std::string mClipName
)
{
	std::vector<AnimationClip::Clip>::iterator& begin = mClips.begin();
	std::vector<AnimationClip::Clip>::iterator& end = mClips.end();

	while (begin != end)
	{
		if (begin->mName == mClipName)
		{
			mCurrentClip = (begin._Ptr);
			return 0;
		}

		begin++;
	}
	return 1;
}

int AnimationClip::nextEvent(
	_In_ std::string mEventName
)
{
	if (!mCurrentClip || (mCurrentClip->mChildNodes.size() == 0))
		return 1;

	auto& begin = mCurrentClip->mChildNodes.begin();
	auto& end	= mCurrentClip->mChildNodes.end();

	while (begin != end)
	{
		if (begin->first == mEventName)
		{
			mCurrentClip = begin->second;
			return 0;
		}

		begin++;
	}
	return 1;
}

void AnimationClip::appendClip(
	std::string mName, 
	float mStartTime, 
	float mEndTime,
	bool isLoop
)
{
	AnimationClip::Clip clip;
	clip.mName			= mName;
	clip.mStartTime		= mStartTime;
	clip.mEndTime		= mEndTime;
	clip.isLoop			= isLoop;

	mClips.push_back(clip);
}

int AnimationClip::appendEvent(
	std::string mCurrentClipName, 
	std::string mNextClipName, 
	std::string mEventName
)
{
	std::vector<AnimationClip::Clip>::iterator& begin = mClips.begin();
	std::vector<AnimationClip::Clip>::iterator& end = mClips.end();

	AnimationClip::Clip *currClip = NULL;
	AnimationClip::Clip *nextClip = NULL;

	while (begin != end)
	{
		if (begin->mName == mCurrentClipName)
			currClip = (begin._Ptr);
		if (begin->mName == mNextClipName)
			nextClip = (begin._Ptr);

		begin++;
	}

	if (!currClip || !nextClip)
		return 1;

	currClip->mChildNodes.push_back(std::make_pair(mEventName, nextClip));
	return 0;
}