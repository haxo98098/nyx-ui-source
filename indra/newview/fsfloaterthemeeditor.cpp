/**
 * @file fsfloaterthemeeditor.cpp
 * @brief Theme editor floater implementation - 4-tab layout
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Copyright (c) 2025
 */

#include "llviewerprecompiledheaders.h"
#include "fsfloaterthemeeditor.h"

#include "fsdynamictheme.h"
#include "llcolorswatch.h"
#include "llsliderctrl.h"
#include "llcombobox.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llviewercontrol.h"
#include "llfocusmgr.h"
#include "llview.h"
#include "llui.h"
#include "lltextbox.h"
#include "llpanel.h"
#include "lltabcontainer.h"

FSFloaterThemeEditor::FSFloaterThemeEditor(const LLSD& key)
    : LLFloater(key)
{
}

FSFloaterThemeEditor::~FSFloaterThemeEditor()
{
    stopPickingTargetButton();
}

bool FSFloaterThemeEditor::postBuild()
{
    // Tab container
    mTabContainer = findChild<LLTabContainer>("theme_tabs");

    // ---------------------------------------------------------------
    // TAB 1: Colors - 32 global swatches + 32 paired alpha sliders
    // ---------------------------------------------------------------

    // Background colors
    initColorSwatch("bg_primary", THEME_BG_PRIMARY);
    initColorSwatch("bg_secondary", THEME_BG_SECONDARY);
    initColorSwatch("bg_tertiary", THEME_BG_TERTIARY);
    initColorSwatch("bg_hover", THEME_BG_HOVER);
    initColorSwatch("bg_selected", THEME_BG_SELECTED);

    initAlphaSlider("bg_primary_alpha", THEME_BG_PRIMARY);
    initAlphaSlider("bg_secondary_alpha", THEME_BG_SECONDARY);
    initAlphaSlider("bg_tertiary_alpha", THEME_BG_TERTIARY);
    initAlphaSlider("bg_hover_alpha", THEME_BG_HOVER);
    initAlphaSlider("bg_selected_alpha", THEME_BG_SELECTED);

    // Text colors
    initColorSwatch("text_primary", THEME_TEXT_PRIMARY);
    initColorSwatch("text_secondary", THEME_TEXT_SECONDARY);
    initColorSwatch("text_disabled", THEME_TEXT_DISABLED);
    initColorSwatch("text_highlight", THEME_TEXT_HIGHLIGHT);

    initAlphaSlider("text_primary_alpha", THEME_TEXT_PRIMARY);
    initAlphaSlider("text_secondary_alpha", THEME_TEXT_SECONDARY);
    initAlphaSlider("text_disabled_alpha", THEME_TEXT_DISABLED);
    initAlphaSlider("text_highlight_alpha", THEME_TEXT_HIGHLIGHT);

    // Accent colors
    initColorSwatch("accent_primary", THEME_ACCENT_PRIMARY);
    initColorSwatch("accent_secondary", THEME_ACCENT_SECONDARY);
    initColorSwatch("accent_hover", THEME_ACCENT_HOVER);
    initColorSwatch("accent_active", THEME_ACCENT_ACTIVE);

    initAlphaSlider("accent_primary_alpha", THEME_ACCENT_PRIMARY);
    initAlphaSlider("accent_secondary_alpha", THEME_ACCENT_SECONDARY);
    initAlphaSlider("accent_hover_alpha", THEME_ACCENT_HOVER);
    initAlphaSlider("accent_active_alpha", THEME_ACCENT_ACTIVE);

    // Border colors
    initColorSwatch("border", THEME_BORDER);
    initColorSwatch("border_focused", THEME_BORDER_FOCUSED);

    initAlphaSlider("border_alpha", THEME_BORDER);
    initAlphaSlider("border_focused_alpha", THEME_BORDER_FOCUSED);

    // Button colors
    initColorSwatch("button_bg", THEME_BUTTON_BG);
    initColorSwatch("button_bg_hover", THEME_BUTTON_BG_HOVER);
    initColorSwatch("button_bg_active", THEME_BUTTON_BG_ACTIVE);
    initColorSwatch("button_text", THEME_BUTTON_TEXT);

    initAlphaSlider("button_bg_alpha", THEME_BUTTON_BG);
    initAlphaSlider("button_bg_hover_alpha", THEME_BUTTON_BG_HOVER);
    initAlphaSlider("button_bg_active_alpha", THEME_BUTTON_BG_ACTIVE);
    initAlphaSlider("button_text_alpha", THEME_BUTTON_TEXT);

    // Chat colors
    initColorSwatch("chat_bg", THEME_CHAT_BG);
    initColorSwatch("chat_text", THEME_CHAT_TEXT);
    initColorSwatch("chat_system", THEME_CHAT_SYSTEM);
    initColorSwatch("chat_self", THEME_CHAT_SELF);
    initColorSwatch("chat_others", THEME_CHAT_OTHERS);

    initAlphaSlider("chat_bg_alpha", THEME_CHAT_BG);
    initAlphaSlider("chat_text_alpha", THEME_CHAT_TEXT);
    initAlphaSlider("chat_system_alpha", THEME_CHAT_SYSTEM);
    initAlphaSlider("chat_self_alpha", THEME_CHAT_SELF);
    initAlphaSlider("chat_others_alpha", THEME_CHAT_OTHERS);

    // Radar colors
    initColorSwatch("radar_bg", THEME_RADAR_BG);
    initColorSwatch("radar_text", THEME_RADAR_TEXT);
    initColorSwatch("radar_highlight", THEME_RADAR_HIGHLIGHT);

    initAlphaSlider("radar_bg_alpha", THEME_RADAR_BG);
    initAlphaSlider("radar_text_alpha", THEME_RADAR_TEXT);
    initAlphaSlider("radar_highlight_alpha", THEME_RADAR_HIGHLIGHT);

    // Inventory colors
    initColorSwatch("inventory_bg", THEME_INVENTORY_BG);
    initColorSwatch("inventory_folder", THEME_INVENTORY_FOLDER);
    initColorSwatch("inventory_item", THEME_INVENTORY_ITEM);

    initAlphaSlider("inventory_bg_alpha", THEME_INVENTORY_BG);
    initAlphaSlider("inventory_folder_alpha", THEME_INVENTORY_FOLDER);
    initAlphaSlider("inventory_item_alpha", THEME_INVENTORY_ITEM);

    // Effect colors (glow + shadow)
    initColorSwatch("glow_color", THEME_GLOW);
    initColorSwatch("shadow_color", THEME_SHADOW);

    initAlphaSlider("glow_color_alpha", THEME_GLOW);
    initAlphaSlider("shadow_color_alpha", THEME_SHADOW);

    // ---------------------------------------------------------------
    // TAB 2: Effects - transparency + glow controls
    // ---------------------------------------------------------------

    mWindowAlphaSlider = findChild<LLSliderCtrl>("window_alpha");
    if (mWindowAlphaSlider)
    {
        mWindowAlphaSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onOpacityChanged(); });
    }

    mPanelAlphaSlider = findChild<LLSliderCtrl>("panel_alpha");
    if (mPanelAlphaSlider)
    {
        mPanelAlphaSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onOpacityChanged(); });
    }

    mGlowEnabledCheck = findChild<LLCheckBoxCtrl>("glow_enabled");
    if (mGlowEnabledCheck)
    {
        mGlowEnabledCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGlowChanged(); });
    }

    mGlowIntensitySlider = findChild<LLSliderCtrl>("glow_intensity");
    if (mGlowIntensitySlider)
    {
        mGlowIntensitySlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGlowChanged(); });
    }

    mGlowRadiusSlider = findChild<LLSliderCtrl>("glow_radius");
    if (mGlowRadiusSlider)
    {
        mGlowRadiusSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGlowChanged(); });
    }

    // ---------------------------------------------------------------
    // TAB 3: Per-Button - target selection + 5 override swatches
    // ---------------------------------------------------------------

    mTargetButtonEditor = findChild<LLLineEditor>("target_button");
    mTargetStatusText = findChild<LLTextBox>("target_status");

    if (LLButton* btn = findChild<LLButton>("pick_target_btn"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onPickTargetButton(); });
    }
    if (LLButton* btn = findChild<LLButton>("use_focused_btn"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onUseFocusedButton(); });
    }
    if (LLButton* btn = findChild<LLButton>("clear_target_btn"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onClearTargetButton(); });
    }

    // Per-button color swatches + alpha sliders
    initPerButtonSwatch("pb_button_bg", THEME_BUTTON_BG);
    initPerButtonSwatch("pb_button_bg_hover", THEME_BUTTON_BG_HOVER);
    initPerButtonSwatch("pb_button_bg_active", THEME_BUTTON_BG_ACTIVE);
    initPerButtonSwatch("pb_button_text", THEME_BUTTON_TEXT);
    initPerButtonSwatch("pb_glow", THEME_GLOW);

    initPerButtonAlphaSlider("pb_button_bg_alpha", THEME_BUTTON_BG);
    initPerButtonAlphaSlider("pb_button_bg_hover_alpha", THEME_BUTTON_BG_HOVER);
    initPerButtonAlphaSlider("pb_button_bg_active_alpha", THEME_BUTTON_BG_ACTIVE);
    initPerButtonAlphaSlider("pb_button_text_alpha", THEME_BUTTON_TEXT);
    initPerButtonAlphaSlider("pb_glow_alpha", THEME_GLOW);

    // ---------------------------------------------------------------
    // TAB 4: Presets
    // ---------------------------------------------------------------

    mPresetCombo = findChild<LLComboBox>("preset_combo");
    if (mPresetCombo)
    {
        populatePresets();
        mPresetCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPresetSelected(); });
    }

    if (LLButton* btn = findChild<LLButton>("save_preset_btn"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onSavePreset(); });
    }
    if (LLButton* btn = findChild<LLButton>("reset_btn"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onResetToDefault(); });
    }
    if (LLButton* btn = findChild<LLButton>("export_btn"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onExportTheme(); });
    }
    if (LLButton* btn = findChild<LLButton>("import_btn"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) { onImportTheme(); });
    }

    // Quick-apply preset buttons
    if (LLButton* btn = findChild<LLButton>("apply_dark"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            FSDynamicTheme::instance().loadPreset("dark");
            refreshColors();
        });
    }
    if (LLButton* btn = findChild<LLButton>("apply_light"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            FSDynamicTheme::instance().loadPreset("light");
            refreshColors();
        });
    }
    if (LLButton* btn = findChild<LLButton>("apply_cyberpunk"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            FSDynamicTheme::instance().loadPreset("cyberpunk");
            refreshColors();
        });
    }
    if (LLButton* btn = findChild<LLButton>("apply_midnight"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            FSDynamicTheme::instance().loadPreset("midnight");
            refreshColors();
        });
    }
    if (LLButton* btn = findChild<LLButton>("apply_neon"))
    {
        btn->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            FSDynamicTheme::instance().loadPreset("neon");
            refreshColors();
        });
    }

    // Load current values
    refreshColors();
    updateTargetStatus();

    return TRUE;
}

// ===================================================================
// draw() - focus-based button tracking for pick mode
// ===================================================================

void FSFloaterThemeEditor::draw()
{
    // Track the last-clicked button across the entire UI.
    // This powers both "Pick" mode and "Use Focused".
    // Mouse capture is checked first: every LLButton captures the mouse on
    // mouse-down, while many buttons (toolbar/chrome, no tab stop) never take
    // keyboard focus - focus alone misses those.
    LLButton* focused_btn = dynamic_cast<LLButton*>(gFocusMgr.getMouseCapture());
    if (!focused_btn)
    {
        focused_btn = dynamic_cast<LLButton*>(gFocusMgr.getKeyboardFocus());
    }

    if (focused_btn)
    {
        // Ignore buttons that are children of THIS floater
        // (clicking our own Pick/Use/Clear/swatch buttons shouldn't count)
        bool is_our_child = false;
        LLView* parent = focused_btn->getParent();
        while (parent)
        {
            if (parent == static_cast<LLView*>(this))
            {
                is_our_child = true;
                break;
            }
            parent = parent->getParent();
        }

        if (!is_our_child)
        {
            mLastFocusedButtonPath = focused_btn->getPathname();

            // If in pick mode, auto-select this button
            if (mPickingTargetButton && mTargetButtonEditor)
            {
                mTargetButtonEditor->setValue(mLastFocusedButtonPath);
                updateTargetStatus();
                refreshPerButtonColors();
                mPickingTargetButton = false;
            }
        }
    }

    LLFloater::draw();
}

// ===================================================================
// Init helpers
// ===================================================================

void FSFloaterThemeEditor::initColorSwatch(const std::string& name, EThemeElement element)
{
    LLColorSwatchCtrl* swatch = findChild<LLColorSwatchCtrl>(name);
    if (swatch)
    {
        mColorSwatches[element] = swatch;
        swatch->setCommitCallback([this, element, swatch](LLUICtrl*, const LLSD&) {
            onColorChanged(element, swatch);
        });
    }
}

void FSFloaterThemeEditor::initAlphaSlider(const std::string& name, EThemeElement element)
{
    LLSliderCtrl* slider = findChild<LLSliderCtrl>(name);
    if (slider)
    {
        mAlphaSliders[element] = slider;
        slider->setCommitCallback([this, element, slider](LLUICtrl*, const LLSD&) {
            onAlphaChanged(element, slider);
        });
    }
}

void FSFloaterThemeEditor::initPerButtonSwatch(const std::string& name, EThemeElement element)
{
    LLColorSwatchCtrl* swatch = findChild<LLColorSwatchCtrl>(name);
    if (swatch)
    {
        mPerButtonSwatches[element] = swatch;
        swatch->setCommitCallback([this, element, swatch](LLUICtrl*, const LLSD&) {
            onPerButtonColorChanged(element, swatch);
        });
    }
}

void FSFloaterThemeEditor::initPerButtonAlphaSlider(const std::string& name, EThemeElement element)
{
    LLSliderCtrl* slider = findChild<LLSliderCtrl>(name);
    if (slider)
    {
        mPerButtonAlphaSliders[element] = slider;
        slider->setCommitCallback([this, element, slider](LLUICtrl*, const LLSD&) {
            onPerButtonAlphaChanged(element, slider);
        });
    }
}

// ===================================================================
// Global color callbacks (Tab 1: Colors)
// ===================================================================

void FSFloaterThemeEditor::onColorChanged(EThemeElement element, LLColorSwatchCtrl* swatch)
{
    if (!swatch) return;

    LLColor4 color = swatch->get();

    // Apply paired alpha slider value if one exists
    auto alpha_it = mAlphaSliders.find(element);
    if (alpha_it != mAlphaSliders.end() && alpha_it->second)
    {
        color.mV[VALPHA] = (F32)alpha_it->second->getValue().asReal();
    }

    FSDynamicTheme::instance().setColor(element, color);
}

void FSFloaterThemeEditor::onAlphaChanged(EThemeElement element, LLSliderCtrl* slider)
{
    if (!slider) return;

    F32 alpha = (F32)slider->getValue().asReal();

    // Get the paired swatch color
    auto swatch_it = mColorSwatches.find(element);
    if (swatch_it != mColorSwatches.end() && swatch_it->second)
    {
        LLColor4 color = swatch_it->second->get();
        color.mV[VALPHA] = alpha;
        FSDynamicTheme::instance().setColor(element, color);
    }
}

// ===================================================================
// Per-button color callbacks (Tab 3: Per-Button)
// ===================================================================

void FSFloaterThemeEditor::onPerButtonColorChanged(EThemeElement element, LLColorSwatchCtrl* swatch)
{
    if (!swatch || !hasTargetButton()) return;

    LLColor4 color = swatch->get();

    // Apply paired per-button alpha slider
    auto alpha_it = mPerButtonAlphaSliders.find(element);
    if (alpha_it != mPerButtonAlphaSliders.end() && alpha_it->second)
    {
        color.mV[VALPHA] = (F32)alpha_it->second->getValue().asReal();
    }

    FSDynamicTheme::instance().setButtonOverrideColor(getTargetButtonPathname(), element, color);
}

void FSFloaterThemeEditor::onPerButtonAlphaChanged(EThemeElement element, LLSliderCtrl* slider)
{
    if (!slider || !hasTargetButton()) return;

    F32 alpha = (F32)slider->getValue().asReal();

    // Get the paired per-button swatch color
    auto swatch_it = mPerButtonSwatches.find(element);
    if (swatch_it != mPerButtonSwatches.end() && swatch_it->second)
    {
        LLColor4 color = swatch_it->second->get();
        color.mV[VALPHA] = alpha;
        FSDynamicTheme::instance().setButtonOverrideColor(getTargetButtonPathname(), element, color);
    }
}

// ===================================================================
// Refresh
// ===================================================================

void FSFloaterThemeEditor::refreshColors()
{
    FSDynamicTheme& theme = FSDynamicTheme::instance();

    // Update all global color swatches and sync paired alpha sliders
    for (auto& pair : mColorSwatches)
    {
        if (pair.second)
        {
            LLColor4 color = theme.getColor(pair.first);
            pair.second->set(color);

            // Sync alpha slider from color's alpha channel
            auto alpha_it = mAlphaSliders.find(pair.first);
            if (alpha_it != mAlphaSliders.end() && alpha_it->second)
            {
                alpha_it->second->setValue(color.mV[VALPHA]);
            }
        }
    }

    // Update opacity sliders
    FSThemeOpacity opacity = theme.getOpacity();
    if (mWindowAlphaSlider)
    {
        mWindowAlphaSlider->setValue(opacity.window_alpha);
    }
    if (mPanelAlphaSlider)
    {
        mPanelAlphaSlider->setValue(opacity.panel_alpha);
    }

    // Update glow controls
    FSThemeGlow glow = theme.getGlow();
    if (mGlowEnabledCheck)
    {
        mGlowEnabledCheck->setValue(glow.enabled);
    }
    if (mGlowIntensitySlider)
    {
        mGlowIntensitySlider->setValue(glow.intensity);
    }
    if (mGlowRadiusSlider)
    {
        mGlowRadiusSlider->setValue(glow.radius);
    }

    // Refresh per-button tab swatches if a target is set
    refreshPerButtonColors();
}

void FSFloaterThemeEditor::refreshPerButtonColors()
{
    FSDynamicTheme& theme = FSDynamicTheme::instance();

    // Initialize per-button swatches from the current global colors
    // (the per-button overrides don't have a separate getter - they're
    //  applied via LLUIColorTable and the button reads them automatically)
    for (auto& pair : mPerButtonSwatches)
    {
        if (pair.second)
        {
            LLColor4 color = theme.getColor(pair.first);
            pair.second->set(color);

            auto alpha_it = mPerButtonAlphaSliders.find(pair.first);
            if (alpha_it != mPerButtonAlphaSliders.end() && alpha_it->second)
            {
                alpha_it->second->setValue(color.mV[VALPHA]);
            }
        }
    }
}

// ===================================================================
// Target button helpers
// ===================================================================

std::string FSFloaterThemeEditor::getTargetButtonPathname() const
{
    if (!mTargetButtonEditor) return std::string();
    return mTargetButtonEditor->getValue().asString();
}

bool FSFloaterThemeEditor::hasTargetButton() const
{
    return !getTargetButtonPathname().empty();
}

void FSFloaterThemeEditor::updateTargetStatus()
{
    if (!mTargetStatusText) return;

    std::string target = getTargetButtonPathname();
    if (target.empty())
    {
        mTargetStatusText->setValue("No button selected. Use Pick or Focused to target a button.");
        mTargetStatusText->setColor(LLColor4(0.8f, 0.6f, 0.2f, 1.0f));
    }
    else
    {
        std::string display = target;
        if (display.length() > 50)
        {
            display = "..." + display.substr(display.length() - 47);
        }
        mTargetStatusText->setValue("Targeting: " + display);
        mTargetStatusText->setColor(LLColor4(0.2f, 0.7f, 0.3f, 1.0f));
    }
}

void FSFloaterThemeEditor::onUseFocusedButton()
{
    if (!mTargetButtonEditor) return;

    // Use the last-tracked focused button path (not current focus,
    // because clicking "Use Focused" itself changes focus to this button)
    if (!mLastFocusedButtonPath.empty())
    {
        mTargetButtonEditor->setValue(mLastFocusedButtonPath);
        updateTargetStatus();
        refreshPerButtonColors();
    }
    else
    {
        LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE",
            "No button has been clicked yet. Click a button in the UI first, then click Use Focused."));
    }
}

void FSFloaterThemeEditor::onClearTargetButton()
{
    if (!mTargetButtonEditor) return;

    std::string target = getTargetButtonPathname();
    if (!target.empty())
    {
        FSDynamicTheme::instance().clearButtonOverrides(target);
    }

    mPickingTargetButton = false;
    mTargetButtonEditor->setValue("");
    updateTargetStatus();
}

void FSFloaterThemeEditor::onPickTargetButton()
{
    if (!mTargetButtonEditor) return;

    // No mouse capture - just set the flag. draw() will detect
    // when any button outside our floater gets focused.
    mPickingTargetButton = true;

    if (mTargetStatusText)
    {
        mTargetStatusText->setValue("PICK MODE: Click any button in the UI...");
        mTargetStatusText->setColor(LLColor4(1.0f, 0.3f, 0.3f, 1.0f));
    }
}

void FSFloaterThemeEditor::stopPickingTargetButton()
{
    mPickingTargetButton = false;
    updateTargetStatus();
}

// ===================================================================
// Open / Close
// ===================================================================

void FSFloaterThemeEditor::onOpen(const LLSD& key)
{
    refreshColors();
}

void FSFloaterThemeEditor::onClose(bool app_quitting)
{
    stopPickingTargetButton();
}

// ===================================================================
// Presets
// ===================================================================

void FSFloaterThemeEditor::populatePresets()
{
    if (!mPresetCombo) return;

    mPresetCombo->clearRows();

    std::vector<std::string> presets = FSDynamicTheme::instance().getAvailablePresets();
    for (const std::string& preset : presets)
    {
        mPresetCombo->add(preset);
    }
}

void FSFloaterThemeEditor::onPresetSelected()
{
    if (!mPresetCombo) return;
    std::string preset = mPresetCombo->getValue().asString();
    if (!preset.empty())
    {
        FSDynamicTheme::instance().loadPreset(preset);
        refreshColors();
    }
}

void FSFloaterThemeEditor::onSavePreset()
{
    LLLineEditor* name_editor = findChild<LLLineEditor>("preset_name");
    if (!name_editor) return;

    std::string name = name_editor->getValue().asString();
    if (name.empty())
    {
        LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", "Please enter a preset name"));
        return;
    }

    FSDynamicTheme::instance().saveCurrentAsPreset(name);
    populatePresets();

    LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", "Preset saved: " + name));
}

void FSFloaterThemeEditor::onResetToDefault()
{
    FSDynamicTheme::instance().resetToDefaults();
    refreshColors();
}

// ===================================================================
// Effects callbacks
// ===================================================================

void FSFloaterThemeEditor::onOpacityChanged()
{
    if (mWindowAlphaSlider)
    {
        F32 window_alpha = (F32)mWindowAlphaSlider->getValue().asReal();
        FSDynamicTheme::instance().setWindowAlpha(window_alpha);
    }
    if (mPanelAlphaSlider)
    {
        F32 panel_alpha = (F32)mPanelAlphaSlider->getValue().asReal();
        FSDynamicTheme::instance().setPanelAlpha(panel_alpha);
    }
}

void FSFloaterThemeEditor::onGlowChanged()
{
    if (mGlowEnabledCheck)
    {
        bool enabled = mGlowEnabledCheck->getValue().asBoolean();
        FSDynamicTheme::instance().setGlowEnabled(enabled);
    }
    if (mGlowIntensitySlider)
    {
        F32 intensity = (F32)mGlowIntensitySlider->getValue().asReal();
        FSDynamicTheme::instance().setGlowIntensity(intensity);
    }
    if (mGlowRadiusSlider)
    {
        F32 radius = (F32)mGlowRadiusSlider->getValue().asReal();
        FSDynamicTheme::instance().setGlowRadius(radius);
    }
}

// ===================================================================
// Export / Import
// ===================================================================

void FSFloaterThemeEditor::onExportTheme()
{
    std::string xml = FSDynamicTheme::instance().exportToXML();

    std::string path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "theme_custom.xml");
    std::ofstream file(path);
    if (file.is_open())
    {
        file << xml;
        file.close();
        LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", "Theme exported to: " + path));
    }
}

void FSFloaterThemeEditor::onImportTheme()
{
    std::string path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "theme_custom.xml");
    std::ifstream file(path);
    if (file.is_open())
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        if (FSDynamicTheme::instance().importFromXML(buffer.str()))
        {
            refreshColors();
            LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", "Theme imported successfully"));
        }
    }
    else
    {
        LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", "Could not find theme file"));
    }
}
