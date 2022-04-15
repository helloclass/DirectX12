#pragma once
#include "BoxApp.h"

static BoxApp* theApp = nullptr;

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		theApp = new BoxApp(hInstance);

		if (!theApp->Initialize())
			return 0;

		// Finally Bind a Resource on Descriptor (like Textures, Material)
		theApp->_Start();

		// The Main CommandList will be Closed and Ready Binded on Command Q 
		theApp->CloseCommandList();

		theApp->Run();

		theApp->_Exit();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 1;
	}

	theApp->~BoxApp();
	delete(theApp);

	return 0;
}

