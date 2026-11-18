/**
 * @file fsdynamictheme.cpp
 * @brief Dynamic theme system implementation
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Copyright (c) 2025
 */

#include "llviewerprecompiledheaders.h"
#include "fsdynamictheme.h"

#include "llviewercontrol.h"
#include "llfloater.h"
#include "llpanel.h"
#include "llbutton.h"
#include "llscrollcontainer.h"
#include "llrootview.h"
#include "llui.h"
#include "llviewermenu.h"
#include "lluicolortable.h"

namespace
{
    const std::vector<std::string>& keys_for_element(EThemeElement element);
}

FSDynamicTheme::FSDynamicTheme()
{
    initDefaultColors();
}

FSDynamicTheme::~FSDynamicTheme()
{
}

void FSDynamicTheme::init()
{
    // Nyx: the interface has its own palette now. Colours saved by an older look would be painted
    // over it, so they are dropped once. Whatever is chosen in the theme editor afterwards stays.
    if (gSavedSettings.controlExists("NyxInterfaceRev") && gSavedSettings.getS32("NyxInterfaceRev") < 1)
    {
        for (int i = 0; i < THEME_ELEMENT_COUNT; i++)
        {
            for (const std::string& key : keys_for_element((EThemeElement)i))
            {
                LLUIColorTable::instance().resetToDefault(key);
            }
        }
        static const char* const extra[] = { "FloaterButtonImageColor", "ButtonImageColor", "DefaultHighlightLight",
            "DefaultHighlightDark", "FocusColor", "AccentColor", "FSChatHistoryBackground", "ChatSelfMentionHighlight" };
        for (const char* key : extra)
        {
            LLUIColorTable::instance().resetToDefault(key);
        }
        gSavedSettings.setS32("NyxInterfaceRev", 1);
    }

    // The colours read here are already the finished ones (skin or saved), so they are not applied
    // again: doing that multiplied the saved window alpha into itself on every start.
    loadSettings();
}

void FSDynamicTheme::initDefaultColors()
{
    // Dark theme as default
    mColors[THEME_BG_PRIMARY] = LLColor4(0.12f, 0.12f, 0.14f, 1.0f);       // Deep black
    mColors[THEME_BG_SECONDARY] = LLColor4(0.16f, 0.16f, 0.20f, 1.0f);     // Dark gray
    mColors[THEME_BG_TERTIARY] = LLColor4(0.20f, 0.20f, 0.24f, 1.0f);      // Lighter gray
    mColors[THEME_BG_HOVER] = LLColor4(0.25f, 0.25f, 0.30f, 1.0f);         // Hover highlight
    mColors[THEME_BG_SELECTED] = LLColor4(0.20f, 0.40f, 0.80f, 0.3f);      // Selection blue

    mColors[THEME_TEXT_PRIMARY] = LLColor4(0.90f, 0.90f, 0.95f, 1.0f);     // White text
    mColors[THEME_TEXT_SECONDARY] = LLColor4(0.60f, 0.60f, 0.70f, 1.0f);   // Gray text
    mColors[THEME_TEXT_DISABLED] = LLColor4(0.40f, 0.40f, 0.50f, 1.0f);    // Disabled text
    mColors[THEME_TEXT_HIGHLIGHT] = LLColor4(0.20f, 0.60f, 1.00f, 1.0f);   // Blue highlight

    mColors[THEME_ACCENT_PRIMARY] = LLColor4(0.00f, 0.60f, 1.00f, 1.0f);   // Electric blue
    mColors[THEME_ACCENT_SECONDARY] = LLColor4(0.80f, 0.20f, 0.80f, 1.0f); // Purple
    mColors[THEME_ACCENT_HOVER] = LLColor4(0.20f, 0.70f, 1.00f, 1.0f);     // Light blue
    mColors[THEME_ACCENT_ACTIVE] = LLColor4(0.00f, 0.40f, 0.80f, 1.0f);    // Dark blue

    mColors[THEME_BORDER] = LLColor4(0.30f, 0.30f, 0.40f, 0.8f);
    mColors[THEME_BORDER_FOCUSED] = LLColor4(0.00f, 0.60f, 1.00f, 1.0f);

    mColors[THEME_BUTTON_BG] = LLColor4(0.25f, 0.25f, 0.30f, 1.0f);
    mColors[THEME_BUTTON_BG_HOVER] = LLColor4(0.30f, 0.35f, 0.45f, 1.0f);
    mColors[THEME_BUTTON_BG_ACTIVE] = LLColor4(0.20f, 0.25f, 0.35f, 1.0f);
    mColors[THEME_BUTTON_TEXT] = LLColor4(0.90f, 0.90f, 0.95f, 1.0f);

    mColors[THEME_CHAT_BG] = LLColor4(0.08f, 0.08f, 0.10f, 1.0f);
    mColors[THEME_CHAT_TEXT] = LLColor4(0.90f, 0.90f, 0.95f, 1.0f);
    mColors[THEME_CHAT_SYSTEM] = LLColor4(0.80f, 0.80f, 0.40f, 1.0f);
    mColors[THEME_CHAT_SELF] = LLColor4(0.40f, 0.80f, 1.00f, 1.0f);
    mColors[THEME_CHAT_OTHERS] = LLColor4(0.90f, 0.90f, 0.95f, 1.0f);

    mColors[THEME_RADAR_BG] = LLColor4(0.10f, 0.10f, 0.12f, 0.95f);
    mColors[THEME_RADAR_TEXT] = LLColor4(0.90f, 0.90f, 0.95f, 1.0f);
    mColors[THEME_RADAR_HIGHLIGHT] = LLColor4(0.00f, 0.60f, 1.00f, 0.3f);

    mColors[THEME_INVENTORY_BG] = LLColor4(0.12f, 0.12f, 0.14f, 1.0f);
    mColors[THEME_INVENTORY_FOLDER] = LLColor4(0.80f, 0.70f, 0.40f, 1.0f);  // Gold
    mColors[THEME_INVENTORY_ITEM] = LLColor4(0.90f, 0.90f, 0.95f, 1.0f);

    mColors[THEME_GLOW] = LLColor4(0.00f, 0.60f, 1.00f, 0.5f);
    mColors[THEME_SHADOW] = LLColor4(0.00f, 0.00f, 0.00f, 0.5f);

    // Default opacity
    mOpacity.window_alpha = 0.98f;
    mOpacity.panel_alpha = 0.95f;
    mOpacity.control_alpha = 1.0f;
    mOpacity.border_alpha = 0.8f;
    mOpacity.shadow_alpha = 0.4f;
    mOpacity.glow_alpha = 0.5f;

    // Default glow
    mGlow.enabled = true;
    mGlow.intensity = 0.5f;
    mGlow.radius = 4.0f;
    mGlow.color = LLColor4(0.00f, 0.60f, 1.00f, 0.5f);
}

namespace
{
    LLControlGroup* get_config_group()
    {
        // LLUI owns the "config" settings group used throughout llui (e.g. EnableButtonFlashing).
        auto* ui = LLUI::getInstance();
        if (!ui) return nullptr;
        auto it = ui->mSettingGroups.find("config");
        if (it == ui->mSettingGroups.end()) return nullptr;
        return it->second;
    }

    F32 get_cfg_f32(LLControlGroup* cfg, const std::string& name, F32 fallback)
    {
        if (!cfg) return fallback;
        LLControlVariable* c = cfg->getControl(name);
        if (!c) return fallback;
        return (F32)c->getValue().asReal();
    }

    bool get_cfg_bool(LLControlGroup* cfg, const std::string& name, bool fallback)
    {
        if (!cfg) return fallback;
        LLControlVariable* c = cfg->getControl(name);
        if (!c) return fallback;
        return c->getValue().asBoolean();
    }

    const std::vector<std::string>& keys_for_element(EThemeElement element)
    {
        static const std::vector<std::string> kEmpty;
        static const std::vector<std::string> kBgPrimary = {"FloaterDefaultBackgroundColor", "MenuPopupBgColor", "floater_bg_color", "floater_bg_color_a"};
        static const std::vector<std::string> kBgSecondary = {"PanelDefaultBackgroundColor"};
        static const std::vector<std::string> kBgTertiary = {"ScrollBgWriteableColor", "ComboListBgColor", "TextBgWriteableColor"};
        static const std::vector<std::string> kBgHover = {"ScrollHoveredColor", "TextBgFocusColor"};
        static const std::vector<std::string> kBgSelected = {"ScrollSelectedBGColor", "TextBgHighlightColor", "MenuItemHighlightBgColor"};

        static const std::vector<std::string> kTextPrimary = {"TextFgColor", "LabelTextColor", "TextDefaultColor"};
        static const std::vector<std::string> kTextSecondary = {"TextFgReadOnlyColor"};
        static const std::vector<std::string> kTextDisabled = {"LabelDisabledColor", "ButtonLabelDisabledColor"};
        static const std::vector<std::string> kTextHighlight = {"LabelSelectedColor", "ButtonLabelSelectedColor"};

        static const std::vector<std::string> kAccentPrimary = {"AccentColor", "FocusColor", "DefaultHighlightLight", "SliderThumbCenterColor"};
        static const std::vector<std::string> kAccentSecondary = {"DefaultHighlightDark", "SliderThumbOutlineColor"};

        static const std::vector<std::string> kButtonBase = {"ButtonImageColor", "FloaterButtonImageColor"};
        static const std::vector<std::string> kButtonHover = {"FSDynamicThemeButtonImageColorHover"};
        static const std::vector<std::string> kButtonActive = {"FSDynamicThemeButtonImageColorActive"};
        static const std::vector<std::string> kButtonText = {"ButtonLabelColor"};

        static const std::vector<std::string> kChatBg = {"BackgroundChatColor", "FSChatHistoryBackground"};
        static const std::vector<std::string> kChatText = {"ChatHistoryTextColor"};
        static const std::vector<std::string> kChatSelf = {"ChatSelfMentionHighlight"};

        static const std::vector<std::string> kInventoryFolder = {"InventoryItemEnabledColor"};
        static const std::vector<std::string> kGlowColor = {"FSDynamicThemeGlowColor"};

        switch (element)
        {
        case THEME_BG_PRIMARY: return kBgPrimary;
        case THEME_BG_SECONDARY: return kBgSecondary;
        case THEME_BG_TERTIARY: return kBgTertiary;
        case THEME_BG_HOVER: return kBgHover;
        case THEME_BG_SELECTED: return kBgSelected;

        case THEME_TEXT_PRIMARY: return kTextPrimary;
        case THEME_TEXT_SECONDARY: return kTextSecondary;
        case THEME_TEXT_DISABLED: return kTextDisabled;
        case THEME_TEXT_HIGHLIGHT: return kTextHighlight;

        case THEME_ACCENT_PRIMARY: return kAccentPrimary;
        case THEME_ACCENT_SECONDARY: return kAccentSecondary;
        case THEME_ACCENT_HOVER: return kAccentPrimary;
        case THEME_ACCENT_ACTIVE: return kAccentPrimary;

        case THEME_BUTTON_BG: return kButtonBase;
        case THEME_BUTTON_BG_HOVER: return kButtonHover;
        case THEME_BUTTON_BG_ACTIVE: return kButtonActive;
        case THEME_BUTTON_TEXT: return kButtonText;

        case THEME_CHAT_BG: return kChatBg;
        case THEME_CHAT_TEXT: return kChatText;
        case THEME_CHAT_SELF: return kChatSelf;

        case THEME_INVENTORY_FOLDER: return kInventoryFolder;
        case THEME_GLOW: return kGlowColor;

        default: return kEmpty;
        }
    }

    bool uses_window_alpha(EThemeElement element)
    {
        return element == THEME_BG_PRIMARY;
    }

    bool uses_panel_alpha(EThemeElement element)
    {
        return element == THEME_BG_SECONDARY || element == THEME_BG_TERTIARY;
    }

    U64 fnv1a64(const std::string& s)
    {
        U64 h = 1469598103934665603ULL;
        for (unsigned char c : s)
        {
            h ^= static_cast<U64>(c);
            h *= 1099511628211ULL;
        }
        return h;
    }

    std::string hex_u64(U64 v)
    {
        static const char* kHex = "0123456789abcdef";
        std::string out(16, '0');
        for (int i = 15; i >= 0; --i)
        {
            out[i] = kHex[v & 0xF];
            v >>= 4;
        }
        return out;
    }
}

LLColor4 FSDynamicTheme::getColor(EThemeElement element) const
{
    if (element >= 0 && element < THEME_ELEMENT_COUNT)
    {
        return mColors[element];
    }
    return LLColor4::white;
}

LLColor4 FSDynamicTheme::getColorWithAlpha(EThemeElement element, F32 override_alpha) const
{
    LLColor4 color = getColor(element);
    if (override_alpha >= 0.0f)
    {
        color.mV[3] = override_alpha;
    }
    return color;
}

std::string FSDynamicTheme::buttonOverrideKey(const std::string& button_pathname, const char* suffix)
{
    // Keep keys short and filesystem-safe (they get persisted via LLUIColorTable).
    // Key format: FSDynamicThemeButtonOverride_<hash>_<suffix>
    return "FSDynamicThemeButtonOverride_" + hex_u64(fnv1a64(button_pathname)) + "_" + (suffix ? suffix : "");
}

void FSDynamicTheme::setButtonOverrideColor(const std::string& button_pathname, EThemeElement element, const LLColor4& color)
{
    if (button_pathname.empty()) return;

    const char* suffix = nullptr;
    switch (element)
    {
    case THEME_BUTTON_BG: suffix = "Base"; break;
    case THEME_BUTTON_BG_HOVER: suffix = "Hover"; break;
    case THEME_BUTTON_BG_ACTIVE: suffix = "Active"; break;
    case THEME_BUTTON_TEXT: suffix = "Text"; break;
    case THEME_GLOW: suffix = "Glow"; break;
    default: return;
    }

    LLUIColorTable::instance().setColor(buttonOverrideKey(button_pathname, suffix), color);
    LLUIColorTable::instance().saveUserSettingsPaletteOnly();
}

void FSDynamicTheme::clearButtonOverrides(const std::string& button_pathname)
{
    if (button_pathname.empty()) return;
    LLUIColorTable& table = LLUIColorTable::instance();

    table.resetToDefault(buttonOverrideKey(button_pathname, "Base"));
    table.resetToDefault(buttonOverrideKey(button_pathname, "Hover"));
    table.resetToDefault(buttonOverrideKey(button_pathname, "Active"));
    table.resetToDefault(buttonOverrideKey(button_pathname, "Text"));
    table.resetToDefault(buttonOverrideKey(button_pathname, "Glow"));

    table.saveUserSettingsPaletteOnly();
}

void FSDynamicTheme::setColor(EThemeElement element, const LLColor4& color)
{
    if (element >= 0 && element < THEME_ELEMENT_COUNT)
    {
        mColors[element] = color;

        // Keep glow settings in sync with the glow swatch.
        if (element == THEME_GLOW)
        {
            mGlow.color = color;
            LLUIColorTable::instance().setColor("FSDynamicThemeGlowColor", color);
        }

        notifyColorChanged(element);
        saveSettings();
    }
}

void FSDynamicTheme::setColor(EThemeElement element, F32 r, F32 g, F32 b, F32 a)
{
    setColor(element, LLColor4(r, g, b, a));
}

void FSDynamicTheme::setColorFromHex(EThemeElement element, const std::string& hex)
{
    // Parse #RRGGBB or #RRGGBBAA
    if (hex.empty()) return;

    std::string h = hex;
    if (h[0] == '#') h = h.substr(1);

    F32 r = 0, g = 0, b = 0, a = 1.0f;

    if (h.length() >= 6)
    {
        r = (F32)strtol(h.substr(0, 2).c_str(), NULL, 16) / 255.0f;
        g = (F32)strtol(h.substr(2, 2).c_str(), NULL, 16) / 255.0f;
        b = (F32)strtol(h.substr(4, 2).c_str(), NULL, 16) / 255.0f;

        if (h.length() >= 8)
        {
            a = (F32)strtol(h.substr(6, 2).c_str(), NULL, 16) / 255.0f;
        }
    }

    setColor(element, LLColor4(r, g, b, a));
}

void FSDynamicTheme::setOpacity(const FSThemeOpacity& opacity)
{
    mOpacity = opacity;
    notifyColorChanged(THEME_BG_PRIMARY);
    notifyColorChanged(THEME_BG_SECONDARY);
    notifyColorChanged(THEME_BG_TERTIARY);
    saveSettings();
}

void FSDynamicTheme::setWindowAlpha(F32 alpha)
{
    mOpacity.window_alpha = llclamp(alpha, 0.1f, 1.0f);
    notifyColorChanged(THEME_BG_PRIMARY);
    saveSettings();
}

void FSDynamicTheme::setPanelAlpha(F32 alpha)
{
    mOpacity.panel_alpha = llclamp(alpha, 0.1f, 1.0f);
    notifyColorChanged(THEME_BG_SECONDARY);
    notifyColorChanged(THEME_BG_TERTIARY);
    saveSettings();
}

void FSDynamicTheme::setGlowEnabled(bool enabled)
{
    mGlow.enabled = enabled;
    saveSettings();
}

void FSDynamicTheme::setGlowIntensity(F32 intensity)
{
    mGlow.intensity = llclamp(intensity, 0.0f, 1.0f);
    saveSettings();
}

void FSDynamicTheme::setGlowRadius(F32 radius)
{
    mGlow.radius = llmax(radius, 0.0f);
    saveSettings();
}

void FSDynamicTheme::setGlowColor(const LLColor4& color)
{
    mGlow.color = color;
    mColors[THEME_GLOW] = color;
    notifyColorChanged(THEME_GLOW);
    saveSettings();
}

void FSDynamicTheme::loadPreset(const std::string& preset_name)
{
    if (preset_name == "dark") applyDarkTheme();
    else if (preset_name == "light") applyLightTheme();
    else if (preset_name == "cyberpunk") applyCyberpunkTheme();
    else if (preset_name == "midnight") applyMidnightBlueTheme();
    else if (preset_name == "neon") applyNeonTheme();
    else
    {
        // Check custom presets
        auto it = mCustomPresets.find(preset_name);
        if (it != mCustomPresets.end())
        {
            for (int i = 0; i < THEME_ELEMENT_COUNT; i++)
            {
                mColors[i] = it->second.colors[i];
            }
            mOpacity = it->second.opacity;
            mGlow = it->second.glow;
        }
    }

    // Notify all elements changed
    for (int i = 0; i < THEME_ELEMENT_COUNT; i++)
    {
        notifyColorChanged((EThemeElement)i);
    }

    saveSettings();
}

void FSDynamicTheme::saveCurrentAsPreset(const std::string& name)
{
    FSThemePreset preset;
    preset.name = name;
    for (int i = 0; i < THEME_ELEMENT_COUNT; i++)
    {
        preset.colors[i] = mColors[i];
    }
    preset.opacity = mOpacity;
    preset.glow = mGlow;

    mCustomPresets[name] = preset;
    saveSettings();
}

std::vector<std::string> FSDynamicTheme::getAvailablePresets() const
{
    std::vector<std::string> presets = {"dark", "light", "cyberpunk", "midnight", "neon"};
    for (const auto& pair : mCustomPresets)
    {
        presets.push_back(pair.first);
    }
    return presets;
}

void FSDynamicTheme::deletePreset(const std::string& name)
{
    auto it = mCustomPresets.find(name);
    if (it != mCustomPresets.end())
    {
        mCustomPresets.erase(it);
        saveSettings();
    }
}

void FSDynamicTheme::applyDarkTheme()
{
    initDefaultColors();
    mCurrentPresetName = "dark";
}

void FSDynamicTheme::applyLightTheme()
{
    mColors[THEME_BG_PRIMARY] = LLColor4(0.95f, 0.95f, 0.97f, 1.0f);
    mColors[THEME_BG_SECONDARY] = LLColor4(0.90f, 0.90f, 0.93f, 1.0f);
    mColors[THEME_BG_TERTIARY] = LLColor4(1.00f, 1.00f, 1.00f, 1.0f);
    mColors[THEME_BG_HOVER] = LLColor4(0.85f, 0.90f, 0.95f, 1.0f);
    mColors[THEME_BG_SELECTED] = LLColor4(0.20f, 0.60f, 1.00f, 0.2f);

    mColors[THEME_TEXT_PRIMARY] = LLColor4(0.10f, 0.10f, 0.15f, 1.0f);
    mColors[THEME_TEXT_SECONDARY] = LLColor4(0.40f, 0.40f, 0.50f, 1.0f);
    mColors[THEME_TEXT_DISABLED] = LLColor4(0.60f, 0.60f, 0.70f, 1.0f);
    mColors[THEME_TEXT_HIGHLIGHT] = LLColor4(0.00f, 0.40f, 0.80f, 1.0f);

    mColors[THEME_ACCENT_PRIMARY] = LLColor4(0.00f, 0.50f, 0.90f, 1.0f);
    mColors[THEME_ACCENT_SECONDARY] = LLColor4(0.80f, 0.20f, 0.80f, 1.0f);

    mColors[THEME_BUTTON_BG] = LLColor4(0.85f, 0.85f, 0.90f, 1.0f);
    mColors[THEME_BUTTON_BG_HOVER] = LLColor4(0.80f, 0.85f, 0.90f, 1.0f);
    mColors[THEME_BUTTON_BG_ACTIVE] = LLColor4(0.75f, 0.80f, 0.85f, 1.0f);
    mColors[THEME_BUTTON_TEXT] = LLColor4(0.10f, 0.10f, 0.15f, 1.0f);

    mColors[THEME_CHAT_BG] = LLColor4(0.98f, 0.98f, 0.99f, 1.0f);
    mColors[THEME_CHAT_TEXT] = LLColor4(0.10f, 0.10f, 0.15f, 1.0f);
    mCurrentPresetName = "light";
}

void FSDynamicTheme::applyCyberpunkTheme()
{
    mColors[THEME_BG_PRIMARY] = LLColor4(0.05f, 0.02f, 0.08f, 1.0f);      // Deep purple-black
    mColors[THEME_BG_SECONDARY] = LLColor4(0.10f, 0.05f, 0.15f, 1.0f);
    mColors[THEME_BG_TERTIARY] = LLColor4(0.15f, 0.08f, 0.20f, 1.0f);

    mColors[THEME_ACCENT_PRIMARY] = LLColor4(1.00f, 0.00f, 0.80f, 1.0f);  // Hot pink
    mColors[THEME_ACCENT_SECONDARY] = LLColor4(0.00f, 1.00f, 0.80f, 1.0f); // Cyan
    mColors[THEME_ACCENT_HOVER] = LLColor4(1.00f, 0.20f, 0.90f, 1.0f);
    mColors[THEME_ACCENT_ACTIVE] = LLColor4(0.80f, 0.00f, 0.60f, 1.0f);

    mColors[THEME_TEXT_PRIMARY] = LLColor4(0.90f, 0.90f, 0.95f, 1.0f);
    mColors[THEME_TEXT_HIGHLIGHT] = LLColor4(1.00f, 0.50f, 0.90f, 1.0f);

    mColors[THEME_BORDER_FOCUSED] = LLColor4(0.00f, 1.00f, 0.80f, 1.0f);

    mColors[THEME_BUTTON_BG] = LLColor4(0.20f, 0.10f, 0.30f, 1.0f);
    mColors[THEME_BUTTON_BG_HOVER] = LLColor4(0.30f, 0.15f, 0.45f, 1.0f);

    mGlow.enabled = true;
    mGlow.color = LLColor4(1.00f, 0.00f, 0.80f, 0.6f);
    mGlow.intensity = 0.7f;
    mCurrentPresetName = "cyberpunk";
}

void FSDynamicTheme::applyMidnightBlueTheme()
{
    mColors[THEME_BG_PRIMARY] = LLColor4(0.02f, 0.05f, 0.12f, 1.0f);      // Deep blue-black
    mColors[THEME_BG_SECONDARY] = LLColor4(0.05f, 0.10f, 0.20f, 1.0f);
    mColors[THEME_BG_TERTIARY] = LLColor4(0.08f, 0.15f, 0.28f, 1.0f);

    mColors[THEME_ACCENT_PRIMARY] = LLColor4(0.20f, 0.60f, 1.00f, 1.0f);  // Bright blue
    mColors[THEME_ACCENT_SECONDARY] = LLColor4(0.40f, 0.80f, 1.00f, 1.0f); // Light blue
    mColors[THEME_ACCENT_HOVER] = LLColor4(0.40f, 0.70f, 1.00f, 1.0f);
    mColors[THEME_ACCENT_ACTIVE] = LLColor4(0.10f, 0.50f, 0.90f, 1.0f);

    mColors[THEME_TEXT_PRIMARY] = LLColor4(0.85f, 0.90f, 0.95f, 1.0f);
    mColors[THEME_TEXT_HIGHLIGHT] = LLColor4(0.40f, 0.80f, 1.00f, 1.0f);

    mColors[THEME_CHAT_SELF] = LLColor4(0.20f, 0.70f, 1.00f, 1.0f);

    mGlow.color = LLColor4(0.20f, 0.60f, 1.00f, 0.5f);
    mCurrentPresetName = "midnight";
}

void FSDynamicTheme::applyNeonTheme()
{
    mColors[THEME_BG_PRIMARY] = LLColor4(0.00f, 0.00f, 0.00f, 1.0f);      // Pure black
    mColors[THEME_BG_SECONDARY] = LLColor4(0.05f, 0.05f, 0.05f, 1.0f);

    mColors[THEME_ACCENT_PRIMARY] = LLColor4(0.00f, 1.00f, 0.00f, 1.0f);  // Neon green
    mColors[THEME_ACCENT_SECONDARY] = LLColor4(1.00f, 0.00f, 1.00f, 1.0f); // Neon pink
    mColors[THEME_ACCENT_HOVER] = LLColor4(0.50f, 1.00f, 0.50f, 1.0f);
    mColors[THEME_ACCENT_ACTIVE] = LLColor4(0.00f, 0.80f, 0.00f, 1.0f);

    mColors[THEME_TEXT_PRIMARY] = LLColor4(0.00f, 1.00f, 0.00f, 1.0f);
    mColors[THEME_TEXT_SECONDARY] = LLColor4(0.50f, 0.80f, 0.50f, 1.0f);
    mColors[THEME_TEXT_HIGHLIGHT] = LLColor4(1.00f, 0.00f, 1.00f, 1.0f);

    mColors[THEME_BORDER_FOCUSED] = LLColor4(0.00f, 1.00f, 0.00f, 1.0f);

    mColors[THEME_BUTTON_BG] = LLColor4(0.10f, 0.10f, 0.10f, 1.0f);
    mColors[THEME_BUTTON_BG_HOVER] = LLColor4(0.20f, 0.30f, 0.20f, 1.0f);
    mColors[THEME_BUTTON_TEXT] = LLColor4(0.00f, 1.00f, 0.00f, 1.0f);

    mColors[THEME_CHAT_BG] = LLColor4(0.00f, 0.00f, 0.00f, 1.0f);
    mColors[THEME_CHAT_TEXT] = LLColor4(0.00f, 1.00f, 0.00f, 1.0f);

    mGlow.enabled = true;
    mGlow.color = LLColor4(0.00f, 1.00f, 0.00f, 0.6f);
    mGlow.intensity = 0.8f;
    mCurrentPresetName = "neon";
}

S32 FSDynamicTheme::addChangeCallback(theme_change_callback_t callback)
{
    S32 id = mNextCallbackId++;
    mCallbacks[id] = callback;
    return id;
}

void FSDynamicTheme::removeChangeCallback(S32 id)
{
    mCallbacks.erase(id);
}

void FSDynamicTheme::notifyColorChanged(EThemeElement element)
{
    // Apply to LLUIColorTable so existing controls update live (LLUIColor holds pointers to table entries).
    const std::vector<std::string>& keys = keys_for_element(element);
    if (!keys.empty())
    {
        LLColor4 applied = mColors[element];
        if (uses_window_alpha(element))
        {
            applied.mV[3] *= mOpacity.window_alpha;
        }
        else if (uses_panel_alpha(element))
        {
            applied.mV[3] *= mOpacity.panel_alpha;
        }

        for (const std::string& key : keys)
        {
            LLUIColorTable::instance().setColor(key, applied);
        }
    }

    for (auto& pair : mCallbacks)
    {
        if (pair.second)
        {
            pair.second(element, mColors[element]);
        }
    }
}

void FSDynamicTheme::applyToFloater(LLFloater* floater)
{
    if (!floater) return;

    // Apply background color
    floater->setBackgroundColor(mColors[THEME_BG_PRIMARY]);
}

void FSDynamicTheme::applyToPanel(LLPanel* panel)
{
    if (!panel) return;

    panel->setBackgroundColor(mColors[THEME_BG_SECONDARY]);
}

void FSDynamicTheme::applyToButton(LLButton* button)
{
    (void)button;
    // Button styling is applied via LLUIColorTable + LLButton runtime support (see llbutton.cpp).
}

void FSDynamicTheme::applyToScroller(LLScrollContainer* scroller)
{
    // Scroller color methods not available in this version
    // Stub for future implementation
}

void FSDynamicTheme::resetToDefaults()
{
    initDefaultColors();

    for (int i = 0; i < THEME_ELEMENT_COUNT; i++)
    {
        notifyColorChanged((EThemeElement)i);
    }

    saveSettings();
}

void FSDynamicTheme::saveSettings()
{
    // Persist knobs to the "config" settings group so the theme editor works without restart.
    LLControlGroup* cfg = get_config_group();
    if (cfg)
    {
        if (LLControlVariable* c = cfg->getControl("FSDynamicThemeWindowAlpha"))
            c->setValue(mOpacity.window_alpha, true);
        if (LLControlVariable* c = cfg->getControl("FSDynamicThemePanelAlpha"))
            c->setValue(mOpacity.panel_alpha, true);
        if (LLControlVariable* c = cfg->getControl("FSDynamicThemeGlowEnabled"))
            c->setValue(mGlow.enabled, true);
        if (LLControlVariable* c = cfg->getControl("FSDynamicThemeGlowIntensity"))
            c->setValue(mGlow.intensity, true);
        if (LLControlVariable* c = cfg->getControl("FSDynamicThemeGlowRadius"))
            c->setValue(mGlow.radius, true);
    }

    // Persist user colors via LLUIColorTable. This is the same mechanism used by the skin color editor.
    LLUIColorTable::instance().saveUserSettingsPaletteOnly();
}

void FSDynamicTheme::loadSettings()
{
    // Prime from current skin colors so enabling the system doesn't unexpectedly recolor everything.
    for (int i = 0; i < THEME_ELEMENT_COUNT; i++)
    {
        const std::vector<std::string>& keys = keys_for_element((EThemeElement)i);
        if (!keys.empty())
        {
            mColors[i] = LLUIColorTable::instance().getColor(keys.front(), mColors[i]).get();
        }
    }

    // If the optional dynamic keys aren't present yet, derive sensible defaults from existing skin colors.
    if (!LLUIColorTable::instance().colorExists("FSDynamicThemeGlowColor"))
    {
        LLColor4 fallback = LLUIColorTable::instance().getColor("FocusColor", mGlow.color).get();
        LLUIColorTable::instance().setColor("FSDynamicThemeGlowColor", fallback);
    }
    if (!LLUIColorTable::instance().colorExists("FSDynamicThemeButtonImageColorHover"))
    {
        LLColor4 base = LLUIColorTable::instance().getColor("ButtonImageColor", mColors[THEME_BUTTON_BG]).get();
        LLUIColorTable::instance().setColor("FSDynamicThemeButtonImageColorHover", base);
    }
    if (!LLUIColorTable::instance().colorExists("FSDynamicThemeButtonImageColorActive"))
    {
        LLColor4 base = LLUIColorTable::instance().getColor("ButtonImageColor", mColors[THEME_BUTTON_BG]).get();
        LLUIColorTable::instance().setColor("FSDynamicThemeButtonImageColorActive", base);
    }

    // Now read the derived/loaded dynamic keys back into our state.
    mGlow.color = LLUIColorTable::instance().getColor("FSDynamicThemeGlowColor", mGlow.color).get();
    mColors[THEME_GLOW] = mGlow.color;
    mColors[THEME_BUTTON_BG_HOVER] = LLUIColorTable::instance().getColor("FSDynamicThemeButtonImageColorHover", mColors[THEME_BUTTON_BG]).get();
    mColors[THEME_BUTTON_BG_ACTIVE] = LLUIColorTable::instance().getColor("FSDynamicThemeButtonImageColorActive", mColors[THEME_BUTTON_BG]).get();

    LLControlGroup* cfg = get_config_group();
    mOpacity.window_alpha = llclamp(get_cfg_f32(cfg, "FSDynamicThemeWindowAlpha", mOpacity.window_alpha), 0.1f, 1.0f);
    mOpacity.panel_alpha = llclamp(get_cfg_f32(cfg, "FSDynamicThemePanelAlpha", mOpacity.panel_alpha), 0.1f, 1.0f);
    mGlow.enabled = get_cfg_bool(cfg, "FSDynamicThemeGlowEnabled", mGlow.enabled);
    mGlow.intensity = llclamp(get_cfg_f32(cfg, "FSDynamicThemeGlowIntensity", mGlow.intensity), 0.0f, 1.0f);
    mGlow.radius = llclamp(get_cfg_f32(cfg, "FSDynamicThemeGlowRadius", mGlow.radius), 0.0f, 20.0f);
}

std::string FSDynamicTheme::exportToXML()
{
    std::stringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<theme name=\"Custom\" version=\"1.0\">\n";

    for (int i = 0; i < THEME_ELEMENT_COUNT; i++)
    {
        xml << "  <color element=\"" << i << "\" ";
        xml << "r=\"" << mColors[i].mV[0] << "\" ";
        xml << "g=\"" << mColors[i].mV[1] << "\" ";
        xml << "b=\"" << mColors[i].mV[2] << "\" ";
        xml << "a=\"" << mColors[i].mV[3] << "\"/>\n";
    }

    xml << "  <opacity window=\"" << mOpacity.window_alpha << "\" ";
    xml << "panel=\"" << mOpacity.panel_alpha << "\"/>\n";

    xml << "  <glow enabled=\"" << (mGlow.enabled ? "true" : "false") << "\" ";
    xml << "intensity=\"" << mGlow.intensity << "\" ";
    xml << "radius=\"" << mGlow.radius << "\" ";
    xml << "r=\"" << mGlow.color.mV[0] << "\" g=\"" << mGlow.color.mV[1] << "\" b=\"" << mGlow.color.mV[2] << "\" a=\"" << mGlow.color.mV[3] << "\"/>\n";

    xml << "</theme>\n";

    return xml.str();
}

bool FSDynamicTheme::importFromXML(const std::string& xml)
{
    // Simple XML parsing - in production use proper XML parser
    // This is a basic implementation

    size_t pos = 0;
    while ((pos = xml.find("<color ", pos)) != std::string::npos)
    {
        size_t end = xml.find("/>", pos);
        if (end == std::string::npos) break;

        std::string element_str = xml.substr(pos, end - pos);

        // Parse attributes
        int elem = -1;
        F32 r = 0, g = 0, b = 0, a = 1.0f;

        size_t e_pos = element_str.find("element=\"");
        if (e_pos != std::string::npos)
        {
            elem = atoi(element_str.c_str() + e_pos + 9);
        }

        size_t r_pos = element_str.find("r=\"");
        if (r_pos != std::string::npos)
        {
            r = (F32)atof(element_str.c_str() + r_pos + 3);
        }

        size_t g_pos = element_str.find("g=\"");
        if (g_pos != std::string::npos)
        {
            g = (F32)atof(element_str.c_str() + g_pos + 3);
        }

        size_t b_pos = element_str.find("b=\"");
        if (b_pos != std::string::npos)
        {
            b = (F32)atof(element_str.c_str() + b_pos + 3);
        }

        size_t a_pos = element_str.find("a=\"");
        if (a_pos != std::string::npos)
        {
            a = (F32)atof(element_str.c_str() + a_pos + 3);
        }

        if (elem >= 0 && elem < THEME_ELEMENT_COUNT)
        {
            mColors[elem] = LLColor4(r, g, b, a);
        }

        pos = end + 2;
    }

    // Opacity block: <opacity window="..." panel="..."/>
    {
        size_t op_pos = xml.find("<opacity ");
        if (op_pos != std::string::npos)
        {
            size_t end = xml.find("/>", op_pos);
            if (end != std::string::npos)
            {
                std::string op_str = xml.substr(op_pos, end - op_pos);
                size_t w_pos = op_str.find("window=\"");
                if (w_pos != std::string::npos)
                {
                    mOpacity.window_alpha = llclamp((F32)atof(op_str.c_str() + w_pos + 8), 0.1f, 1.0f);
                }
                size_t p_pos = op_str.find("panel=\"");
                if (p_pos != std::string::npos)
                {
                    mOpacity.panel_alpha = llclamp((F32)atof(op_str.c_str() + p_pos + 7), 0.1f, 1.0f);
                }
            }
        }
    }

    // Glow block: <glow enabled="true|false" intensity="..." radius="..." r="..." g="..." b="..." a="..."/>
    {
        size_t g_pos = xml.find("<glow ");
        if (g_pos != std::string::npos)
        {
            size_t end = xml.find("/>", g_pos);
            if (end != std::string::npos)
            {
                std::string g_str = xml.substr(g_pos, end - g_pos);

                size_t e_pos = g_str.find("enabled=\"");
                if (e_pos != std::string::npos)
                {
                    std::string enabled_str = g_str.substr(e_pos + 9, 5);
                    mGlow.enabled = (enabled_str.find("true") != std::string::npos);
                }

                size_t i_pos = g_str.find("intensity=\"");
                if (i_pos != std::string::npos)
                {
                    mGlow.intensity = llclamp((F32)atof(g_str.c_str() + i_pos + 11), 0.0f, 1.0f);
                }

                size_t r_pos = g_str.find("radius=\"");
                if (r_pos != std::string::npos)
                {
                    mGlow.radius = llclamp((F32)atof(g_str.c_str() + r_pos + 8), 0.0f, 20.0f);
                }

                F32 cr = mGlow.color.mV[0];
                F32 cg = mGlow.color.mV[1];
                F32 cb = mGlow.color.mV[2];
                F32 ca = mGlow.color.mV[3];

                size_t cr_pos = g_str.find("r=\"");
                if (cr_pos != std::string::npos) cr = (F32)atof(g_str.c_str() + cr_pos + 3);
                size_t cg_pos = g_str.find("g=\"");
                if (cg_pos != std::string::npos) cg = (F32)atof(g_str.c_str() + cg_pos + 3);
                size_t cb_pos = g_str.find("b=\"");
                if (cb_pos != std::string::npos) cb = (F32)atof(g_str.c_str() + cb_pos + 3);
                size_t ca_pos = g_str.find("a=\"");
                if (ca_pos != std::string::npos) ca = (F32)atof(g_str.c_str() + ca_pos + 3);

                mGlow.color = LLColor4(cr, cg, cb, ca);
                mColors[THEME_GLOW] = mGlow.color;
            }
        }
    }

    // Notify all changes
    for (int i = 0; i < THEME_ELEMENT_COUNT; i++)
    {
        notifyColorChanged((EThemeElement)i);
    }

    saveSettings();
    return true;
}
