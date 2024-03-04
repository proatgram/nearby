#pragma once

#include "MainWindow.g.h"


namespace winrt::a_wasdk_app::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        fire_and_forget myButton_Click(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

        Windows::Foundation::IAsyncOperation<Windows::Storage::StorageFile> BrowseFileAsync();
    };
}

namespace winrt::a_wasdk_app::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
