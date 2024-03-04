#pragma once

#include "DevicesPage.g.h"

namespace winrt::a_wasdk_app::implementation
{
    struct DevicesPage : DevicesPageT<DevicesPage>
    {
        DevicesPage();

        Windows::Foundation::Collections::IVector<Windows::Foundation::IInspectable> Devices() const
        {
            return m_devices;
        }

        int TransferProgress() const
        {
            return m_TransferProgress;
        }

        void TransferProgress(int progress);

        bool ShowProgress()
        {
            return m_showProgress;
        }

        bool ShowDevicesSection()
        {
            return m_ShowDevicesSection;
        }

        bool ShowTransferComplete()
        {
            return m_ShowTransferComplete;
        }

        bool ShowAuthSection()
        {
            return m_showAuthSection;
        }

        bool EnableDeviceScan();

        bool ShowPinSpinner()
        {
            return m_showPinSpinner;
        }

        bool TransferStarted()
        {
            return m_transferStarted;
        }

        void TransferStarted(bool value)
        {
            m_transferStarted = value;
        }

        hstring FileName()
        {
            return m_fileName;
        }

        hstring AuthToken()
        {
            return m_authToken;
        }

        void OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs);

        fire_and_forget PopulateDevice(hstring device_name, hstring endpoint_id);
        fire_and_forget UpdateProgress(int progress);
        void OnTransferComplete();
        void OnTransferStarted();
        void UpdateAuthToken(std::string);
        void UpdateFileToShare(winrt::Windows::Storage::StorageFile file);

        void StartWatcher();

        // Event handler.
        void ImageGridView_ItemClick(Windows::Foundation::IInspectable const, Microsoft::UI::Xaml::Controls::ItemClickEventArgs const);

        // Property changed notifications.
        event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&);
        void PropertyChanged(event_token const&);

        fire_and_forget OpenFileButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void ScanButton_Click(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

        // Property changed notifications.
        void RaisePropertyChanged(hstring const&);
        event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;

        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

        Windows::Foundation::Collections::IVector<IInspectable> m_devices{ nullptr };

        std::wstring m_filePath;

        hstring m_fileName;
        hstring m_authToken;
        int m_TransferProgress{ 0 };
        bool m_showProgress{ false };
        bool m_ShowDevicesSection{ true };
        bool m_ShowTransferComplete{ false };
        bool m_showAuthSection{ false };
        bool m_transferStarted{ false };
        bool m_showPinSpinner{ false };
    };
}

namespace winrt::a_wasdk_app::factory_implementation
{
    struct DevicesPage : DevicesPageT<DevicesPage, implementation::DevicesPage>
    {
    };
}
