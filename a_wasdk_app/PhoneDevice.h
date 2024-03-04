// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PhoneDevice.g.h"

namespace winrt::a_wasdk_app::implementation
{
    struct PhoneDevice : PhoneDeviceT<PhoneDevice>
    {
        PhoneDevice() = default;

        PhoneDevice(hstring const& name, hstring const& endpoint_id)
            : m_deviceName(name),
            m_endpointId(endpoint_id),
            m_TransferProgress(0)
        {
            
        }

        hstring DeviceName() const
        {
            return m_deviceName;
        }

        hstring EndpointId() const
        {
            return m_endpointId;
        }

        int TransferProgress() const
        {
            return m_TransferProgress;
        }

        void TransferProgress(int progress);

        void DeviceName(hstring const& value);

        // Property changed notifications.
        event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& value)
        {
            return m_propertyChanged.add(value);
        }

        void PropertyChanged(event_token const& token)
        {
            m_propertyChanged.remove(token);
        }

    private:
        // File and information fields.
        hstring m_deviceName;
        hstring m_endpointId;

        int m_TransferProgress{ 0 };

        // Property changed notification.
        event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;

        void RaisePropertyChanged(hstring const& propertyName)
        {
            m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
        }
    };
}

namespace winrt::a_wasdk_app::factory_implementation
{
    struct PhoneDevice : PhoneDeviceT<PhoneDevice, implementation::PhoneDevice>
    {
    };
}