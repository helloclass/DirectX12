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

	// AnimNode�� mName�� key, AnimClip�� Value
	std::vector<struct Clip> mClips;

	std::vector<Clip*> mCurrentClip;
	std::vector<struct Clip*> mStackClip;

public:
	bool CurrentClipIsNULL(int mInstanceOffset);
	const std::string getCurrentClipName(int mInstanceOffset) const;
	// ���� �ִϸ��̼� Ŭ�� ��ȯ
	const int getCurrentClip(
		_In_ int mInstanceOffset,
		_Out_ float& mBeginTime,
		_Out_ float& mEndTime,
		_In_  bool isCompression
	) const;
	// ���� ������ �ִϸ��̼� Ŭ���� ����.
	int pushClip(
		_In_ std::string mClipName
	);
	// ���� �ִϸ��̼� Ŭ���� ����
	int setCurrentClip(
		_In_ std::string mClipName,
		_In_ int mInstanceOffset
	);
	// �̺�Ʈ�� �߻����� ���� �ִϸ��̼��� ����
	int nextEvent(
		_In_ std::string mEventName,
		_In_ int mInstanceOffset
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