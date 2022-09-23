#include "Animation.h"

bool AnimationClip::CurrentClipIsNULL(int mInstanceOffset)
{
	if (mInstanceOffset < mCurrentClip.size())
		return !mCurrentClip[mInstanceOffset];
	return false;
}

const std::string AnimationClip::getCurrentClipName(int mInstanceOffset) const
{
	if (mInstanceOffset < mCurrentClip.size())
		return mCurrentClip[mInstanceOffset]->mName;
	return "";
}

const int AnimationClip::getCurrentClip(
	int mInstanceOffset,
	float& mBeginTime,
	float& mEndTime,
	bool isCompression
) const
{
	if (!mCurrentClip[mInstanceOffset])
		return 1;

	if (isCompression)
	{
		mBeginTime = 0;
		mEndTime = 0;
		for (int idx = 0; idx < mClips.size(); idx++)
		{
			mEndTime += (mClips[idx].mEndTime - mClips[idx].mStartTime);

			if (mClips[idx].mName == mCurrentClip[mInstanceOffset]->mName)
			{
				break;
			}

			mBeginTime += (mClips[idx].mEndTime - mClips[idx].mStartTime);
		}
	}
	else
	{
		mBeginTime	= mCurrentClip[mInstanceOffset]->mStartTime;
		mEndTime	= mCurrentClip[mInstanceOffset]->mEndTime;
	}

	return 0;
}

int AnimationClip::pushClip(
	_In_ std::string mClipName
)
{
	std::vector<AnimationClip::Clip>::iterator& begin = mClips.begin();
	std::vector<AnimationClip::Clip>::iterator& end = mClips.end();

	while (begin != end)
	{
		if (begin->mName == mClipName)
		{
			mStackClip.push_back(begin._Ptr);
			return 0;
		}

		begin++;
	}
	return 1;
}

int AnimationClip::setCurrentClip(
	std::string mClipName,
	int mInstanceOffset
)
{
	std::vector<AnimationClip::Clip>::iterator& begin = mClips.begin();
	std::vector<AnimationClip::Clip>::iterator& end = mClips.end();

	while (begin != end)
	{
		if (begin->mName == mClipName)
		{
			mCurrentClip[mInstanceOffset] = (begin._Ptr);
			return 0;
		}

		begin++;
	}
	return 1;
}

int AnimationClip::nextEvent(
	std::string mEventName,
	int mInstanceOffset
)
{
	if (!mCurrentClip[mInstanceOffset] || (mCurrentClip[mInstanceOffset]->mChildNodes.size() == 0))
		return 1;

	auto& begin = mCurrentClip[mInstanceOffset]->mChildNodes.begin();
	auto& end	= mCurrentClip[mInstanceOffset]->mChildNodes.end();

	while (begin != end)
	{
		if (begin->first == mEventName)
		{
			mCurrentClip[mInstanceOffset] = begin->second;
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