#include "GUI.h"

std::vector<std::shared_ptr<ImGuiFrameComponent>> ImGuiFrameComponents;

void InitGUI(
	HWND& mHWND,
	Microsoft::WRL::ComPtr<ID3D12Device>& mDevice,
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& ImguiDescHeap
)
{
	D3D12_DESCRIPTOR_HEAP_DESC imguiDescHeapDesc = {};
	imguiDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	imguiDescHeapDesc.NumDescriptors = 10;
	imguiDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	mDevice->CreateDescriptorHeap(
		&imguiDescHeapDesc,
		IID_PPV_ARGS(ImguiDescHeap.GetAddressOf())
	);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(mHWND);
	ImGui_ImplDX12_Init(
		mDevice.Get(),
		3,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		ImguiDescHeap.Get(),
		ImguiDescHeap->GetCPUDescriptorHandleForHeapStart(),
		ImguiDescHeap->GetGPUDescriptorHandleForHeapStart()
	);
	ImGui::StyleColorsDark();

	io.Fonts->AddFontFromFileTTF("C:\\windows\\Fonts\\malgun.ttf", 18.0f, NULL);

}

void DrawGUI(const GameTimer& gt)
{
	ImGuiTextComponent* textComponent					= nullptr;
	ImGuiImageComponent* imageComponent					= nullptr;
	ImGuiToggleComponent* toggleComponent				= nullptr;
	ImGuiSliderFloatComponent* sliderFloatComponent		= nullptr;
	ImGuiSliderFloat3Component* sliderFloat3Component	= nullptr;
	ImGuiButtonComponent* buttonComponent				= nullptr;
	ImGuiColorEdit3Component* colorEdit3Component		= nullptr;

	// Initialization Deer ImGui
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	int* mFence = nullptr;
	bool condition = false;

	for (std::shared_ptr<ImGuiFrameComponent>& frame : ImGuiFrameComponents)
	{
		ImGui::Begin(frame->mTitle.c_str(), 0, frame->window_flags);

		std::vector<std::unique_ptr<ImGuiComponenet>>::iterator iter;
		std::vector<std::unique_ptr<ImGuiComponenet>>::iterator end = frame->mComponents.end();

		ImGuiComponenet* item;
		for (iter = frame->mComponents.begin(); iter != end; iter++)
		{
			item = (*iter).get();

			if (!item->isFence) {
				switch (item->mType) {
				case ImGuiComponentType::IMGUI_TEXT_COMPONENT:
					textComponent = (ImGuiTextComponent*)item;
					ImGui::Text(textComponent->getText());
					break;
				case ImGuiComponentType::IMGUI_IMAGE_COMPONENT:
					imageComponent = (ImGuiImageComponent*)item;
					if (!imageComponent->mGauge)
					{
						ImGui::Image((void**)imageComponent->mImagePTR, ImVec2(imageComponent->mWidth, imageComponent->mHeight));
					}
					// if the Image width or height value is hung to mGauge variable.
					else
					{
						float percentage = *imageComponent->mGauge;
						ImGui::Image((void**)imageComponent->mImagePTR, ImVec2(imageComponent->mWidth * percentage, imageComponent->mHeight), ImVec2(0, 0), ImVec2(percentage, 1));
					}
					break;
				case ImGuiComponentType::IMGUI_TOGGLE_COMPONENT:
					toggleComponent = (ImGuiToggleComponent*)item;
					ImGui::Checkbox(
						toggleComponent->getHint(),
						toggleComponent->mToggle
					);
					break;
				case ImGuiComponentType::IMGUI_SLIDER_FLOAT_COMPONENT:
					sliderFloatComponent = (ImGuiSliderFloatComponent*)item;
					ImGui::SliderFloat(
						sliderFloatComponent->getHint(),
						sliderFloatComponent->mFloat,
						sliderFloatComponent->mMin,
						sliderFloatComponent->mMax,
						"%.5%f"
					);
					break;
				case ImGuiComponentType::IMGUI_SLIDER_FLOAT_3_COMPONENT:
					sliderFloat3Component = (ImGuiSliderFloat3Component*)item;
					ImGui::SliderFloat3(
						sliderFloat3Component->getHint(),
						sliderFloat3Component->mFloats,
						sliderFloat3Component->mMin,
						sliderFloat3Component->mMax,
						"%.5%f"
					);
					break;
				case ImGuiComponentType::IMGUI_BOTTON_COMPONENT:
					buttonComponent = (ImGuiButtonComponent*)item;
					*buttonComponent->isClicked = ImGui::Button(buttonComponent->getHint());
					break;
				case ImGuiComponentType::IMGUI_COLOR_EDIT_3_COMPONENT:
					colorEdit3Component = (ImGuiColorEdit3Component*)item;
					ImGui::ColorEdit3("clear color", colorEdit3Component->mColor);
					break;
				case ImGuiComponentType::IMGUI_SCROLL_BAR_COMPONENT:
					break;
				}
			}
			else {
				switch (item->mType) {
				case ImGuiComponentType::IMGUI_TEXT_COMPONENT:
					textComponent = (ImGuiTextComponent*)item;
					ImGui::Text(textComponent->getText());
					break;
				case ImGuiComponentType::IMGUI_TOGGLE_COMPONENT:
					toggleComponent = (ImGuiToggleComponent*)item;
					condition = ImGui::Checkbox(
						toggleComponent->getHint(),
						toggleComponent->mToggle
					);
					break;
				case ImGuiComponentType::IMGUI_SLIDER_FLOAT_COMPONENT:
					sliderFloatComponent = (ImGuiSliderFloatComponent*)item;
					condition = ImGui::SliderFloat(
						sliderFloatComponent->getHint(),
						sliderFloatComponent->mFloat,
						sliderFloatComponent->mMin,
						sliderFloatComponent->mMax
					);
					break;
				case ImGuiComponentType::IMGUI_SLIDER_FLOAT_3_COMPONENT:
					sliderFloat3Component = (ImGuiSliderFloat3Component*)item;
					condition = ImGui::SliderFloat3(
						sliderFloat3Component->getHint(),
						sliderFloat3Component->mFloats,
						sliderFloat3Component->mMin,
						sliderFloat3Component->mMax
					);
					break;
				case ImGuiComponentType::IMGUI_BOTTON_COMPONENT:
					buttonComponent = (ImGuiButtonComponent*)item;
					*buttonComponent->isClicked = ImGui::Button(buttonComponent->getHint());
					break;
				case ImGuiComponentType::IMGUI_COLOR_EDIT_3_COMPONENT:
					colorEdit3Component = (ImGuiColorEdit3Component*)item;
					condition = ImGui::ColorEdit3("clear color", colorEdit3Component->mColor);
					break;
				case ImGuiComponentType::IMGUI_SCROLL_BAR_COMPONENT:
					break;
				}

				if (condition)
					*item->mFence = 1;
			}
			
			if (!item->getNewLine())
				ImGui::SameLine();
		}

		ImGui::End();
	}

	ImGui::Render();
}

// push new frame info
std::shared_ptr<ImGuiFrameComponent> pushFrame(std::string Title, std::string Hint)
{
	ImGuiFrameComponents.push_back(std::make_shared<ImGuiFrameComponent>(ImGuiFrameComponent(Title, Hint)));

	return ImGuiFrameComponents[ImGuiFrameComponents.size() - 1];
}