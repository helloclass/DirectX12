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

	// AnimNode�� mName�� key, AnimClip�� Value
	std::vector<struct Clip> mClips;

	struct Clip* mCurrentClip;

public:
	bool CurrentClipIsNULL();
	const std::string getCurrentClipName() const;
	// ���� �ִϸ��̼� Ŭ�� ��ȯ
	const int getCurrentClip(
		_Out_ float& mBeginTime,
		_Out_ float& mEndTime,
		_In_  bool isCompression
	) const;
	// ���� �ִϸ��̼� Ŭ���� ����
	int setCurrentClip(
		_In_ std::string mClipName
	);
	// �̺�Ʈ�� �߻����� ���� �ִϸ��̼��� ����
	int nextEvent(
		_In_ std::string mEventName
	);
	// ���ο� Ŭ�� �߰�
	void appendClip(
		_In_ std::string mName, 
		_In_ float mStartTime,
		_In_ float mEndTime,
		_In_ bool isLoop = false
	);
	// ���ο� �̺�Ʈ �߰�
	int appendEvent(
		_In_ std::string mCurrentClipName,
		_In_ std::string mNextClipName,
		_In_ std::string mEventName
	);
};