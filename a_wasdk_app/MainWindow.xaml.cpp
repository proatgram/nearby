#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <microsoft.ui.xaml.window.h>
#include <Shobjidl.h>

#include "winrt/Windows.Storage.Pickers.h"
#include "winrt/Windows.Storage.Pickers.Provider.h"

#include "a_win32_dll/nearby_api.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage;

using namespace nearby::windows;

#define MAX_THREADS 5
DWORD   dwThreadIdArray[MAX_THREADS];
HANDLE  hThreadArray[MAX_THREADS] = {};

extern winrt::a_wasdk_app::implementation::MainWindow* g_mainWindow;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

void DeviceAdded(std::string device_name, std::string endpoint_id)
{
	OutputDebugStringA(device_name.c_str());
	//OutputDebugStringA(endpoint_id.c_str());
}

namespace winrt::a_wasdk_app::implementation
{
    MainWindow::MainWindow()
    {
        g_mainWindow = this;

        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
    }

    int32_t MainWindow::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainWindow::MyProperty(int32_t /* value */)
    {
		throw hresult_not_implemented();
	}

	void StartWatcher()
	{
		auto work = []() {
			NearbyShareAPI* nearby = new NearbyShareAPI();
			nearby->InitializeNearby();
			nearby->StartScanning2(DeviceAdded);

			while (true) {
				WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);
				Sleep(10);
			}
			};
		std::thread([work]() {
			work();
			}).detach();
	}

    Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile> MainWindow::BrowseFileAsync()
    {
        auto lifetime = get_strong();


        FileOpenPicker openPicker;
        auto windowNative{ this->m_inner.as<::IWindowNative>() };
        HWND hWnd{ 0 };
        windowNative->get_WindowHandle(&hWnd);

        openPicker.as<::IInitializeWithWindow>()->Initialize(hWnd);

        openPicker.ViewMode(PickerViewMode::Thumbnail);
        openPicker.SuggestedStartLocation(PickerLocationId::PicturesLibrary);
        //openPicker.FileTypeFilter().ReplaceAll({ L".jpg", L".jpeg", L".png" });
        openPicker.FileTypeFilter().ReplaceAll({ L"*"});
        //openPicker.FileTypeFilter.Add("*");
        StorageFile file = co_await openPicker.PickSingleFileAsync();
        if (file != nullptr)
        {
            co_return file;
        }
        else
        {
            co_return  nullptr;
        }
    }

    fire_and_forget MainWindow::myButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        myButton().Content(box_value(L"Clicked"));

		//StartWatcher();
        co_return;
    }
}
