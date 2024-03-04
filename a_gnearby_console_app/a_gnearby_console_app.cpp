// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define NOMINMAX
#define GLOG_NO_ABBREVIATED_SEVERITIES

#include <windows.h>
#include <algorithm>
#include <memory>
#include <iostream>

#include "a_win32_dll/Adapter.h"
#include "a_win32_dll/endpointInfo.h"

#include "a_win32_dll/nearby_api.h"

using namespace nearby;
using namespace nearby::windows;

DWORD WINAPI RequestConnectionWork(LPVOID lpParam);
DWORD WINAPI AcceptConnectionWork(LPVOID lpParam);
DWORD WINAPI SendPayloadWork(LPVOID lpParam);

#define MAX_THREADS 5
DWORD   dwThreadIdArray[MAX_THREADS];
HANDLE  hThreadArray[MAX_THREADS] = {};

Core* core = nullptr;
ResultCallbackW connectResultCallback;
ResultCallbackW acceptResultCallback;
ResultCallbackW payloadResultCallback;

constexpr uint8_t kPayload[] = { 0x0f, 0x0a, 0x0c, 0x0e };

const char* nameInEndpointIfo = "Ra";
const char* kServiceId = "NearbySharing";

//const char* local_fast_advertisement_service_uuid = "0000fef3-0000-1000-8000-00805f9b34fb";
const char* local_fast_advertisement_service_uuid = nullptr;

std::string request_connection_endpoint_id;
std::string accept_connection_endpoint_id;

void ListenerEndpointFoundCB(const char* endpoint_id, const char* endpoint_info,
    size_t endpoint_info_size,
    const char* str_service_id)
{
    std::cout << "Found endpoint " << endpoint_id << " on service " << str_service_id << std::endl;

    request_connection_endpoint_id = endpoint_id;

    hThreadArray[0] = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        RequestConnectionWork,       // thread function name
        NULL,          // argument to thread function 
        0,                      // use default creation flags 
        &dwThreadIdArray[0]);   // returns the thread identifier 
}

void ListenerEndpointLostCB(const char* endpoint_id)
{
    std::cout << "Device lost: " << endpoint_id << std::endl;
}

void ListenerEndpointDistanceChangedCB(const char* endpoint_id,
    DistanceInfoW distance_info)
{
    std::cout << "Device distance changed: " << endpoint_id << std::endl;
}

void ResultCBMee(Status status)
{
    (void)status;  // Avoid unused parameter warning
    std::cout << "Status: " << std::endl;
}

void ListenerInitiatedCB(
    const char* endpoint_id,
    const ConnectionResponseInfoW& connection_response_info)
{
    std::cout << "Advertising initiated: " << endpoint_id << std::endl;
    accept_connection_endpoint_id = endpoint_id;

    hThreadArray[1] = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        AcceptConnectionWork,       // thread function name
        NULL,          // argument to thread function 
        0,                      // use default creation flags 
        &dwThreadIdArray[1]);   // returns the thread identifier 
}

void ListenerAcceptedCB(const char* endpoint_id)
{
    std::cout << "Advertising accepted: " << endpoint_id << std::endl;

    //hThreadArray[2] = CreateThread(
    //    NULL,                   // default security attributes
    //    0,                      // use default stack size  
    //    SendPayloadWork,       // thread function name
    //    NULL,          // argument to thread function 
    //    0,                      // use default creation flags 
    //    &dwThreadIdArray[2]);   // returns the thread identifier 
}

void ListenerRejectedCB(const char* endpoint_id, connections::Status status)
{
    std::cout << "Advertising rejected: " << endpoint_id << std::endl;
}

void ListenerDisconnectedCB(const char* endpoint_id)
{
    std::cout << "Advertising disconnected: " << endpoint_id << std::endl;
}

void ListenerBandwidthChangedCB(const char* endpoint_id, MediumW medium)
{
    std::cout << "Advertising bandwidth changed: " << endpoint_id << std::endl;
}

void ListenerPayloadCB(const char* endpoint_id, PayloadW& payload)
{
    std::cout << "Payload callback called. id: " << endpoint_id << "payload_id: " << payload.GetId() << "type: " << (int)payload.GetType() << "offset : " << payload.GetOffset() << std::endl;

    switch (payload.GetType())
    {
    case nearby::connections::PayloadType::kBytes:
    {
        const char* bytes = nullptr;
        size_t bytes_size;

        if (!payload.AsBytes(bytes, bytes_size))
        {
            std::cout << "Failed to get the payload as bytes." << std::endl;
            return;
        }
        else
        {
            std::cout << bytes << std::endl;
        }

        return;
    }
    case nearby::connections::PayloadType::kStream: {
        return;
    }
    case nearby::connections::PayloadType::kFile: {
        std::string path = payload.GetFileName();
        std::cout << path << std::endl;
        return;
    }
    default:
        std::cout << "Invalid payload type." << std::endl;
        return;
    }
}

void ListenerPayloadProgressCB(
    const char* endpoint_id,
    const PayloadProgressInfoW& payload_progress_info)
{
    std::cout << "Payload progress callback called. id: " << endpoint_id << "payload_id: " << payload_progress_info.payload_id
        << "bytes transferred: " << payload_progress_info.bytes_transferred << "total: " << payload_progress_info.total_bytes
        << "status: " << (int)payload_progress_info.status << std::endl;
}

DWORD WINAPI AcceptConnectionWork(LPVOID lpParam)
{
    PayloadListenerW listener(ListenerPayloadCB, ListenerPayloadProgressCB);

    ResultCallbackW acceptResultCallbacktmp;
    acceptResultCallbacktmp.result_cb = ResultCBMee;

    AcceptConnection(core, accept_connection_endpoint_id.c_str(), listener, acceptResultCallback);

    std::cout << "accepting part is done by Rajendra" << std::endl;

    while (true)
    {
        Sleep(10);
    }

    return 0;
}

DWORD WINAPI SendPayloadWork(LPVOID lpParam)
{
    std::cout << "sending payload Rajendra" << std::endl;

    std::vector<std::string> endpoint_ids = { std::string(accept_connection_endpoint_id) };

    const std::vector<uint8_t> expected_payload(std::begin(kPayload),
        std::end(kPayload));

    std::string payloadStr = std::string(expected_payload.begin(), expected_payload.end());

    PayloadW payload(payloadStr.c_str(), payloadStr.size());

    std::vector<const char*> c_string_array;

    std::transform(endpoint_ids.begin(), endpoint_ids.end(),
        std::back_inserter(c_string_array),
        [](const std::string& s) {
            char* pc = new char[s.size() + 1];
            strncpy(pc, s.c_str(), s.size() + 1);
            return pc;
        });

    payloadResultCallback.result_cb = ResultCBMee;

    SendPayload(core, c_string_array.data(), c_string_array.size(),
        std::move(payload), payloadResultCallback);

    while (true)
    {
        Sleep(10);
    }

    return 0;
}

DWORD WINAPI RequestConnectionWork(LPVOID lpParam)
{
    ConnectionOptionsW connection_options;
    connection_options.allowed.ble = true;
    connection_options.allowed.bluetooth = true;
    connection_options.allowed.web_rtc = false;
    connection_options.allowed.wifi_lan = false;
    connection_options.allowed.wifi_direct = false;
    //connection_options.remote_bluetooth_mac_address = "ac:5f:ea:38:eb:e9";
    connection_options.remote_bluetooth_mac_address = nullptr;
    connection_options.enforce_topology_constraints = true;
    connection_options.auto_upgrade_bandwidth = false;
    connection_options.fast_advertisement_service_uuid = local_fast_advertisement_service_uuid;

    ConnectionListenerW clistener(ListenerInitiatedCB, ListenerAcceptedCB,
        ListenerRejectedCB, ListenerDisconnectedCB,
        ListenerBandwidthChangedCB);

    auto endpointInfo = CreateEndpointInfo(nameInEndpointIfo, nearby::endpoint::ShareTargetType::kLaptop);
    std::string infoo = std::string(endpointInfo->begin(), endpointInfo->end());
    ConnectionRequestInfoW info{ infoo.c_str(), infoo.length(), clistener };;

    ResultCallbackW connectResultCallback2;
    connectResultCallback.result_cb = ResultCBMee;

    RequestConnection(core, request_connection_endpoint_id.c_str(), info, connection_options, connectResultCallback2);

    while (true)
    {
        Sleep(10);
    }

    return 0;
}

int newMain()
{
    NearbyShareAPI* nearby = new NearbyShareAPI();
    nearby->InitializeNearby();
    nearby->StartScanning();

    while (true) {
        WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);
        Sleep(10);
    }

    return 0;
}

int main()
{
    return newMain();

    auto router = InitServiceControllerRouter();
    core = InitCore(router);

    AdvertisingOptionsW a_options;
    a_options.strategy = StrategyW::kP2pPointToPoint;
    a_options.device_info = "connectionsd";;
    a_options.allowed.ble = true;
    a_options.allowed.bluetooth = true;
    a_options.allowed.wifi_lan = false;
    a_options.allowed.wifi_direct = false;
    a_options.allowed.wifi_hotspot = false;
    a_options.allowed.web_rtc = false;
    a_options.low_power = false;
    a_options.auto_upgrade_bandwidth = false;
    a_options.enforce_topology_constraints = true;
    a_options.enable_bluetooth_listening = true;


    a_options.fast_advertisement_service_uuid = local_fast_advertisement_service_uuid;

    ConnectionListenerW clistener(ListenerInitiatedCB, ListenerAcceptedCB,
        ListenerRejectedCB, ListenerDisconnectedCB,
        ListenerBandwidthChangedCB);

    auto endpointInfo = nearby::endpoint::CreateEndpointInfo(nameInEndpointIfo, nearby::endpoint::ShareTargetType::kLaptop);
    std::string infoo = std::string(endpointInfo->begin(), endpointInfo->end());
    ConnectionRequestInfoW info{ infoo.c_str(), infoo.length(), clistener };;


    //{infoo, strlen(infoo), clistener };

    ResultCallbackW callback2;
    callback2.result_cb = ResultCBMee;

    StartAdvertising(core, kServiceId, a_options, info, callback2);

    DiscoveryOptionsW d_options;
    d_options.strategy = StrategyW::kP2pPointToPoint;
    d_options.allowed.ble = true;
    d_options.allowed.bluetooth = true;
    d_options.allowed.wifi_lan = false;
    d_options.allowed.wifi_direct = false;
    d_options.allowed.wifi_hotspot = false;
    d_options.allowed.web_rtc = false;
    d_options.fast_advertisement_service_uuid = "0000fef3-0000-1000-8000-00805f9b34fb";
    d_options.is_out_of_band_connection = false;

    DiscoveryListenerW dlistener(ListenerEndpointFoundCB, ListenerEndpointLostCB,
        ListenerEndpointDistanceChangedCB);

    ResultCallbackW callback;
    callback.result_cb = ResultCBMee;

    StartDiscovery(core, kServiceId, d_options, dlistener, callback);

    std::cout << "=====================================================================================================" << std::endl;
    std::cout << "=====================================================================================================" << std::endl;
    std::cout << "=====================================================================================================" << std::endl;

    while (true) {
        WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);
        Sleep(10);
    }


    return 0;
}

