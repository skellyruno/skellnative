#include "IDs.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory_constexpr.h"

namespace Skelly::NativeSpacer {
Steinberg::FUnknown* createProcessor(void*);
Steinberg::FUnknown* createController(void*);
}

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF("Skell Audio", "https://skellyr.uno", "info@skellyr.uno", 2)

DEF_CLASS(Skelly::NativeSpacer::kProcessorUID, PClassInfo::kManyInstances,
          kVstAudioEffectClass, "Skell Spacer Native DEV", Vst::kDistributable,
          "Fx|Dynamics", "0.1.0", kVstVersionString, Skelly::NativeSpacer::createProcessor, nullptr)

DEF_CLASS(Skelly::NativeSpacer::kControllerUID, PClassInfo::kManyInstances,
          kVstComponentControllerClass, "Skell Spacer Native DEV Edit",
          0, "", "0.1.0", kVstVersionString, Skelly::NativeSpacer::createController, nullptr)

END_FACTORY
