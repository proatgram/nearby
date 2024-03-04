#pragma once

enum MediumW : int {
	UNKNOWN_MEDIUM = 0,
	MDNS = 1,
	BLUETOOTH = 2,
	WIFI_HOTSPOT = 3,
	BLE = 4,
	WIFI_LAN = 5,
	WIFI_AWARE = 6,
	NFC = 7,
	WIFI_DIRECT = 8,
	WEB_RTC = 9,
	BLE_L2CAP = 10,
	USB = 11
};

bool Medium_IsValid(int value) {
    switch (value) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
        return true;
    default:
        return false;
    }
}

constexpr MediumW MediumW_MIN = UNKNOWN_MEDIUM;
constexpr MediumW MediumW_MAX = USB;
constexpr int MediumW_ARRAYSIZE = MediumW_MAX + 1;