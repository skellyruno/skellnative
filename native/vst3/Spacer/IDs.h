#pragma once
#include "pluginterfaces/base/funknown.h"

namespace Skelly::NativeSpacer {
// Separate experimental class IDs. Never install this build over legacy Spacer.
static DECLARE_UID(kProcessorUID, 0x0C6C714D, 0xBC9E4528, 0xA05AF80A, 0x2C219C48);
static DECLARE_UID(kControllerUID, 0xD692F31E, 0x5F754933, 0xA3B1B7F5, 0x82C49CB2);
} // namespace Skelly::NativeSpacer
