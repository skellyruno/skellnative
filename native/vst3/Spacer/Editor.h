#pragma once

#include "Parameters.h"
#include "Spectrum.h"
#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <SkellDraw/Win32FlatControls.h>

namespace Skelly::NativeSpacer {

class EditorModel {
public:
    virtual ~EditorModel() = default;
    virtual float get(Index) = 0;
    virtual void begin(Index) = 0;
    virtual void edit(Index, float) = 0;
    virtual void end(Index) = 0;
    virtual void text(Index, Steinberg::Vst::String128) = 0;
    virtual int theme() const = 0;
    virtual void cycleTheme() = 0;
    virtual Spectrum spectrum() = 0;
};

class Editor final : public Steinberg::CPluginView, public Steinberg::IPlugViewContentScaleSupport {
public:
    Editor(EditorModel&, Steinberg::FUnknown* owner);
    ~Editor() override;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID, void**) override;
    Steinberg::uint32 PLUGIN_API addRef() override { return CPluginView::addRef(); }
    Steinberg::uint32 PLUGIN_API release() override { return CPluginView::release(); }
    Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor) override;
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API attached(void*, Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API removed() override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect*) override;
    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultTrue; }
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect*) override;
    Steinberg::tresult PLUGIN_API onWheel(float) override;
    Steinberg::tresult PLUGIN_API onKeyDown(Steinberg::char16, Steinberg::int16, Steinberg::int16) override;

private:
    static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT windowMessage(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC, int width, int height);
    void poll();
    skelldraw::Point designPoint(int x, int y) const;
    int hit(skelldraw::Point) const;
    void finishDrag();
    void editOnce(Index, float);
    void selectRange(Index, float);

    EditorModel& model_;
    Steinberg::FUnknown* owner_;
    HWND window_ = nullptr;
    WNDPROC previousProc_ = nullptr;
    HDC buffer_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previousBitmap_ = nullptr;
    int bufferWidth_ = 0, bufferHeight_ = 0;
    Spectrum spectrum_{};
    Values paintedValues_{};
    int paintedTheme_ = -1;
    int hovered_ = -1, focused_ = amount, dragging_ = -1, pressed_ = -1, startY_ = 0;
    float initial_ = 0.f, scale_ = 1.f;
};

} // namespace Skelly::NativeSpacer
