#include "graphics/render/material/MaterialTextureCache.h"

#include <ncl_material/MaterialPreset.h>

namespace render
{
    namespace
    {
        // The engine upload identifier has fixed capacity.
        constexpr size_t kMaxTexturePathLength = 128;
    } // namespace

    MaterialTextureCache::MaterialTextureCache(RenderDevice& device, MaterialImageLoader& loader) :
        device_(device),
        loader_(loader)
    {
        device_.Register(this);
    }

    MaterialTextureCache::~MaterialTextureCache()
    {
        device_.Unregister(this);
    }

    unsigned int MaterialTextureCache::Get(const char* path, tex::TextureLoadGuard guard, bool* canceled)
    {
        return GetImage(path, guard, canceled).texture;
    }

    MaterialImage MaterialTextureCache::GetImage(const char* path, tex::TextureLoadGuard guard, bool* canceled)
    {
        if (canceled != nullptr)
        {
            *canceled = false;
        }

        if (path == nullptr || *path == '\0' || !device_.CheckAvailable())
        {
            return {};
        }

        for (const Entry& entry : textures_)
        {
            if (entry.path == path)
            {
                return entry.image;
            }
        }

        const std::string owned_path(path);
        guard.device = &device_;
        guard.expected_device_generation = device_.get_generation();
        guard.native_context = guard.native_context != nullptr ? guard.native_context : context::Current();
        guard.texture_revision = &revision_;
        guard.expected_texture_revision = revision_;
        if (!ncl_material::IsAdmissibleTexturePath(owned_path, kMaxTexturePathLength))
        {
            loader_.ReportRejected(owned_path.c_str());
            if (!guard.is_current())
            {
                if (canceled != nullptr)
                {
                    *canceled = true;
                }
                return {};
            }
            textures_.push_back(Entry{owned_path, {}});

            return {};
        }

        guard.texture_revision = &revision_;
        guard.expected_texture_revision = revision_;
        const uint32_t device_generation = device_.get_generation();
        MaterialImage image = loader_.Load(owned_path.c_str(), guard);
        if (!guard.is_current())
        {
            if (canceled != nullptr)
            {
                *canceled = true;
            }
            if (image.texture != 0 && device_.get_generation() == device_generation && context::Current() == guard.native_context &&
                device_.IsContextCurrent())
            {
                loader_.DeleteTexture(image.texture);
            }
            return {};
        }
        textures_.push_back(Entry{owned_path, image});

        return image;
    }

    unsigned int MaterialTextureCache::Neutral(tex::TextureLoadGuard guard, bool* canceled)
    {
        if (canceled != nullptr)
        {
            *canceled = false;
        }
        if (neutral_ == 0 && device_.CheckAvailable())
        {
            guard.device = &device_;
            guard.expected_device_generation = device_.get_generation();
            guard.native_context = guard.native_context != nullptr ? guard.native_context : context::Current();
            guard.texture_revision = &revision_;
            guard.expected_texture_revision = revision_;
            const uint32_t device_generation = device_.get_generation();
            const unsigned int texture = loader_.CreateNeutral();
            if (!guard.is_current())
            {
                if (canceled != nullptr)
                {
                    *canceled = true;
                }
                if (texture != 0 && device_.get_generation() == device_generation && context::Current() == guard.native_context &&
                    device_.IsContextCurrent())
                {
                    loader_.DeleteTexture(texture);
                }
                return 0;
            }
            neutral_ = texture;
        }

        return neutral_;
    }

    void MaterialTextureCache::Remove(const std::string& path)
    {
        std::vector<unsigned int> retired;
        std::erase_if(textures_, [&](const Entry& entry) {
#ifdef _WIN32
            const bool matches = ncl_material::EqualsIgnoreCaseAscii(entry.path, path);
#else
            const bool matches = entry.path == path;
#endif
            if (matches)
            {
                retired.push_back(entry.image.texture);
            }

            return matches;
        });

        if (retired.empty())
        {
            return;
        }

        ++revision_;

        if (device_.IsContextCurrent())
        {
            for (unsigned int texture : retired)
            {
                if (texture != 0)
                {
                    loader_.DeleteTexture(texture);
                }
            }
        }
    }

    void MaterialTextureCache::Clear()
    {
        if (device_.IsContextCurrent())
        {
            for (const Entry& entry : textures_)
            {
                if (entry.image.texture != 0)
                {
                    loader_.DeleteTexture(entry.image.texture);
                }
            }

            if (neutral_ != 0)
            {
                loader_.DeleteTexture(neutral_);
            }
        }

        Forget();
    }

    void MaterialTextureCache::Forget()
    {
        ++revision_;
        textures_.clear();
        neutral_ = 0;
    }

    void MaterialTextureCache::OnDeviceLost()
    {
        Forget();
    }
} // namespace render
