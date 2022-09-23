#pragma once

#include "../../Common/d3dUtil.h"
#include "../../Common/GameTimer.h"

typedef enum ImGuiComponentType
{
	IMGUI_TEXT_COMPONENT,
	IMGUI_IMAGE_COMPONENT,
	IMGUI_TOGGLE_COMPONENT,
	IMGUI_SLIDER_FLOAT_COMPONENT,
	IMGUI_SLIDER_FLOAT_3_COMPONENT,
	IMGUI_BOTTON_COMPONENT,
	IMGUI_COLOR_EDIT_3_COMPONENT,
	IMGUI_SCROLL_BAR_COMPONENT,
}ComponentType;

class ImGuiComponenet
{
	std::string mName;
	std::string mHint;

	bool isNewLine;

public:
	ImGuiComponentType mType;

	// 옵져버 역할을 하는 변수의 주소를 받아와, 만일 컴포넌트의 변화가 있을 시, 옵져버를 변경한다.
	bool isFence = false;
	int* mFence = nullptr;

public:
	ImGuiComponenet() = delete;
	ImGuiComponenet& operator=(ImGuiComponenet) = delete;

	ImGuiComponenet(
		std::string name,
		ImGuiComponentType type,
		bool isNewLine = true
	) : mName(name), mHint(""), isNewLine(isNewLine), mType(type)
	{}

	ImGuiComponenet(
		std::string name, 
		std::string hint, 
		ImGuiComponentType type,
		bool isNewLine = true
	): mName(name), mHint(hint), isNewLine(isNewLine), mType(type)
	{}

	void setNewLine(bool isNewLine)
	{
		this->isNewLine = isNewLine;
	}

	const char* getName()
	{
		return mName.c_str();
	}
	const char* getHint()
	{
		return mHint.c_str();
	}
	bool getNewLine()
	{
		return isNewLine;
	}

	void bindFence(int* fence)
	{
		isFence = true;
		mFence = fence;
	}
};

class ImGuiTextComponent : public ImGuiComponenet
{
public:
	std::string mText;

public:
	ImGuiTextComponent(
		std::string name,
		std::string text
	) :ImGuiComponenet(name, ImGuiComponentType::IMGUI_TEXT_COMPONENT),
		mText(text)
	{}

	ImGuiTextComponent(
		std::string name,
		std::string hint,
		std::string text
	) :ImGuiComponenet(name, hint, ImGuiComponentType::IMGUI_TEXT_COMPONENT),
		mText(text)
	{}

	void setText(std::string text)
	{
		mText = text;
	}

	const char* getText()
	{
		return mText.c_str();
	}
};

class ImGuiImageComponent : public ImGuiComponenet
{
public:
	UINT64 mImagePTR;
	float mWidth;
	float mHeight;

	float* mGauge = nullptr;

public:
	ImGuiImageComponent(
		std::string name,
		UINT64 imagePTR,
		float width,
		float height
	) :ImGuiComponenet(name, ImGuiComponentType::IMGUI_IMAGE_COMPONENT),
		mImagePTR(imagePTR),
		mWidth(width),
		mHeight(height),
		mGauge(nullptr)
	{}
	ImGuiImageComponent(
		std::string name,
		UINT64 imagePTR,
		float width,
		float height,
		float* gauge
	) :ImGuiComponenet(name, ImGuiComponentType::IMGUI_IMAGE_COMPONENT),
		mImagePTR(imagePTR),
		mWidth(width),
		mHeight(height),
		mGauge(gauge)
	{}

	void setImage(UINT64 imagePTR)
	{
		mImagePTR = imagePTR;
	}
};

class ImGuiToggleComponent : public ImGuiComponenet
{
public:
	bool* mToggle = nullptr;

public:
	ImGuiToggleComponent(
		std::string name,
		bool& toggle
	) :ImGuiComponenet(name, ImGuiComponentType::IMGUI_TOGGLE_COMPONENT),
		mToggle(&toggle)
	{
		//if (!mToggle)
		//	throw std::runtime_error("");
	}

	ImGuiToggleComponent(
		std::string name,
		std::string hint,
		bool& toggle
	) :ImGuiComponenet(name, hint, ImGuiComponentType::IMGUI_TOGGLE_COMPONENT),
		mToggle(&toggle)
	{
		//if (!mToggle)
		//	throw std::runtime_error("");
	}

	bool getToggle()
	{
		return *mToggle;
	}
};

class ImGuiSliderFloatComponent : public ImGuiComponenet
{
public:
	float* mFloat = nullptr;
	float mMin, mMax;

public:
	ImGuiSliderFloatComponent(
		std::string name,
		float min,
		float max,
		float& mFloat
	) :ImGuiComponenet(name, ImGuiComponentType::IMGUI_SLIDER_FLOAT_COMPONENT),
		mMin(min), mMax(max),
		mFloat(&mFloat)
	{
		//if (!mFloat)
		//	throw std::runtime_error("");
	}

	ImGuiSliderFloatComponent(
		std::string name,
		std::string hint,
		float min,
		float max,
		float& mFloat
	) :ImGuiComponenet(name, hint, ImGuiComponentType::IMGUI_SLIDER_FLOAT_COMPONENT),
		mMin(min), mMax(max),
		mFloat(&mFloat)
	{
		//if (!mFloat)
		//	throw std::runtime_error("");
	}

	float getFloat()
	{
		return *mFloat;
	}
};

class ImGuiSliderFloat3Component : public ImGuiComponenet
{
public:
	float mFloats[3] = {0.0f, 0.0f, 0.0f};
	float mMin, mMax;

public:
	ImGuiSliderFloat3Component(
		std::string name,
		float min,
		float max
	) :ImGuiComponenet(name, ImGuiComponentType::IMGUI_SLIDER_FLOAT_3_COMPONENT),
		mMin(min), mMax(max)
	{}

	ImGuiSliderFloat3Component(
		std::string name,
		std::string hint,
		float min,
		float max
	) :ImGuiComponenet(name, hint, ImGuiComponentType::IMGUI_SLIDER_FLOAT_3_COMPONENT),
		mMin(min), mMax(max)
	{}

	float* getFloat()
	{
		return mFloats;
	}
};

class ImGuiButtonComponent : public ImGuiComponenet
{
public:
	bool* isClicked = nullptr;

public:
	ImGuiButtonComponent(
		std::string name,
		bool& isClicked
	) :ImGuiComponenet(name, ImGuiComponentType::IMGUI_BOTTON_COMPONENT),
		isClicked(&isClicked)
	{
		//if (!isClicked)
		//	throw std::runtime_error("");
	}

	ImGuiButtonComponent(
		std::string name,
		std::string hint,
		bool& isClicked
	) :ImGuiComponenet(name, hint, ImGuiComponentType::IMGUI_BOTTON_COMPONENT),
		isClicked(&isClicked)
	{
		//if (!isClicked)
		//	throw std::runtime_error("");
	}

	bool getIsClicked()
	{
		return *isClicked;
	}
};

class ImGuiColorEdit3Component : public ImGuiComponenet
{
public:
	float mColor[3] = { 0.0f, 0.0f, 0.0f };

public:
	ImGuiColorEdit3Component(
		std::string name
	) :ImGuiComponenet(name, ImGuiComponentType::IMGUI_COLOR_EDIT_3_COMPONENT)
	{}

	ImGuiColorEdit3Component(
		std::string name,
		std::string hint
	) :ImGuiComponenet(name, hint, ImGuiComponentType::IMGUI_COLOR_EDIT_3_COMPONENT)
	{}
};

class ImGuiScrollBarComponent : public ImGuiComponenet
{
public:
};

class ImGuiFrameComponent
{
public:
	std::string mTitle;
	std::string mHint;

	ImGuiWindowFlags window_flags = 0;

	/*
		ImGuiWindowFlags window_flags = 0;mComponents
		window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
		window_flags |= ImGuiWindowFlags_NoBackground;
		window_flags |= ImGuiWindowFlags_NoTitleBar;
	*/

public:
	std::vector<std::unique_ptr<ImGuiComponenet>> mComponents;

public:
	ImGuiFrameComponent (
		std::string name,
		std::string hint = ""
	): mTitle(name), mHint(hint)
	{
		window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
	}

	ImGuiFrameComponent(
		std::string name,
		ImGuiWindowFlags& windowFlags
	) : mTitle(name), mHint("")
	{
		window_flags = windowFlags;
	}

	inline void setWindowFlags(ImGuiWindowFlags& windowFlag)
	{
		this->window_flags = windowFlag;
	}

	inline void pushTextComponent(
		std::string name,
		std::string text = ""
	) {
		mComponents.push_back(std::make_unique<ImGuiTextComponent>(name, mHint, text));
	}
	inline void pushImageComponent(
		std::string name,
		UINT64 texturePTR,
		UINT width,
		UINT height,
		bool newLine = true,
		float* mGauge = nullptr
	) {
		if (!mGauge)
			mComponents.push_back(std::make_unique<ImGuiImageComponent>(name, texturePTR, width, height));
		else
			mComponents.push_back(std::make_unique<ImGuiImageComponent>(name, texturePTR, width, height, mGauge));

		mComponents[mComponents.size() - 1].get()->setNewLine(newLine);
	}
	inline void pushToggleComponent(
		bool& toggle,
		std::string name,
		std::string hint = ""
	) {
		mComponents.push_back(std::make_unique<ImGuiToggleComponent>(name, hint, toggle));
	}
	inline void pushSliderFloatComponent(
		float& sliderFloat,
		std::string name,
		std::string hint = "",
		float min = 0.0f,
		float max = 1.0f
	) {
		mComponents.push_back(std::make_unique<ImGuiSliderFloatComponent>(name, hint, min, max, sliderFloat));
	}
	inline void pushSliderFloat3Component(
		std::string name,
		std::string hint = "",
		float min = 0.0f,
		float max = 1.0f
	) {
		mComponents.push_back(std::make_unique<ImGuiSliderFloat3Component>(name, hint, min, max));
	}
	inline void pushButtonComponent(
		bool& isClicked,
		std::string name,
		std::string hint = ""
	) {
		mComponents.push_back(std::make_unique<ImGuiButtonComponent>(name, hint, isClicked));
	}
	inline void pushColorEdit3Component(
		std::string name,
		std::string hint = ""
	) {
		mComponents.push_back(std::make_unique<ImGuiColorEdit3Component>(name, hint));
	}
};

void InitGUI(
	HWND& mHWND,
	Microsoft::WRL::ComPtr<ID3D12Device>& mDevice,
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& ImguiDescHeap
);

std::shared_ptr<ImGuiFrameComponent> pushFrame(std::string Title, std::string Hint = "");
void DrawGUI(const GameTimer& gt);