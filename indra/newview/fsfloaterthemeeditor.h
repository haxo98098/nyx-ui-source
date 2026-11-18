/**
 * @file fsfloaterthemeeditor.h
 * @brief Theme editor floater for real-time color customization
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Copyright (c) 2025
 */

#ifndef FS_FLOATER_THEME_EDITOR_H
#define FS_FLOATER_THEME_EDITOR_H

#include "llfloater.h"
#include "v4color.h"
#include "fsdynamictheme.h"

class LLButton;
class LLColorSwatchCtrl;
class LLSliderCtrl;
class LLComboBox;
class LLCheckBoxCtrl;
class LLLineEditor;
class LLTextBox;
class LLPanel;
class LLTabContainer;

class FSFloaterThemeEditor : public LLFloater
{
public:
    FSFloaterThemeEditor(const LLSD& key);
    ~FSFloaterThemeEditor();

    bool postBuild() override;
    void draw() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;

    void refreshColors();
    void refreshPerButtonColors();

    // Global color callbacks
    void onColorChanged(EThemeElement element, LLColorSwatchCtrl* swatch);
    void onAlphaChanged(EThemeElement element, LLSliderCtrl* slider);

    // Per-button color callbacks
    void onPerButtonColorChanged(EThemeElement element, LLColorSwatchCtrl* swatch);
    void onPerButtonAlphaChanged(EThemeElement element, LLSliderCtrl* slider);

    // Other callbacks
    void onPresetSelected();
    void onSavePreset();
    void onResetToDefault();
    void onOpacityChanged();
    void onGlowChanged();
    void onExportTheme();
    void onImportTheme();

private:
    // Tab container
    LLTabContainer* mTabContainer = nullptr;

    // Global color swatches and paired alpha sliders
    std::map<EThemeElement, LLColorSwatchCtrl*> mColorSwatches;
    std::map<EThemeElement, LLSliderCtrl*> mAlphaSliders;

    // Per-button override swatches and alpha sliders
    std::map<EThemeElement, LLColorSwatchCtrl*> mPerButtonSwatches;
    std::map<EThemeElement, LLSliderCtrl*> mPerButtonAlphaSliders;

    // Preset controls
    LLComboBox* mPresetCombo = nullptr;

    // Effects controls
    LLSliderCtrl* mWindowAlphaSlider = nullptr;
    LLSliderCtrl* mPanelAlphaSlider = nullptr;
    LLCheckBoxCtrl* mGlowEnabledCheck = nullptr;
    LLSliderCtrl* mGlowIntensitySlider = nullptr;
    LLSliderCtrl* mGlowRadiusSlider = nullptr;

    // Per-button targeting
    LLLineEditor* mTargetButtonEditor = nullptr;
    LLTextBox* mTargetStatusText = nullptr;
    bool mPickingTargetButton = false;
    std::string mLastFocusedButtonPath;

    // Button targeting helpers
    std::string getTargetButtonPathname() const;
    bool hasTargetButton() const;
    void onUseFocusedButton();
    void onPickTargetButton();
    void onClearTargetButton();
    void stopPickingTargetButton();
    void updateTargetStatus();

    // Init helpers
    void initColorSwatch(const std::string& name, EThemeElement element);
    void initAlphaSlider(const std::string& name, EThemeElement element);
    void initPerButtonSwatch(const std::string& name, EThemeElement element);
    void initPerButtonAlphaSlider(const std::string& name, EThemeElement element);
    void populatePresets();
};

#endif // FS_FLOATER_THEME_EDITOR_H
