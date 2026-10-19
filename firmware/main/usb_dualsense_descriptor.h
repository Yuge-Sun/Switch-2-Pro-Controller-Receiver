#pragma once

#include "ps5_profile.h"

#include "tusb.h"

#define DUALSENSE_USB_AUDIO_EP_OUT 0x01
#define DUALSENSE_USB_AUDIO_EP_IN 0x82
#define DUALSENSE_USB_HID_EP_OUT 0x03
#define DUALSENSE_USB_HID_EP_IN 0x84
#define DUALSENSE_USB_AUDIO_SAMPLE_RATE 48000
#define DUALSENSE_USB_AUDIO_BYTES_PER_SAMPLE 2
#define DUALSENSE_USB_AUDIO_BITS_PER_SAMPLE 16
#define DUALSENSE_USB_UAC1_4CH_PACKET_SIZE 384
#define DUALSENSE_USB_UAC1_MIC_PACKET_SIZE 192
#define DUALSENSE_USB_UAC1_PACKET_SIZE DUALSENSE_USB_UAC1_4CH_PACKET_SIZE
#define DUALSENSE_USB_UAC1_DS5_AC_LEN 82
#define DUALSENSE_USB_UAC1_DS5_AS_OUT_LEN 52
#define DUALSENSE_USB_UAC1_DS5_AS_IN_LEN 52
#define DUALSENSE_USB_UAC1_DS5_DESC_LEN \
    (DUALSENSE_USB_UAC1_DS5_AC_LEN + \
     DUALSENSE_USB_UAC1_DS5_AS_OUT_LEN + \
     DUALSENSE_USB_UAC1_DS5_AS_IN_LEN)
#define DUALSENSE_USB_UAC1_DESC_LEN DUALSENSE_USB_UAC1_DS5_DESC_LEN

const tusb_desc_device_t *dualsense_usb_device_descriptor(void);
const uint8_t *dualsense_usb_configuration_descriptor(void);
const char **dualsense_usb_string_descriptors(void);
int dualsense_usb_string_descriptor_count(void);
