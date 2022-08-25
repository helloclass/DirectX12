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

	AnimationClip() {
		mCurrentClip = new struct Clip();
	}
	~AnimationClip() {
		delete(mCurrentClip);
	}

	// AnimNode의 mName이 key, AnimClip이 Value
	std::vector<struct Clip> mClips;

	struct Clip* mCurrentClip;

public:
	bool CurrentClipIsNULL();
	const std::string getCurrentClipName() const;
	// 현재 애니메이션 클립 반환
	const int getCurrentClip(
		_Out_ float& mBeginTime,
		_Out_ float& mEndTime,
		_In_  bool isCompression
	) const;
	// 현재 애니메이션 클립을 설정
	int setCurrentClip(
		_In_ std::string mClipName
	);
	// 이벤트를 발생시켜 다음 애니메이션을 실행
	int nextEvent(
		_In_ std::string mEventName
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