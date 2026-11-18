#include "linden_common.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llgltexture.h"
#include "llimage.h"
#include "llimagegl.h"
#include "llrender.h"

#include "nyxui.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include <map>

// The interface renderer. It draws ImGui's lists through LL's immediate-mode renderer (gGL) with
// the UI shader and binds LLImageGL textures, so it has no graphics-API code of its own and runs
// on whatever backend LL's renderer is using.

namespace
{
    // Textures NyxUI created (font atlas, logo, icon atlases, backgrounds), kept alive here.
    std::map<LLImageGL*, LLPointer<LLImageGL>> sOwned;
    // Replaced textures stay alive a few frames: the GPU may still be reading them.
    struct Retired
    {
        LLPointer<LLImageGL> image;
        int frames = 0;
    };
    std::vector<Retired> sRetired;
    bool sReady = false;

    LLImageGL* imageOf(NyxUI::Tex tex)
    {
        return reinterpret_cast<LLImageGL*>((intptr_t)tex);
    }

    NyxUI::Tex handleOf(LLImageGL* image)
    {
        return (NyxUI::Tex)(intptr_t)image;
    }

    bool upload(LLImageGL* image, int width, int height, const void* rgba)
    {
        if (!image || width <= 0 || height <= 0 || !rgba) return false;
        LLPointer<LLImageRaw> raw = new LLImageRaw((U16)width, (U16)height, 4);
        if (!raw->getData()) return false;
        memcpy(raw->getData(), rgba, (size_t)width * height * 4);
        // LLImageGL only takes power-of-two sizes. Callers always map the whole image (UV 0..1),
        // so stretching it to the next power of two changes nothing they see.
        if (!LLImageGL::checkSize(width, height))
        {
            auto pot = [](int v) { int p = 1; while (p < v && p < 8192) p <<= 1; return p; };
            raw->scale(pot(width), pot(height));
        }
        return image->createGLTexture(0, raw);
    }

    void syncImGuiTexture(ImTextureData* tex)
    {
        if (tex->Status == ImTextureStatus_WantCreate)
        {
            tex->SetTexID(NyxUI::createTexture(tex->Width, tex->Height, tex->GetPixels(), false));
            tex->SetStatus(ImTextureStatus_OK);
        }
        else if (tex->Status == ImTextureStatus_WantUpdates)
        {
            // New glyphs: upload the whole page as a new texture and point ImGui at it (draw
            // commands resolve the id through the texture data, so this frame uses the new one).
            // Re-creating an existing texture in place doesn't take under every backend.
            const NyxUI::Tex fresh = NyxUI::createTexture(tex->Width, tex->Height, tex->GetPixels(), false);
            if (fresh)
            {
                NyxUI::destroyTexture(tex->GetTexID());
                tex->SetTexID(fresh);
            }
            tex->SetStatus(ImTextureStatus_OK);
        }
        else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
        {
            NyxUI::destroyTexture(tex->GetTexID());
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
    }
}

namespace NyxUI
{
    Tex createTexture(int width, int height, const void* rgba, bool mipmaps)
    {
        LLPointer<LLImageGL> image = new LLImageGL(mipmaps, false);
        if (!upload(image, width, height, rgba)) return 0;
        image->setFilteringOption(mipmaps ? LLTexUnit::TFO_TRILINEAR : LLTexUnit::TFO_BILINEAR);
        image->setAddressMode(LLTexUnit::TAM_CLAMP);
        sOwned[image.get()] = image;
        return handleOf(image.get());
    }

    Tex updateTexture(Tex tex, int width, int height, const void* rgba)
    {
        const Tex fresh = createTexture(width, height, rgba, false);
        if (!fresh) return tex;
        destroyTexture(tex);
        return fresh;
    }

    void destroyTexture(Tex tex)
    {
        auto it = sOwned.find(imageOf(tex));
        if (it == sOwned.end()) return;
        sRetired.push_back({ it->second, 0 });
        sOwned.erase(it);
    }

    Tex textureHandle(const LLGLTexture* texture)
    {
        if (!texture || !texture->hasGLTexture()) return 0;
        return textureHandle(texture->getGLTexture());
    }

    Tex textureHandle(LLImageGL* image)
    {
        return image && image->getHasGLTexture() ? handleOf(image) : 0;
    }

    ImVec2 textureSize(Tex tex)
    {
        const LLImageGL* image = imageOf(tex);
        return image ? ImVec2((float)image->getWidth(), (float)image->getHeight()) : ImVec2(0.f, 0.f);
    }

    bool glInit()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererName = "nyxui_llrender";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
        sReady = true;
        return true;
    }

    void glShutdown()
    {
        if (ImGui::GetCurrentContext())
        {
            for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
            {
                if (tex->RefCount == 1 && tex->GetTexID() != ImTextureID_Invalid)
                {
                    tex->SetTexID(ImTextureID_Invalid);
                    tex->SetStatus(ImTextureStatus_Destroyed);
                }
            }
        }
        sOwned.clear();
        sReady = false;
    }

    void glRender(ImDrawData* dd, int fb_w, int fb_h)
    {
        if (!sReady || !dd || fb_w <= 0 || fb_h <= 0) return;
        for (Retired& r : sRetired) ++r.frames;
        sRetired.erase(std::remove_if(sRetired.begin(), sRetired.end(), [](const Retired& r) { return r.frames > 4; }), sRetired.end());
        if (dd->Textures)
        {
            for (ImTextureData* tex : *dd->Textures)
            {
                if (tex->Status != ImTextureStatus_OK) syncImGuiTexture(tex);
            }
        }
        if (dd->CmdListsCount == 0) return;

        gGL.flush();
        LLGLSLShader* prev_shader = LLGLSLShader::sCurBoundShaderPtr;
        gUIProgram.bind();
        gGL.setViewport(0, 0, fb_w, fb_h);
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.pushMatrix();
        gGL.loadIdentity();
        // y down, origin at the top left of the display, as ImGui lays things out.
        gGL.ortho(dd->DisplayPos.x, dd->DisplayPos.x + dd->DisplaySize.x, dd->DisplayPos.y + dd->DisplaySize.y, dd->DisplayPos.y, -1.f, 1.f);
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.pushMatrix();
        gGL.loadIdentity();
        {
            LLGLDepthTest no_depth(GL_FALSE, GL_FALSE);
            LLGLDisable no_cull(GL_CULL_FACE);
            LLGLEnable scissor(GL_SCISSOR_TEST);
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
            gGL.color4f(1.f, 1.f, 1.f, 1.f);

            const ImVec2 clip_off = dd->DisplayPos;
            const ImVec2 clip_scale = dd->FramebufferScale;
            LLImageGL* bound = nullptr;
            for (const ImDrawList* list : dd->CmdLists)
            {
                const ImDrawVert* vtx = list->VtxBuffer.Data;
                const ImDrawIdx* idx = list->IdxBuffer.Data;
                for (const ImDrawCmd& cmd : list->CmdBuffer)
                {
                    if (cmd.UserCallback)
                    {
                        if (cmd.UserCallback != ImDrawCallback_ResetRenderState) cmd.UserCallback(list, &cmd);
                        // A callback (an embedded 3D view, a preview) may bind its own shader, viewport
                        // and matrices; put the UI's back before the next batch.
                        gGL.flush();
                        gUIProgram.bind();
                        gGL.setViewport(0, 0, fb_w, fb_h);
                        gGL.matrixMode(LLRender::MM_PROJECTION);
                        gGL.loadIdentity();
                        gGL.ortho(dd->DisplayPos.x, dd->DisplayPos.x + dd->DisplaySize.x, dd->DisplayPos.y + dd->DisplaySize.y, dd->DisplayPos.y, -1.f, 1.f);
                        gGL.matrixMode(LLRender::MM_MODELVIEW);
                        gGL.loadIdentity();
                        gGL.setSceneBlendType(LLRender::BT_ALPHA);
                        bound = nullptr;
                        continue;
                    }
                    const ImVec2 cmin((cmd.ClipRect.x - clip_off.x) * clip_scale.x, (cmd.ClipRect.y - clip_off.y) * clip_scale.y);
                    const ImVec2 cmax((cmd.ClipRect.z - clip_off.x) * clip_scale.x, (cmd.ClipRect.w - clip_off.y) * clip_scale.y);
                    if (cmax.x <= cmin.x || cmax.y <= cmin.y || cmd.ElemCount == 0) continue;
                    LLImageGL* image = imageOf(cmd.GetTexID());
                    if (!image || !image->getHasGLTexture()) continue;

                    gGL.flush();
                    // LL's scissor takes rows from the bottom of the target on every backend.
                    gGL.setScissor((S32)cmin.x, (S32)((float)fb_h - cmax.y), (S32)(cmax.x - cmin.x), (S32)(cmax.y - cmin.y));
                    if (image != bound)
                    {
                        gGL.getTexUnit(0)->bind(image);
                        bound = image;
                    }
                    // Whole triangles in batches below gGL's own mid-batch flush point (2048), which
                    // loses the earlier part of a batch under the Vulkan backend.
                    constexpr U32 kBatch = 2040;
                    for (U32 start = 0; start < cmd.ElemCount; start += kBatch)
                    {
                        const U32 end = std::min(cmd.ElemCount, start + kBatch);
                        gGL.begin(LLRender::TRIANGLES);
                        for (U32 i = start; i < end; ++i)
                        {
                            const ImDrawVert& v = vtx[cmd.VtxOffset + idx[cmd.IdxOffset + i]];
                            gGL.texCoord2f(v.uv.x, v.uv.y);
                            gGL.color4ub((U8)(v.col >> IM_COL32_R_SHIFT), (U8)(v.col >> IM_COL32_G_SHIFT), (U8)(v.col >> IM_COL32_B_SHIFT), (U8)(v.col >> IM_COL32_A_SHIFT));
                            gGL.vertex2f(v.pos.x, v.pos.y);
                        }
                        gGL.end();
                        gGL.flush();
                    }
                }
            }
            gGL.flush();
            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        }
        gGL.setScissor(0, 0, fb_w, fb_h);
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.popMatrix();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.popMatrix();
        gUIProgram.unbind();
        if (prev_shader) prev_shader->bind();
    }
}
