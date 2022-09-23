#pragma once
#include "../../Common/d3dUtil.h"

class AnimationClip {
public:
	typedef struct Clip {
		std::string mName;
		float mStartTime;
		float mEndTime;
		bool isLoop;
		std::vector<std::pair<std::string, struct Clip*>> mChildNodes;
	}Clip;

	AnimationClip(int mInstanceCount) {
		mCurrentClip.resize(mInstanceCount);
	}
	~AnimationClip() {
		mCurrentClip.clear();
	}

	// AnimNode의 mName이 key, AnimClip이 Value
	std::vector<struct Clip> mClips;

	std::vector<Clip*> mCurrentClip;
	std::vector<struct Clip*> mStackClip;

public:
	bool CurrentClipIsNULL(int mInstanceOffset);
	const std::string getCurrentClipName(int mInstanceOffset) const;
	// 현재 애니메이션 클립 반환
	const int getCurrentClip(
		_In_ int mInstanceOffset,
		_Out_ float& mBeginTime,
		_Out_ float& mEndTime,
		_In_  bool isCompression
	) const;
	// 다음 실행할 애니메이션 클립을 쌓음.
	int pushClip(
		_In_ std::string mClipName
	);
	// 현재 애니메이션 클립을 설정
	int setCurrentClip(
		_In_ std::string mClipName,
		_In_ int mInstanceOffset
	);
	// 이벤트를 발생시켜 다음 애니메이션을 실행
	int nextEvent(
		_In_ std::string mEventName,
		_In_ int mInstanceOffset
	);
	// 새로운 클립 추가
	void appendClip(
		_In_ std::string mName, 
		_In_ float mStartTime,
		_In_ float mEndTime,
		_In_ bool isLoop = false
	);
	// 새로운 이벤트 추가
	int appendEvent(
		_In_ std::string mCurrentClipName,
		_In_ std::string mNextClipName,
		_In_ std::string mEventName
	);
};