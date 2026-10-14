/*
 created by Deqing Sun for use with CH55xduino
 Modified for VMacropad real-time USB LED and configuration control
 */

#include "USBhandler.h"
#include "USBconstant.h"

// Forward declarations for LED controller functions
void led_set_vmacropad_rgb(uint8_t r0, uint8_t g0, uint8_t b0,
                           uint8_t r1, uint8_t g1, uint8_t b1,
                           uint8_t r2, uint8_t g2, uint8_t b2);
void led_set_vmacropad_single(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
void led_set_vmacropad_mode(uint8_t mode);

void USB_EP1_OUT();
void USB_EP2_IN();
void USB_EP2_OUT();

// clang-format off
__xdata __at (EP0_ADDR) uint8_t Ep0Buffer[8];
__xdata __at (EP1_ADDR) uint8_t Ep1Buffer[128]; // EP1 IN (0..63) and OUT (64..127)
// clang-format on

#if (EP1_ADDR + 128) > USER_USB_RAM
#error "This example needs more USB ram. Increase this setting in menu."
#endif

__data uint16_t SetupLen;
__data uint8_t SetupReq;
volatile __xdata uint8_t UsbConfig;

__code uint8_t *__data pDescr;
volatile uint8_t usbMsgFlags = 0;

inline void NOP_Process(void) {}

// Parse VMacropad packets from either Endpoint 1 OUT or Endpoint 0 Setup/Feature
static void handle_vmacropad_packet(const uint8_t *buf, uint8_t len) {
  uint8_t offset = 0;
  if (len < 2) return;
  if (buf[0] == 0x03) {
    offset = 1; // Skip Report ID 3
  }

  // 0xB1: Multi-key RGB [0xB1, r0, g0, b0, r1, g1, b1, r2, g2, b2]
  if (buf[offset] == 0xB1 && len >= offset + 10) {
    led_set_vmacropad_rgb(buf[offset + 1], buf[offset + 2], buf[offset + 3],
                          buf[offset + 4], buf[offset + 5], buf[offset + 6],
                          buf[offset + 7], buf[offset + 8], buf[offset + 9]);
  }
  // 0xB2: Single-key RGB [0xB2, idx, r, g, b]
  else if (buf[offset] == 0xB2 && len >= offset + 5) {
    led_set_vmacropad_single(buf[offset + 1], buf[offset + 2], buf[offset + 3], buf[offset + 4]);
  }
  // 0xB0: LED Mode or Single RGB
  else if (buf[offset] == 0xB0) {
    if (len >= offset + 5 && buf[offset + 1] == 0x18) {
      // Stock single-color RGB: [0xB0, 0x18, r, g, b]
      led_set_vmacropad_rgb(buf[offset + 2], buf[offset + 3], buf[offset + 4],
                            buf[offset + 2], buf[offset + 3], buf[offset + 4],
                            buf[offset + 2], buf[offset + 3], buf[offset + 4]);
    } else if (len >= offset + 3 && buf[offset + 1] == 0x08) {
      // Mode command: [0xB0, 0x08, mode]
      led_set_vmacropad_mode(buf[offset + 2]);
    } else if (len >= offset + 2) {
      // 2-byte mode command: [0xB0, mode]
      led_set_vmacropad_mode(buf[offset + 1]);
    }
  }
}

void USB_EP1_OUT() {
  uint8_t len = USB_RX_LEN;
  if (len > 0) {
    handle_vmacropad_packet(&Ep1Buffer[64], len);
  }
  UEP1_CTRL = UEP1_CTRL & ~MASK_UEP_R_RES | UEP_R_RES_ACK;
}

void USB_EP0_SETUP() {
  __data uint8_t len = USB_RX_LEN;
  if (len == (sizeof(USB_SETUP_REQ))) {
    SetupLen = ((uint16_t)UsbSetupBuf->wLengthH << 8) | (UsbSetupBuf->wLengthL);
    len = 0;
    SetupReq = UsbSetupBuf->bRequest;
    usbMsgFlags = 0;
    if ((UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD) {
      switch ((UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK)) {
      case USB_REQ_TYP_VENDOR:
        len = 0xFF;
        break;
      case USB_REQ_TYP_CLASS:
        switch (SetupReq) {
        case 0x09: // SET_REPORT (Accept Feature/Output reports on EP0)
        case 0x0A: // SET_IDLE
          len = 0;
          break;
        default:
          len = 0xFF;
          break;
        }
        break;
      default:
        len = 0xFF;
        break;
      }
    } else {
      switch (SetupReq) {
      case USB_GET_DESCRIPTOR:
        switch (UsbSetupBuf->wValueH) {
        case 1:
          pDescr = (__code uint8_t *)DeviceDescriptor;
          len = sizeof(USB_Descriptor_Device_t);
          break;
        case 2:
          pDescr = (__code uint8_t *)ConfigurationDescriptor;
          len = sizeof(USB_Descriptor_Configuration_t);
          break;
        case 3:
          if (UsbSetupBuf->wValueL == 0) {
            pDescr = LanguageDescriptor;
          } else if (UsbSetupBuf->wValueL == 1) {
            pDescr = (__code uint8_t *)ManufacturerDescriptor;
          } else if (UsbSetupBuf->wValueL == 2) {
            pDescr = (__code uint8_t *)ProductDescriptor;
          } else if (UsbSetupBuf->wValueL == 3) {
            pDescr = (__code uint8_t *)SerialDescriptor;
          } else {
            len = 0xff;
            break;
          }
          len = pDescr[0];
          break;
        case 0x22:
          if (UsbSetupBuf->wValueL == 0) {
            pDescr = (__code uint8_t *)ReportDescriptor;
            len = ConfigurationDescriptor.HID_KeyboardHID.HIDReportLength;
          } else {
            len = 0xff;
          }
          break;
        default:
          len = 0xff;
          break;
        }
        if (len != 0xff) {
          if (SetupLen > len) SetupLen = len;
          len = SetupLen >= DEFAULT_ENDP0_SIZE ? DEFAULT_ENDP0_SIZE : SetupLen;
          for (__data uint8_t i = 0; i < len; i++) {
            Ep0Buffer[i] = pDescr[i];
          }
          SetupLen -= len;
          pDescr += len;
        }
        break;
      case USB_SET_ADDRESS:
        SetupLen = UsbSetupBuf->wValueL;
        break;
      case USB_GET_CONFIGURATION:
        Ep0Buffer[0] = UsbConfig;
        if (SetupLen >= 1) len = 1;
        break;
      case USB_SET_CONFIGURATION:
        UsbConfig = UsbSetupBuf->wValueL;
        break;
      case USB_CLEAR_FEATURE:
        len = 0xFF;
        break;
      case USB_SET_FEATURE:
        len = 0xFF;
        break;
      case USB_GET_STATUS:
        Ep0Buffer[0] = 0x00;
        Ep0Buffer[1] = 0x00;
        len = (SetupLen >= 2) ? 2 : SetupLen;
        break;
      default:
        len = 0xff;
        break;
      }
    }
  } else {
    len = 0xff;
  }

  if (len == 0xff) {
    SetupReq = 0xFF;
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
  } else if (len <= DEFAULT_ENDP0_SIZE) {
    UEP0_T_LEN = len;
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
  } else {
    UEP0_T_LEN = 0;
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
  }
}

void USB_EP0_IN() {
  switch (SetupReq) {
  case USB_GET_DESCRIPTOR: {
    __data uint8_t len = SetupLen >= DEFAULT_ENDP0_SIZE ? DEFAULT_ENDP0_SIZE : SetupLen;
    for (__data uint8_t i = 0; i < len; i++) {
      Ep0Buffer[i] = pDescr[i];
    }
    SetupLen -= len;
    pDescr += len;
    UEP0_T_LEN = len;
    UEP0_CTRL ^= bUEP_T_TOG;
  } break;
  case USB_SET_ADDRESS:
    USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | SetupLen;
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    break;
  default:
    UEP0_T_LEN = 0;
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    break;
  }
}

void USB_EP0_OUT() {
  handle_vmacropad_packet(Ep0Buffer, 8);
  UEP0_T_LEN = 0;
  UEP0_CTRL |= UEP_R_RES_ACK | UEP_T_RES_NAK;
}

#pragma save
#pragma nooverlay
void USBInterrupt(void) {
  if (UIF_TRANSFER) {
    __data uint8_t callIndex = USB_INT_ST & MASK_UIS_ENDP;
    switch (USB_INT_ST & MASK_UIS_TOKEN) {
    case UIS_TOKEN_OUT:
      switch (callIndex) {
      case 0: EP0_OUT_Callback(); break;
      case 1: USB_EP1_OUT(); break;
      case 2: EP2_OUT_Callback(); break;
      case 3: EP3_OUT_Callback(); break;
      case 4: EP4_OUT_Callback(); break;
      }
      break;
    case UIS_TOKEN_SOF:
      switch (callIndex) {
      case 0: EP0_SOF_Callback(); break;
      case 1: EP1_SOF_Callback(); break;
      case 2: EP2_SOF_Callback(); break;
      case 3: EP3_SOF_Callback(); break;
      case 4: EP4_SOF_Callback(); break;
      }
      break;
    case UIS_TOKEN_IN:
      switch (callIndex) {
      case 0: EP0_IN_Callback(); break;
      case 1: EP1_IN_Callback(); break;
      case 2: EP2_IN_Callback(); break;
      case 3: EP3_IN_Callback(); break;
      case 4: EP4_IN_Callback(); break;
      }
      break;
    case UIS_TOKEN_SETUP:
      switch (callIndex) {
      case 0: EP0_SETUP_Callback(); break;
      case 1: EP1_SETUP_Callback(); break;
      case 2: EP2_SETUP_Callback(); break;
      case 3: EP3_SETUP_Callback(); break;
      case 4: EP4_SETUP_Callback(); break;
      }
      break;
    }
    UIF_TRANSFER = 0;
  }

  if (UIF_BUS_RST) {
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;
    USB_DEV_AD = 0x00;
    UIF_SUSPEND = 0;
    UIF_TRANSFER = 0;
    UIF_BUS_RST = 0;
    UsbConfig = 0;
  }

  if (UIF_SUSPEND) {
    UIF_SUSPEND = 0;
    if (!(USB_MIS_ST & bUMS_SUSPEND)) {
      USB_INT_FG = 0xFF;
    }
  }
}
#pragma restore

void USBDeviceCfg() {
  USB_CTRL = 0x00;
  USB_CTRL &= ~bUC_HOST_MODE;
  USB_CTRL |= bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;
  USB_DEV_AD = 0x00;
  USB_CTRL &= ~bUC_LOW_SPEED;
  UDEV_CTRL &= ~bUD_LOW_SPEED;
#if defined(CH551) || defined(CH552) || defined(CH549)
  UDEV_CTRL = bUD_PD_DIS;
#endif
  UDEV_CTRL |= bUD_PORT_EN;
}

void USBDeviceIntCfg() {
  USB_INT_EN |= bUIE_SUSPEND;
  USB_INT_EN |= bUIE_TRANSFER;
  USB_INT_EN |= bUIE_BUS_RST;
  USB_INT_FG |= 0x1F;
  IE_USB = 1;
  EA = 1;
}

void USBDeviceEndPointCfg() {
  UEP0_DMA = (uint16_t)Ep0Buffer;
  UEP1_DMA = (uint16_t)Ep1Buffer;
  UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;
  UEP4_1_MOD = 0XC0; // Endpoint 1 TX and RX enable
  UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
}
