/**
 * @file fsdynamictheme.h
 * @brief Dynamic theme system with real-time color switching
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Copyright (c) 2025
 */

#ifndef FS_DYNAMIC_THEME_H
#define FS_DYNAMIC_THEME_H

#include "llsingleton.h"
#include "v4color.h"
#include <string>
#include <map>
#include <vector>
#include <functional>

// UI element types that can be customized
typedef enum e_theme_element
{
    THEME_BG_PRIMARY = 0,      // Main window backgrounds
    THEME_BG_SECONDARY,        // Panel backgrounds
    THEME_BG_TERTIARY,         // Input fields, lists
    THEME_BG_HOVER,            // Hover states
    THEME_BG_SELECTED,         // Selected items

    THEME_TEXT_PRIMARY,        // Main text
    THEME_TEXT_SECONDARY,      // Labels, hints
    THEME_TEXT_DISABLED,       // Disabled text
    THEME_TEXT_HIGHLIGHT,      // Selected text

    THEME_ACCENT_PRIMARY,      // Main accent (buttons, links)
    THEME_ACCENT_SECONDARY,    // Secondary accent
    THEME_ACCENT_HOVER,        // Accent hover
    THEME_ACCENT_ACTIVE,       // Accent active/pressed

    THEME_BORDER,              // Borders
    THEME_BORDER_FOCUSED,      // Focused borders

    THEME_BUTTON_BG,           // Button background
    THEME_BUTTON_BG_HOVER,     // Button hover
    THEME_BUTTON_BG_ACTIVE,    // Button pressed
    THEME_BUTTON_TEXT,         // Button text

    THEME_CHAT_BG,             // Chat background
    THEME_CHAT_TEXT,           // Chat text
    THEME_CHAT_SYSTEM,         // System messages
    THEME_CHAT_SELF,           // Your messages
    THEME_CHAT_OTHERS,         // Others' messages

    THEME_RADAR_BG,            // Radar background
    THEME_RADAR_TEXT,          // Radar text
    THEME_RADAR_HIGHLIGHT,     // Radar selection

    THEME_INVENTORY_BG,        // Inventory background
    THEME_INVENTORY_FOLDER,    // Folder color
    THEME_INVENTORY_ITEM,      // Item color

    THEME_GLOW,                // Glow effect color
    THEME_SHADOW,              // Shadow color

    THEME_ELEMENT_COUNT
} EThemeElement;

// Transparency/opacity settings
struct FSThemeOpacity
{
    F32 window_alpha = 1.0f;
    F32 panel_alpha = 0.95f;
    F32 control_alpha = 1.0f;
    F32 border_alpha = 0.8f;
    F32 shadow_alpha = 0.3f;
    F32 glow_alpha = 0.5f;
};

// Glow settings
struct FSThemeGlow
{
    bool enabled = true;
    F32 intensity = 0.5f;
    F32 radius = 4.0f;
    LLColor4 color;
};

// Predefined theme
struct FSThemePreset
{
    std::string name;
    std::string description;
    LLColor4 colors[THEME_ELEMENT_COUNT];
    FSThemeOpacity opacity;
    FSThemeGlow glow;
};

// Callback for theme changes
typedef std::function<void(EThemeElement element, const LLColor4& color)> theme_change_callback_t;

class FSDynamicTheme : public LLSingleton<FSDynamicTheme>
{
    LLSINGLETON(FSDynamicTheme);

public:
    ~FSDynamicTheme();

    // Initialize and load saved theme
    void init();

    // Get current color for element
    LLColor4 getColor(EThemeElement element) const;
    LLColor4 getColorWithAlpha(EThemeElement element, F32 override_alpha = -1.0f) const;

    // Set color for element (applies immediately)
    void setColor(EThemeElement element, const LLColor4& color);
    void setColor(EThemeElement element, F32 r, F32 g, F32 b, F32 a = 1.0f);

    // Set from hex string (#RRGGBB or #RRGGBBAA)
    void setColorFromHex(EThemeElement element, const std::string& hex);

    // Opacity settings
    void setOpacity(const FSThemeOpacity& opacity);
    FSThemeOpacity getOpacity() const { return mOpacity; }
    void setWindowAlpha(F32 alpha);
    void setPanelAlpha(F32 alpha);

    // Glow settings
    void setGlowEnabled(bool enabled);
    void setGlowIntensity(F32 intensity);
    void setGlowRadius(F32 radius);
    void setGlowColor(const LLColor4& color);
    FSThemeGlow getGlow() const { return mGlow; }

    // Preset themes
    void loadPreset(const std::string& preset_name);
    void saveCurrentAsPreset(const std::string& name);
    std::vector<std::string> getAvailablePresets() const;
    void deletePreset(const std::string& name);

    // Built-in presets
    void applyDarkTheme();
    void applyLightTheme();
    void applyCyberpunkTheme();
    void applyMidnightBlueTheme();
    void applyNeonTheme();

    // Get current preset name
    std::string getCurrentPresetName() const { return mCurrentPresetName; }

    // Callbacks
    // Returns an id the caller can pass to removeChangeCallback() to unregister (e.g. in a
    // floater/panel destructor), so a destroyed registrant's callback isn't invoked later
    // on a color change (use-after-free).
    S32 addChangeCallback(theme_change_callback_t callback);
    void removeChangeCallback(S32 id);
    void notifyColorChanged(EThemeElement element);

    // Apply to specific UI components
    void applyToFloater(LLFloater* floater);
    void applyToPanel(LLPanel* panel);
    void applyToButton(LLButton* button);
    void applyToScroller(LLScrollContainer* scroller);

    // Per-button overrides (by full view pathname; intended for Theme Editor "target button")
    void setButtonOverrideColor(const std::string& button_pathname, EThemeElement element, const LLColor4& color);
    void clearButtonOverrides(const std::string& button_pathname);

    // Reset to defaults
    void resetToDefaults();

    // Import/Export
    std::string exportToXML();
    bool importFromXML(const std::string& xml);

    // Save/Load settings
    void saveSettings();
    void loadSettings();

private:
    LLColor4 mColors[THEME_ELEMENT_COUNT];
    FSThemeOpacity mOpacity;
    FSThemeGlow mGlow;
    std::string mCurrentPresetName = "dark";

    std::map<S32, theme_change_callback_t> mCallbacks;
    S32 mNextCallbackId = 1;
    std::map<std::string, FSThemePreset> mCustomPresets;

    // Default colors
    void initDefaultColors();

    // Built-in preset generators
    FSThemePreset generateDarkPreset();
    FSThemePreset generateLightPreset();
    FSThemePreset generateCyberpunkPreset();
    FSThemePreset generateMidnightBluePreset();
    FSThemePreset generateNeonPreset();

    // XML helpers
    std::string colorToXML(EThemeElement element);
    void colorFromXML(EThemeElement element, const std::string& xml);

    static std::string buttonOverrideKey(const std::string& button_pathname, const char* suffix);
};

// Helper macros for common color access
#define THEME_COLOR(elem) FSDynamicTheme::instance().getColor(elem)
#define THEME_COLOR_ALPHA(elem, alpha) FSDynamicTheme::instance().getColorWithAlpha(elem, alpha)
#define THEME_SET_COLOR(elem, color) FSDynamicTheme::instance().setColor(elem, color)

#endif // FS_DYNAMIC_THEME_H
