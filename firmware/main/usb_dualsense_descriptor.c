#include "ps5_profile.h"

#include "usb_dualsense_descriptor.h"

#include "class/audio/audio.h"
#include "dualsense_report.h"

#define DS5_USB_VID 0x054c

#define DS5_AUDIO_CONTROL_INTERFACE 0
#define DS5_AUDIO_STREAMING_INTERFACE 1
#define DS5_AUDIO_INPUT_STREAMING_INTERFACE 2
#define DS5_HID_INTERFACE DS5_HID_INTERFACE_NUMBER

#define DS5_HID_EP_OUT DUALSENSE_USB_HID_EP_OUT
#define DS5_HID_EP_IN DUALSENSE_USB_HID_EP_IN
#define DS5_HID_EP_SIZE 64
// A real DualSense is high-speed USB and advertises bInterval=6, which is
// effectively a 4 ms HID period. ESP32-S3 is full-speed, where bInterval is
// expressed directly in frames.
#define DS5_HID_POLL_MS 4

#define DS5_AUDIO_EP_OUT DUALSENSE_USB_AUDIO_EP_OUT
#define DS5_AUDIO_SAMPLE_RATE DUALSENSE_USB_AUDIO_SAMPLE_RATE
#define DS5_AUDIO_BYTES_PER_SAMPLE DUALSENSE_USB_AUDIO_BYTES_PER_SAMPLE
#define DS5_AUDIO_BITS_PER_SAMPLE DUALSENSE_USB_AUDIO_BITS_PER_SAMPLE
#define DS5_AUDIO_EP_OUT_SIZE \
    TUD_AUDIO_EP_SIZE(DS5_AUDIO_SAMPLE_RATE, \
                      DS5_AUDIO_BYTES_PER_SAMPLE, \
                      DS5_AUDIO_CHANNELS)
#define DS5_AUDIO_RENDER_DESC_LEN DUALSENSE_USB_UAC1_DESC_LEN

#define DS5_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + DS5_AUDIO_RENDER_DESC_LEN + 32)

#define DS5_UAC1_AUDIO_RENDER_DESCRIPTOR(_stridx, _epout, _epsize) \
    0x09, TUSB_DESC_INTERFACE, \
    DS5_AUDIO_CONTROL_INTERFACE, 0x00, 0x00, \
    TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_INT_PROTOCOL_CODE_UNDEF, 0x00, \
    0x0a, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_HEADER, \
    U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(0x0049), \
    0x02, DS5_AUDIO_STREAMING_INTERFACE, DS5_AUDIO_INPUT_STREAMING_INTERFACE, \
    0x0c, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL, \
    0x01, U16_TO_U8S_LE(AUDIO_TERM_TYPE_USB_STREAMING), \
    0x06, 0x04, U16_TO_U8S_LE(0x0033), 0x00, 0x00, \
    0x0c, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, \
    0x02, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x09, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL, \
    0x03, U16_TO_U8S_LE(AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER), \
    0x04, 0x02, 0x00, \
    0x0c, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL, \
    0x04, U16_TO_U8S_LE(0x0402), \
    0x03, 0x02, U16_TO_U8S_LE(0x0003), 0x00, 0x00, \
    0x09, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, \
    0x05, 0x04, 0x01, 0x03, 0x00, 0x00, \
    0x09, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL, \
    0x06, U16_TO_U8S_LE(AUDIO_TERM_TYPE_USB_STREAMING), \
    0x01, 0x05, 0x00, \
    0x09, TUSB_DESC_INTERFACE, \
    DS5_AUDIO_STREAMING_INTERFACE, 0x00, 0x00, \
    TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, AUDIO_INT_PROTOCOL_CODE_UNDEF, 0x00, \
    0x09, TUSB_DESC_INTERFACE, \
    DS5_AUDIO_STREAMING_INTERFACE, 0x01, 0x01, \
    TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, AUDIO_INT_PROTOCOL_CODE_UNDEF, 0x00, \
    0x07, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_AS_GENERAL, \
    0x01, 0x01, U16_TO_U8S_LE(0x0001), \
    0x0b, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_FORMAT_TYPE, \
    AUDIO_FORMAT_TYPE_I, 0x04, 0x02, 0x10, 0x01, 0x80, 0xbb, 0x00, \
    0x09, TUSB_DESC_ENDPOINT, _epout, \
    0x09, U16_TO_U8S_LE(_epsize), 0x01, 0x00, 0x00, \
    0x07, TUSB_DESC_CS_ENDPOINT, AUDIO_CS_EP_SUBTYPE_GENERAL, \
    0x00, 0x00, U16_TO_U8S_LE(0x0000), \
    0x09, TUSB_DESC_INTERFACE, \
    DS5_AUDIO_INPUT_STREAMING_INTERFACE, 0x00, 0x00, \
    TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, AUDIO_INT_PROTOCOL_CODE_UNDEF, 0x00, \
    0x09, TUSB_DESC_INTERFACE, \
    DS5_AUDIO_INPUT_STREAMING_INTERFACE, 0x01, 0x01, \
    TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, AUDIO_INT_PROTOCOL_CODE_UNDEF, 0x00, \
    0x07, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_AS_GENERAL, \
    0x06, 0x01, U16_TO_U8S_LE(0x0001), \
    0x0b, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_FORMAT_TYPE, \
    AUDIO_FORMAT_TYPE_I, 0x02, 0x02, 0x10, 0x01, 0x80, 0xbb, 0x00, \
    0x09, TUSB_DESC_ENDPOINT, DUALSENSE_USB_AUDIO_EP_IN, \
    0x05, U16_TO_U8S_LE(DUALSENSE_USB_UAC1_MIC_PACKET_SIZE), \
    0x01, 0x00, 0x00, \
    0x07, TUSB_DESC_CS_ENDPOINT, AUDIO_CS_EP_SUBTYPE_GENERAL, \
    0x00, 0x00, U16_TO_U8S_LE(0x0000)

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_AUDIO_INTERFACE,
    STRID_HID_INTERFACE,
};

static const uint8_t s_ds5_hid_report_descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x01, 0x09, 0x30, 0x09, 0x31,
    0x09, 0x32, 0x09, 0x35, 0x09, 0x33, 0x09, 0x34, 0x15, 0x00, 0x26, 0xff,
    0x00, 0x75, 0x08, 0x95, 0x06, 0x81, 0x02, 0x06, 0x00, 0xff, 0x09, 0x20,
    0x95, 0x01, 0x81, 0x02, 0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07,
    0x35, 0x00, 0x46, 0x3b, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81,
    0x42, 0x65, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0f, 0x15, 0x00, 0x25,
    0x01, 0x75, 0x01, 0x95, 0x0f, 0x81, 0x02, 0x06, 0x00, 0xff, 0x09, 0x21,
    0x95, 0x0d, 0x81, 0x02, 0x06, 0x00, 0xff, 0x09, 0x22, 0x15, 0x00, 0x26,
    0xff, 0x00, 0x75, 0x08, 0x95, 0x34, 0x81, 0x02, 0x85, 0x02, 0x09, 0x23,
    0x95, 0x2f, 0x91, 0x02, 0x85, 0x05, 0x09, 0x33, 0x95, 0x28, 0xb1, 0x02,
    0x85, 0x08, 0x09, 0x34, 0x95, 0x2f, 0xb1, 0x02, 0x85, 0x09, 0x09, 0x24,
    0x95, 0x13, 0xb1, 0x02, 0x85, 0x0a, 0x09, 0x25, 0x95, 0x1a, 0xb1, 0x02,
    0x85, 0x20, 0x09, 0x26, 0x95, 0x3f, 0xb1, 0x02, 0x85, 0x21, 0x09, 0x27,
    0x95, 0x04, 0xb1, 0x02, 0x85, 0x22, 0x09, 0x40, 0x95, 0x3f, 0xb1, 0x02,
    0x85, 0x80, 0x09, 0x28, 0x95, 0x3f, 0xb1, 0x02, 0x85, 0x81, 0x09, 0x29,
    0x95, 0x3f, 0xb1, 0x02, 0x85, 0x82, 0x09, 0x2a, 0x95, 0x09, 0xb1, 0x02,
    0x85, 0x83, 0x09, 0x2b, 0x95, 0x3f, 0xb1, 0x02, 0x85, 0x84, 0x09, 0x2c,
    0x95, 0x3f, 0xb1, 0x02, 0x85, 0x85, 0x09, 0x2d, 0x95, 0x02, 0xb1, 0x02,
    0x85, 0xa0, 0x09, 0x2e, 0x95, 0x01, 0xb1, 0x02, 0x85, 0xe0, 0x09, 0x2f,
    0x95, 0x3f, 0xb1, 0x02, 0x85, 0xf0, 0x09, 0x30, 0x95, 0x3f, 0xb1, 0x02,
    0x85, 0xf1, 0x09, 0x31, 0x95, 0x3f, 0xb1, 0x02, 0x85, 0xf2, 0x09, 0x32,
    0x95, 0x0f, 0xb1, 0x02, 0x85, 0xf4, 0x09, 0x35, 0x95, 0x3f, 0xb1, 0x02,
    0x85, 0xf5, 0x09, 0x36, 0x95, 0x03, 0xb1, 0x02, 0xc0,
};

_Static_assert(sizeof(s_ds5_hid_report_descriptor) == 0x0111,
               "DualSense HID report descriptor length mismatch");

static const tusb_desc_device_t s_ds5_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = DS5_USB_VID,
    .idProduct = DS5_USB_PID,
    .bcdDevice = 0x0103,
    .iManufacturer = STRID_MANUFACTURER,
    .iProduct = STRID_PRODUCT,
    .iSerialNumber = 0x00,
    .bNumConfigurations = 0x01,
};

static const uint8_t s_ds5_configuration_descriptor[] = {
    0x09, TUSB_DESC_CONFIGURATION,
    U16_TO_U8S_LE(DS5_CONFIG_TOTAL_LEN),
    DS5_INTERFACE_COUNT, 0x01, 0x00,
    0xc0, 0xfa,

    DS5_UAC1_AUDIO_RENDER_DESCRIPTOR(STRID_AUDIO_INTERFACE,
                                     DS5_AUDIO_EP_OUT,
                                     DUALSENSE_USB_UAC1_PACKET_SIZE),

    0x09, TUSB_DESC_INTERFACE,
    DS5_HID_INTERFACE, 0x00, 0x02,
    TUSB_CLASS_HID, 0x00, 0x00, STRID_HID_INTERFACE,

    0x09, HID_DESC_TYPE_HID,
    U16_TO_U8S_LE(0x0111),
    0x00, 0x01, HID_DESC_TYPE_REPORT,
    U16_TO_U8S_LE(sizeof(s_ds5_hid_report_descriptor)),

    0x07, TUSB_DESC_ENDPOINT,
    DS5_HID_EP_IN, TUSB_XFER_INTERRUPT,
    U16_TO_U8S_LE(DS5_HID_EP_SIZE), DS5_HID_POLL_MS,

    0x07, TUSB_DESC_ENDPOINT,
    DS5_HID_EP_OUT, TUSB_XFER_INTERRUPT,
    U16_TO_U8S_LE(DS5_HID_EP_SIZE), DS5_HID_POLL_MS,
};

static const char *s_ds5_string_descriptors[] = {
    "",
    "Sony Interactive Entertainment",
    DS5_USB_PRODUCT,
    DS5_PROFILE_SERIAL,
    "Wireless Controller Audio",
    DS5_USB_PRODUCT,
};

_Static_assert(sizeof(s_ds5_configuration_descriptor) == DS5_CONFIG_TOTAL_LEN,
               "DualSense configuration descriptor length mismatch");
_Static_assert(DS5_AUDIO_RENDER_DESC_LEN == DUALSENSE_USB_UAC1_DESC_LEN,
               "UAC1 descriptor length mismatch");

const tusb_desc_device_t *dualsense_usb_device_descriptor(void)
{
    return &s_ds5_device_descriptor;
}

const uint8_t *dualsense_usb_configuration_descriptor(void)
{
    return s_ds5_configuration_descriptor;
}

const char **dualsense_usb_string_descriptors(void)
{
    return s_ds5_string_descriptors;
}

int dualsense_usb_string_descriptor_count(void)
{
    return sizeof(s_ds5_string_descriptors) / sizeof(s_ds5_string_descriptors[0]);
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_ds5_hid_report_descriptor;
}
