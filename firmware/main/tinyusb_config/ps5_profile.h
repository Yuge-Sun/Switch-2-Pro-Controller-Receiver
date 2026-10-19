#pragma once

// Fixed standard DualSense identity and four-channel UAC1 audio topology.
// Preserve the existing serial so Windows keeps the receiver's device identity.
#define DS5_PROFILE_NAME "hid_audio_uac1_4ch_dualsense"
#define DS5_PROFILE_SERIAL "V5915PS5STD"
#define DS5_USB_PID 0x0ce6
#define DS5_USB_PRODUCT "DualSense Wireless Controller"
#define DS5_AUDIO_CHANNELS 4
#define DS5_HID_INTERFACE_NUMBER 3
#define DS5_INTERFACE_COUNT 4
