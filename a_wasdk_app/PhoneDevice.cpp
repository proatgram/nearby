// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PhoneDevice.h"
#include "PhoneDevice.g.cpp"
#include <sstream>

using namespace std;
namespace winrt
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Media::Imaging;
    using namespace Windows::Foundation;
    using namespace Windows::Storage;
    using namespace Windows::Storage::Streams;
}

namespace winrt::a_wasdk_app::implementation
{
    void PhoneDevice::DeviceName(hstring const& value)
    {
        if (m_deviceName != value)
        {
            m_deviceName = value;
            RaisePropertyChanged(L"DeviceName");
        }
    }

    void PhoneDevice::TransferProgress(int progress)
    {
        m_TransferProgress = progress;
        RaisePropertyChanged(L"TransferProgress");
    }
}
