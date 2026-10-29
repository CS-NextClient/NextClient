#include "performance/PerformancePanel.h"

#include <algorithm>
#include <cmath>

#include <strtools.h>
#include <glad/glad.h>
#include <vgui/ILocalize.h>

#include "performance/PerformanceMapper.h"
#include "performance/PerformanceNetwork.h"

namespace performance
{

    namespace
    {
        constexpr ncl_perf::PerfDuration_t kLayoutRefreshInterval = 250000000;

        class MapOutlineGlState
        {
        private:
            GLint program_{};
            GLint active_texture_{};
            GLint matrix_mode_{};
            GLint draw_framebuffer_{};

        public:
            explicit MapOutlineGlState(const PerfMapRenderView& view)
            {
                glGetIntegerv(GL_MATRIX_MODE, &matrix_mode_);
                if (glUseProgram)
                {
                    glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
                }
                if (glActiveTexture)
                {
                    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_);
                }
                if (glBindFramebuffer)
                {
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
                }
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                if (glBindFramebuffer)
                {
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(view.draw_framebuffer));
                }
                if (glUseProgram)
                {
                    glUseProgram(0);
                }
                glViewport(view.viewport[0], view.viewport[1], view.viewport[2], view.viewport[3]);
                glScissor(view.viewport[0], view.viewport[1], view.viewport[2], view.viewport[3]);
                glEnable(GL_SCISSOR_TEST);
                GLint units = 1;
                if (glActiveTexture)
                {
                    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
                }
                for (int unit = 0; unit < units; ++unit)
                {
                    if (glActiveTexture)
                    {
                        glActiveTexture(GL_TEXTURE0 + unit);
                    }
                    glDisable(GL_TEXTURE_1D);
                    glDisable(GL_TEXTURE_2D);
                    if (GLAD_GL_VERSION_1_2)
                    {
                        glDisable(GL_TEXTURE_3D);
                    }
                    if (GLAD_GL_VERSION_1_3)
                    {
                        glDisable(GL_TEXTURE_CUBE_MAP);
                    }
                }
                for (GLenum capability :
                     {GL_ALPHA_TEST, GL_CULL_FACE, GL_LIGHTING, GL_FOG, GL_STENCIL_TEST, GL_BLEND, GL_LINE_SMOOTH, GL_LINE_STIPPLE})
                {
                    glDisable(capability);
                }
                for (GLenum plane = GL_CLIP_PLANE0; plane <= GL_CLIP_PLANE5; ++plane)
                {
                    glDisable(plane);
                }
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GLenum(view.depth_function));
                glDepthRange(view.depth_range[0], view.depth_range[1]);
                glDepthMask(GL_FALSE);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                GLfloat widths[2]{};
                glGetFloatv(GL_LINE_WIDTH_RANGE, widths);
                glLineWidth(std::min(2.0f, widths[1]));
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadMatrixf(view.projection.data());
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadMatrixf(view.modelview.data());
            }

            ~MapOutlineGlState()
            {
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                // Draw-buffer attributes belong to the framebuffer that supplied them.
                if (glBindFramebuffer)
                {
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer_);
                }
                glPopAttrib();
                if (glUseProgram)
                {
                    glUseProgram(program_);
                }
                if (glActiveTexture)
                {
                    glActiveTexture(active_texture_);
                }
                glMatrixMode(matrix_mode_);
            }
        };

        class PanelGlState
        {
        private:
            GLint program_{};
            GLint matrix_mode_{};
            GLint active_texture_{};
            GLint read_framebuffer_{};
            GLint draw_framebuffer_{};
            GLint default_draw_buffer_{};
            int* current_texture_{};
            int engine_texture_{};

        public:
            PanelGlState(int width, int height, int* current_texture) :
                current_texture_(current_texture)
            {
                if (glUseProgram)
                {
                    glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
                }
                glGetIntegerv(GL_MATRIX_MODE, &matrix_mode_);
                if (glActiveTexture)
                {
                    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_);
                }
                if (glBindFramebuffer)
                {
                    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
                }
                engine_texture_ = current_texture_ ? *current_texture_ : 0;

                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

                if (glBindFramebuffer)
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glGetIntegerv(GL_DRAW_BUFFER, &default_draw_buffer_);
                }

                GLboolean double_buffer = GL_FALSE;
                glGetBooleanv(GL_DOUBLEBUFFER, &double_buffer);
                glDrawBuffer(double_buffer ? GL_BACK : GL_FRONT);
                if (glUseProgram)
                {
                    glUseProgram(0);
                }
                glViewport(0, 0, width, height);

                GLint units = 1;
                if (glActiveTexture)
                {
                    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
                }

                for (int unit = 0; unit < units; ++unit)
                {
                    if (glActiveTexture)
                    {
                        glActiveTexture(GL_TEXTURE0 + unit);
                    }
                    glDisable(GL_TEXTURE_1D);
                    glDisable(GL_TEXTURE_2D);
                    if (GLAD_GL_VERSION_1_2)
                    {
                        glDisable(GL_TEXTURE_3D);
                    }
                    if (GLAD_GL_VERSION_1_3)
                    {
                        glDisable(GL_TEXTURE_CUBE_MAP);
                    }
                }

                if (glActiveTexture)
                {
                    glActiveTexture(GL_TEXTURE0);
                }
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glMatrixMode(GL_TEXTURE);
                glPushMatrix();
                glLoadIdentity();

                for (GLenum capability : {GL_DEPTH_TEST, GL_ALPHA_TEST, GL_CULL_FACE, GL_LIGHTING, GL_FOG, GL_STENCIL_TEST, GL_SCISSOR_TEST})
                {
                    glDisable(capability);
                }
                glEnable(GL_BLEND);
                for (GLenum plane = GL_CLIP_PLANE0; plane <= GL_CLIP_PLANE5; ++plane)
                {
                    glDisable(plane);
                }
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                if (glBlendEquation)
                {
                    glBlendEquation(GL_FUNC_ADD);
                }
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, width, height, 0, -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();
            }

            ~PanelGlState()
            {
                if (glActiveTexture)
                {
                    glActiveTexture(GL_TEXTURE0);
                }
                glMatrixMode(GL_TEXTURE);
                glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();

                // Attribute restoration includes read/draw buffers, which belong to these framebuffers.
                if (glBindFramebuffer)
                {
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                    glDrawBuffer(default_draw_buffer_);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer_);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer_);
                }

                glPopClientAttrib();
                glPopAttrib();

                if (glUseProgram)
                {
                    glUseProgram(program_);
                }
                if (glActiveTexture)
                {
                    glActiveTexture(active_texture_);
                }
                glMatrixMode(matrix_mode_);
                if (current_texture_)
                {
                    *current_texture_ = engine_texture_;
                }
            }
        };

        void SetColor(uint32_t color)
        {
            glColor4ub(uint8_t(color >> 24), uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color));
        }

        void Quad(const PerfPanelCommand& command)
        {
            glVertex2i(command.x, command.vertex_y[0]);
            glVertex2i(command.x + command.width, command.vertex_y[1]);
            glVertex2i(command.x + command.width, command.vertex_y[2]);
            glVertex2i(command.x, command.vertex_y[3]);
        }

        void DrawText(
            const PerfPanelCommand& command,
            const char* text,
            int drawable_height,
            int font,
            float scale,
            TextRenderInterface* text_service,
            ConsoleText_t console_text
        )
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(command.x, drawable_height - command.y - command.height, command.width, command.height);
#ifdef _WIN32
            (void)scale;
            (void)console_text;
            TextRenderInterface* service = text_service;
            int x = command.x;
            while (*text)
            {
                uint32_t codepoint = 0;
                bool error = false;
                int consumed = Q_UTF8ToUChar32(text, codepoint, error);
                if (consumed <= 0)
                {
                    break;
                }
                text += consumed;
                const TextGlyph* glyph = service->GetGlyph(font, codepoint);
                if (!glyph)
                {
                    continue;
                }
                if (glyph->renderable)
                {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, glyph->texture);
                    int left = x + glyph->bearing_x;
                    int top = command.y + glyph->bearing_y;
                    glBegin(GL_QUADS);
                    glTexCoord2f(glyph->u0, glyph->v0);
                    glVertex2i(left, top);
                    glTexCoord2f(glyph->u1, glyph->v0);
                    glVertex2i(left + glyph->wide, top);
                    glTexCoord2f(glyph->u1, glyph->v1);
                    glVertex2i(left + glyph->wide, top + glyph->tall);
                    glTexCoord2f(glyph->u0, glyph->v1);
                    glVertex2i(left, top + glyph->tall);
                    glEnd();
                }
                x += glyph->advance;
            }
#else
            (void)text_service;
            (void)font;
            if (console_text)
            {
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glTranslatef(float(command.x), float(command.y), 0);
                glScalef(scale, scale, 1);
                console_text(0, 0, const_cast<char*>(text));
                glPopMatrix();
            }
#endif
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_SCISSOR_TEST);
        }

        void Localize(vgui2::ILocalize* localization, const char* token, char (&output)[128])
        {
            wchar_t* translated = localization ? localization->Find(token) : nullptr;
            if (translated)
            {
                Q_UnicodeToUTF8(translated, output, sizeof(output));
            }
        }
    } // namespace

    void panel::DrawMapOutline(const PerfMapperSnapshot& mapper, const PerfMapRenderView& view)
    {
        if (!mapper.enabled || mapper.level < 2 || !mapper.target_hit || mapper.outline.empty())
        {
            return;
        }

        MapOutlineGlState state(view);
        glColor4ub(80, 220, 255, 255);
        glBegin(GL_LINES);

        for (const PerfMapLine& line : mapper.outline)
        {
            glVertex3fv(line.start.data());
            glVertex3fv(line.end.data());
        }

        glEnd();
    }

    PerformancePanel::PerformancePanel(
#ifdef _WIN32
        TextRenderInterface& text,
#endif
        ncl_perf::PerfClockInterface& clock,
        int* current_texture,
        vgui2::ILocalize* localization,
        ConsoleText_t console_text
    ) :
        clock_(clock),
        current_texture_(current_texture),
        localization_(localization),
        console_text_(console_text)
#ifdef _WIN32
        ,
        text_(text)
#endif
    {}

    void PerformancePanel::BindPresentation(vgui2::ILocalize* localization, ConsoleText_t console_text)
    {
        localization_ = localization;
        console_text_ = console_text;
        Invalidate();
    }

    void PerformancePanel::Invalidate()
    {
        layout_valid_ = false;
        text_ready_ = false;
        font_ = 0;
        text_runs_.clear();
    }

    void PerformancePanel::SetBackgroundAlpha(float alpha)
    {
        if (layout_.SetBackgroundAlpha(alpha))
        {
            layout_valid_ = false;
        }
    }

#ifdef _WIN32
    void PerformancePanel::PrepareText()
    {
        TextRenderInterface* service = &text_;
        bool complete = true;
        bool atlas_changed = text_generation_ != service->Generation();
        auto matches = [&](const TextRun& run, const PerfPanelCommand& command) {
            return run.ready && run.x == command.x && run.y == command.y && run.text == layout_.get_text(command);
        };
        for (const PerfPanelCommand& command : layout_.get_commands())
        {
            if (command.kind != PerfPanelCommandKind::Text)
            {
                continue;
            }
            if (text_runs_.size() <= command.text_index)
            {
                text_runs_.resize(size_t(command.text_index) + 1);
            }
            if (!atlas_changed && matches(text_runs_[command.text_index], command))
            {
                continue;
            }
            const char* text = layout_.get_text(command);
            while (*text)
            {
                uint32_t codepoint = 0;
                bool error = false;
                int consumed = Q_UTF8ToUChar32(text, codepoint, error);
                if (consumed <= 0)
                {
                    break;
                }
                text += consumed;
                if (!service->GetGlyph(font_, codepoint))
                {
                    complete = false;
                }
            }
        }

        uint32_t generation = service->Generation();
        atlas_changed = text_generation_ != generation;
        for (const PerfPanelCommand& command : layout_.get_commands())
        {
            if (command.kind != PerfPanelCommandKind::Text)
            {
                continue;
            }
            TextRun& run = text_runs_[command.text_index];
            if (!atlas_changed && matches(run, command))
            {
                continue;
            }
            run.glyphs.clear();
            run.ready = complete;
            run.x = command.x;
            run.y = command.y;
            run.text = layout_.get_text(command);
            int x = command.x;
            const char* text = layout_.get_text(command);
            while (*text)
            {
                uint32_t codepoint = 0;
                bool error = false;
                int consumed = Q_UTF8ToUChar32(text, codepoint, error);
                if (consumed <= 0)
                {
                    break;
                }
                text += consumed;
                const TextGlyph* glyph = service->GetGlyph(font_, codepoint);
                if (!glyph)
                {
                    complete = false;
                    run.ready = false;
                    continue;
                }
                if (glyph->renderable)
                {
                    run.glyphs.push_back({*glyph, x + glyph->bearing_x, command.y + glyph->bearing_y});
                }
                x += glyph->advance;
            }
        }
        text_generation_ = generation;
        text_ready_ = complete && generation == service->Generation();
    }

    void PerformancePanel::DrawCachedText(const PerfPanelCommand& command, int drawable_height)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(command.x, drawable_height - command.y - command.height, command.width, command.height);
        glEnable(GL_TEXTURE_2D);
        const TextRun& run = text_runs_[command.text_index];
        bool drawing = false;
        unsigned int texture = 0;
        for (const GlyphQuad& quad : run.glyphs)
        {
            const TextGlyph& glyph = quad.glyph;
            if (!drawing || texture != glyph.texture)
            {
                if (drawing)
                {
                    glEnd();
                }
                texture = glyph.texture;
                glBindTexture(GL_TEXTURE_2D, texture);
                glBegin(GL_QUADS);
                drawing = true;
            }
            glTexCoord2f(glyph.u0, glyph.v0);
            glVertex2i(quad.x, quad.y);
            glTexCoord2f(glyph.u1, glyph.v0);
            glVertex2i(quad.x + glyph.wide, quad.y);
            glTexCoord2f(glyph.u1, glyph.v1);
            glVertex2i(quad.x + glyph.wide, quad.y + glyph.tall);
            glTexCoord2f(glyph.u0, glyph.v1);
            glVertex2i(quad.x, quad.y + glyph.tall);
        }
        if (drawing)
        {
            glEnd();
        }
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_SCISSOR_TEST);
    }
#endif

    void PerformancePanel::Draw(
        const PerfSnapshot& snapshot,
        int width,
        int height,
        int mode,
        const PerfNetworkSnapshot* network,
        const PerfMapperSnapshot* mapper,
        int batch_level,
        const PerfSnapshot* batch_snapshot,
        const PerfGroupedGraphs* groups,
        const PerfFrameGraphs* frames
    )
    {
        if ((mode <= 0 && !network && !mapper && !batch_level) || width <= 0 || height <= 0)
        {
            return;
        }
        ncl_perf::PerfTimestamp_t now = clock_.Now();
        LayoutKey key{
            width,
            height,
            mode,
            batch_level,
            network && network->enabled,
            mapper != nullptr,
            mapper ? mapper->level : 0,
            mapper && mapper->map_loaded,
            mapper ? uint32_t(std::min(size_t(kPerfMapPropertyRows), mapper->properties.size())) : 0,
            snapshot.panel_scale,
            snapshot.collector_generation,
            snapshot.map_generation,
            snapshot.context_generation,
            frames ? uint32_t(frames->frames.size()) : 0,
            frames ? frames->captured_at : 0,
            frames != nullptr
        };
        if (!layout_valid_ || key != layout_key_ || now < layout_time_ || now - layout_time_ >= kLayoutRefreshInterval)
        {
            PerfPanelLabels labels;
            Localize(localization_, "NextClient_Perf_Title", labels.title);
            Localize(localization_, "NextClient_Perf_History", labels.history);
            Localize(localization_, "NextClient_Perf_Cpu", labels.cpu);
            Localize(localization_, "NextClient_Perf_Gpu", labels.gpu);
            Localize(localization_, "NextClient_Perf_Worker", labels.worker);
            Localize(localization_, "NextClient_Perf_BatchTitle", labels.batch_title);
            layout_.SetLabels(labels);
            layout_.Build(snapshot, width, height, mode, network, mapper, batch_level, batch_snapshot, groups, frames);
            layout_key_ = key;
            layout_time_ = now;
            layout_valid_ = true;
            text_ready_ = false;
        }
        else
        {
            if (mapper && mapper_revision_ != mapper->revision)
            {
                layout_.RefreshMapper(*mapper);
                text_ready_ = false;
            }
            if (network && (network_revision_ != network->revision || network_columns_ != network->graphs[0].size()))
            {
                layout_.RefreshNetwork(*network);
                text_ready_ = false;
            }
            if (batch_level && batch_snapshot &&
                (batch_frame_ != batch_snapshot->cpu_frame_id || batch_generation_ != batch_snapshot->collector_generation))
            {
                layout_.RefreshBatching(snapshot, batch_snapshot);
                text_ready_ = false;
            }
        }
        network_revision_ = network ? network->revision : 0;
        mapper_revision_ = mapper ? mapper->revision : 0;
        network_columns_ = network ? uint32_t(network->graphs[0].size()) : 0;
        batch_frame_ = batch_snapshot ? batch_snapshot->cpu_frame_id : 0;
        batch_generation_ = batch_snapshot ? batch_snapshot->collector_generation : 0;
        if (layout_.get_commands().empty())
        {
            return;
        }
        PanelGlState state(width, height, current_texture_);
#ifdef _WIN32
        if (!font_ || font_tall_ != layout_.get_font_tall())
        {
            font_tall_ = layout_.get_font_tall();
            font_ =
                text_.FindOrCreateFont({"Consolas", font_tall_, 400, kTextFontAntialias | kTextFontDropShadow, TextGlyphFormat::Coverage});
            text_ready_ = false;
            text_runs_.clear();
        }
        if (!text_ready_ || text_generation_ != text_.Generation())
        {
            PrepareText();
        }
#endif
        bool drawing = false;
        for (const PerfPanelCommand& command : layout_.get_commands())
        {
            SetColor(command.color);
            if (command.kind == PerfPanelCommandKind::Text)
            {
                if (drawing)
                {
                    glEnd();
                    drawing = false;
                }
#ifdef _WIN32
                if (text_ready_)
                {
                    DrawCachedText(command, height);
                }
                else
#endif
                {
                    DrawText(
                        command,
                        layout_.get_text(command),
                        height,
                        font_,
                        layout_.get_scale(),
#ifdef _WIN32
                        &text_,
#else
                        nullptr,
#endif
                        console_text_
                    );
                }
            }
            else
            {
                if (!drawing)
                {
                    glBegin(GL_QUADS);
                    drawing = true;
                }
                Quad(command);
            }
        }
        if (drawing)
        {
            glEnd();
        }
    }
} // namespace performance
