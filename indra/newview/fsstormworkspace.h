#pragma once
#include "lluuid.h"

// Firestorm UI routing only. Legacy views remain backing controllers, not windows.
namespace FSStormWorkspace
{
    bool enabled();
    bool showPhotoMode();
    bool showAppearance(bool shape);
    bool showTerrainBrush();
    bool showLand();
    bool showChat();
    bool chatVisible();
    bool showSearch(bool groups = false);
    bool searchVisible();
    bool showActionNotification(const LLUUID& id);
    bool showScriptNotification(const LLUUID& id);
    bool showConversation(const LLUUID& session);
    bool showInventoryItem(const LLUUID& item, bool open = true);
    bool pinInventoryFolder(const LLUUID& folder);
    bool dropInventoryFolder(const LLUUID& folder, int x, int y, bool drop);
    void remapConversation(const LLUUID& old_id, const LLUUID& new_id);
    void hideLegacyWindows();
}
