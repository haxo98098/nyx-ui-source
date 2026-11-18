#pragma once

#include "nyxui.h"
#include "imgui_internal.h"

#include <memory>
#include <string>
#include <vector>

namespace NyxUI
{
    namespace Dock
    {
        struct Node
        {
            bool leaf = true;
            bool horizontal = true;
            float ratio = 0.5f;
            std::unique_ptr<Node> a;
            std::unique_ptr<Node> b;
            std::vector<std::string> tabs;
            int active = 0;
            // Tabs drawn as large icon-over-label tiles; written as a leading '*' in a layout ("[*people|chat]").
            bool tiles = false;
            ImRect rect;
        };

        struct Floating
        {
            std::string id;
            ImVec2 pos;
            ImVec2 size;
            bool placed = false;
        };

        struct Layout
        {
            std::unique_ptr<Node> root;
            std::vector<Floating> floating;
        };

        std::unique_ptr<Node> parse(const std::string& text);
        std::string serialize(const Node* n);

        Node* findLeaf(Node* n, const std::string& panel);
        Node* largestLeaf(Node* n, bool allow_viewport);
        bool contains(const Layout& l, const std::string& panel);
        void removePanel(Layout& l, const std::string& panel);
        void dockPanel(Layout& l, const std::string& panel);
        void floatPanel(Layout& l, const std::string& panel);

        void draw(Layout& l, const ImRect& area);
        bool viewport(ImRect& out);
    }
}
