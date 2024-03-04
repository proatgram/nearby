#pragma once

#include "Class.g.h"

namespace winrt::a_wasdk_dll::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
    };
}

namespace winrt::a_wasdk_dll::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
