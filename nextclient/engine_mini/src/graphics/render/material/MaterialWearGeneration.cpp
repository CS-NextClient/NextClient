#include "graphics/render/material/MaterialEditor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <optional>
#include <thread>

#include <ncl_image/Decode.h>
#include <ncl_image/Encode.h>
#include <ncl_studio/StudioWearBake.h>
#include <tier1/strtools.h>

#include "graphics/render/material/MaterialDocumentStore.h"
#include "graphics/render/material/MaterialTextureCache.h"

namespace render
{
    namespace
    {
        constexpr size_t kMaxModelBytes = 64 * 1024 * 1024;
        constexpr size_t kMaxScratchBytes = 8 * 1024 * 1024;
        constexpr uint64_t kMaxWearPixels = 1048576;
        constexpr int kWearMapSize = 1024;
        constexpr const char* kScratchPath = "materials/masks/scratches/fine_random.png";

        void ReplaceParam(ncl_material::Material& material, const char* name, ncl_material::ParamValue value)
        {
            auto found = std::find_if(material.params.begin(), material.params.end(), [name](const ncl_material::Param& param) {
                return param.name == name;
            });

            if (found == material.params.end())
            {
                material.params.push_back({name, std::move(value)});
            }
            else
            {
                found->value = std::move(value);
            }
        }

        void CopyError(const std::string& error, char* error_out, int error_out_len)
        {
            if (error_out != nullptr && error_out_len > 0)
            {
                V_strncpy(error_out, error.c_str(), error_out_len);
            }
        }
    } // namespace

    struct MaterialEditor::WearGenerationJob
    {
        uint64_t revision{};
        uint64_t request_revision{};
        int skin_family{};
        float scratch_strength{};
        std::string model_data;
        std::string scratch_data;
        ncl_studio::StudioTextures textures;
        std::vector<int> texture_indices;
        std::vector<ncl_material::MaterialEntry> entries;
        std::vector<std::string> png_images;
        std::string error;
        std::atomic<bool> finished{};
        std::jthread worker;

        std::string Prepare(std::stop_token stop)
        {
            if (stop.stop_requested())
            {
                return "wear generation canceled";
            }

            ncl_studio::WearBakeOptions options;
            options.size = kWearMapSize;
            options.skin_family = skin_family;
            options.scratch_strength = scratch_strength;
            options.texture_indices = texture_indices;
            options.stop_token = stop;
            std::optional<ncl_utils::ResultT<ncl_image::RgbaImage, ncl_image::DecodeError>> scratches;

            if (scratch_strength > 0)
            {
                scratches.emplace(
                    ncl_image::DecodeRgba8({reinterpret_cast<const uint8_t*>(scratch_data.data()), scratch_data.size()}, kMaxWearPixels)
                );

                if (scratches->has_error())
                {
                    return scratches->error_str();
                }

                const ncl_image::RgbaImage& image = **scratches;
                options.scratch_width = image.width;
                options.scratch_height = image.height;
                options.scratch_rgba = {image.pixels.get(), static_cast<size_t>(image.width) * image.height * 4};
            }

            const ncl_utils::ResultT<std::vector<ncl_studio::StudioWearMap>> baked =
                ncl_studio::BakeStudioWearMaps(model_data.data(), model_data.size(), textures, options);

            if (baked.has_error())
            {
                return baked.error_str();
            }

            for (const ncl_material::MaterialEntry& entry : entries)
            {
                if (stop.stop_requested())
                {
                    return "wear generation canceled";
                }

                auto found = std::find_if(baked->begin(), baked->end(), [&](const ncl_studio::StudioWearMap& image) {
                    return ncl_material::EqualsIgnoreCaseAscii(textures.textures[image.texture_index].name, entry.texture_name);
                });

                if (found == baked->end())
                {
                    return "selected texture is not used by this skin family: " + entry.texture_name;
                }

                if (static_cast<uint64_t>(found->width) * found->height > kMaxWearPixels)
                {
                    return "generated wear image exceeds the material image limit";
                }

                const ncl_utils::ResultT<std::vector<uint8_t>> encoded =
                    ncl_image::EncodePng(found->width, found->height, 1, found->pixels);

                if (encoded.has_error())
                {
                    return encoded.error_str();
                }

                png_images.emplace_back(reinterpret_cast<const char*>(encoded->data()), encoded->size());
            }

            return stop.stop_requested() ? "wear generation canceled" : std::string();
        }

        void Run(std::stop_token stop)
        {
            try
            {
                error = Prepare(stop);
            }
            catch (const std::exception& exception)
            {
                error = exception.what();
            }
            catch (...)
            {
                error = "wear generation failed";
            }

            finished.store(true, std::memory_order_release);
        }
    };

    MaterialEditor::~MaterialEditor()
    {
        CancelWearGeneration();
    }

    bool MaterialEditor::BeginWearGeneration(int skin_family, float scratch_strength, char* error_out, int error_out_len)
    {
        const auto fail = [&](const std::string& error) {
            CopyError(error, error_out, error_out_len);
            return false;
        };
        CopyError({}, error_out, error_out_len);

        if (!session_.active || session_.entries.empty() || !session_.preview_values.empty())
        {
            return fail("select a material and finish the current parameter preview before generating wear");
        }

        if (!std::isfinite(scratch_strength) || scratch_strength < 0 || scratch_strength > 1)
        {
            return fail("scratch intensity must be between 0 and 1");
        }

        std::shared_ptr<WearGenerationJob> job = std::make_shared<WearGenerationJob>();
        job->revision = edit_revision_;
        job->request_revision = ++wear_request_revision_;
        job->skin_family = skin_family;
        job->scratch_strength = scratch_strength;
        job->entries = session_.entries;

        for (const ncl_material::MaterialEntry& entry : job->entries)
        {
            if (entry.material.program != "weapon_finish")
            {
                return fail("wear generation requires weapon_finish on every selected texture");
            }
        }

        const std::string model = session_.model;
        const auto is_current = [&] {
            return session_.active && edit_revision_ == job->revision && wear_request_revision_ == job->request_revision;
        };
        const MaterialReadStatus model_status = documents_.ReadFile(model.c_str(), job->model_data, kMaxModelBytes);

        if (!is_current())
        {
            return fail("material editing changed while loading the model");
        }

        if (model_status != MaterialReadStatus::Ready)
        {
            return fail("cannot read model " + model);
        }

        const ncl_utils::ResultT<ncl_studio::StudioTextures> embedded =
            ncl_studio::ReadStudioTextures(job->model_data.data(), job->model_data.size());

        if (embedded.has_error())
        {
            return fail(embedded.error_str());
        }

        job->textures = *embedded;

        if (job->textures.textures.empty())
        {
            const std::string companion = model.substr(0, model.size() - 4) + "T.mdl";
            std::string companion_data;
            const MaterialReadStatus companion_status = documents_.ReadFile(companion.c_str(), companion_data, kMaxModelBytes);

            if (!is_current())
            {
                return fail("material editing changed while loading model textures");
            }

            if (companion_status != MaterialReadStatus::Ready)
            {
                return fail("cannot read texture companion " + companion);
            }

            const ncl_utils::ResultT<ncl_studio::StudioTextures> split =
                ncl_studio::ReadStudioTextures(companion_data.data(), companion_data.size());

            if (split.has_error())
            {
                return fail(split.error_str());
            }

            job->textures = *split;
        }

        for (const ncl_material::MaterialEntry& entry : job->entries)
        {
            auto found =
                std::find_if(job->textures.textures.begin(), job->textures.textures.end(), [&](const ncl_studio::TextureInfo& texture) {
                    return ncl_material::EqualsIgnoreCaseAscii(texture.name, entry.texture_name);
                });

            if (found == job->textures.textures.end())
            {
                return fail("selected texture is missing from the model: " + entry.texture_name);
            }

            job->texture_indices.push_back(static_cast<int>(found - job->textures.textures.begin()));
        }

        if (scratch_strength > 0)
        {
            const MaterialReadStatus scratch_status = documents_.ReadFile(kScratchPath, job->scratch_data, kMaxScratchBytes);

            if (!is_current())
            {
                return fail("material editing changed while loading scratches");
            }

            if (scratch_status != MaterialReadStatus::Ready)
            {
                return fail(std::string("cannot read scratch image ") + kScratchPath);
            }
        }

        wear_error_.clear();
        wear_status_ = MaterialWearStatus::Running;

        if (wear_job_ != nullptr)
        {
            wear_job_->worker.request_stop();
            pending_wear_job_ = std::move(job);
        }
        else
        {
            if (!StartWearJob(std::move(job)))
            {
                return fail(wear_error_);
            }
        }

        return true;
    }

    bool MaterialEditor::StartWearJob(std::shared_ptr<WearGenerationJob> job)
    {
        WearGenerationJob* worker_job = job.get();

        try
        {
            job->worker = std::jthread([worker_job](std::stop_token stop) { worker_job->Run(stop); });
        }
        catch (const std::exception& exception)
        {
            wear_error_ = exception.what();
            wear_status_ = MaterialWearStatus::Failed;
            return false;
        }

        wear_job_ = std::move(job);
        return true;
    }

    MaterialWearStatus MaterialEditor::PollWearGeneration(char* error_out, int error_out_len)
    {
        if (wear_job_ != nullptr && wear_job_->finished.load(std::memory_order_acquire))
        {
            std::shared_ptr<WearGenerationJob> completed = std::move(wear_job_);
            completed->worker.join();

            if (pending_wear_job_ != nullptr)
            {
                if (!session_.active || edit_revision_ != pending_wear_job_->revision ||
                    wear_request_revision_ != pending_wear_job_->request_revision)
                {
                    pending_wear_job_.reset();
                    wear_error_ = "material editing changed while generating wear";
                    wear_status_ = MaterialWearStatus::Failed;
                }
                else
                {
                    StartWearJob(std::move(pending_wear_job_));
                }
            }
            else if (!session_.active || edit_revision_ != completed->revision || wear_request_revision_ != completed->request_revision)
            {
                wear_error_ = "material editing changed while generating wear";
                wear_status_ = MaterialWearStatus::Failed;
            }
            else if (!completed->error.empty())
            {
                wear_error_ = completed->error;
                wear_status_ = MaterialWearStatus::Failed;
            }
            else
            {
                std::string error;
                const bool published = PublishWearImages(*completed, error);

                if (wear_request_revision_ == completed->request_revision)
                {
                    wear_error_ = std::move(error);
                    wear_status_ = published ? MaterialWearStatus::Ready : MaterialWearStatus::Failed;
                }
            }
        }

        CopyError(wear_error_, error_out, error_out_len);
        return wear_status_;
    }

    void MaterialEditor::CancelWearGeneration()
    {
        ++wear_request_revision_;
        pending_wear_job_.reset();

        if (wear_job_ != nullptr)
        {
            wear_job_->worker.request_stop();
            wear_job_.reset();
        }

        wear_status_ = MaterialWearStatus::Idle;
        wear_error_.clear();
    }

    bool MaterialEditor::PublishWearImages(WearGenerationJob& job, std::string& error)
    {
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::string prefix = "materials/generated/wear_" + std::to_string(timestamp) + "_" + std::to_string(++generated_sequence_);
        std::vector<std::string> created;
        const auto rollback = [&] {
            for (const std::string& image : created)
            {
                if (!documents_.RemoveGenerated(image))
                {
                    documents_.Report("materials: cannot remove failed preview image %s\n", image.c_str());
                }
            }
        };

        for (size_t index = 0; index < job.entries.size(); index++)
        {
            const std::string path = prefix + "_" + std::to_string(index) + ".png";

            if (!documents_.WriteGenerated(path, job.png_images[index], error))
            {
                rollback();
                return false;
            }

            created.push_back(path);

            if (!session_.active || edit_revision_ != job.revision || wear_request_revision_ != job.request_revision)
            {
                rollback();
                error = "material editing changed while writing generated images";
                return false;
            }

            ncl_material::ParamValue map;
            map.type = ncl_material::ParamType::TextureRef;
            map.texture_ref = path;
            ReplaceParam(job.entries[index].material, "uWearMap", std::move(map));
            ncl_material::ParamValue transform;
            transform.type = ncl_material::ParamType::Vec4;
            transform.vec4 = {1, 1, 0, 0};
            ReplaceParam(job.entries[index].material, "uWearMapTransform", std::move(transform));
        }

        std::vector<std::string> obsolete;

        for (const std::string& image : generated_images_)
        {
            if (!ReferencesImage(job.entries, image))
            {
                obsolete.push_back(image);
            }
        }

        if (!obsolete.empty() && image_retirement_barrier_)
        {
            image_retirement_barrier_();
        }

        if (!session_.active || edit_revision_ != job.revision || wear_request_revision_ != job.request_revision)
        {
            rollback();
            error = "material editing changed while retiring generated images";
            return false;
        }

        std::erase_if(obsolete, [&](const std::string& image) {
            return std::find(generated_images_.begin(), generated_images_.end(), image) == generated_images_.end();
        });
        std::erase_if(generated_images_, [&](const std::string& image) { return !ReferencesImage(job.entries, image); });
        generated_images_.insert(generated_images_.end(), created.begin(), created.end());
        session_.entries = std::move(job.entries);
        ++edit_revision_;
        RemoveGeneratedImages(obsolete);
        return true;
    }

    void MaterialEditor::DiscardGeneratedImages()
    {
        const std::vector<std::string> images = std::move(generated_images_);
        generated_images_.clear();

        if (!images.empty() && image_retirement_barrier_)
        {
            image_retirement_barrier_();
        }

        RemoveGeneratedImages(images);
    }

    void MaterialEditor::RemoveGeneratedImages(const std::vector<std::string>& images)
    {
        for (const std::string& image : images)
        {
            textures_.Remove(image);

            if (!documents_.RemoveGenerated(image))
            {
                documents_.Report("materials: cannot remove unsaved image %s\n", image.c_str());
            }
        }
    }

    bool MaterialEditor::ReferencesImage(const std::vector<ncl_material::MaterialEntry>& entries, const std::string& image)
    {
        return std::any_of(entries.begin(), entries.end(), [&](const ncl_material::MaterialEntry& entry) {
            return std::any_of(entry.material.params.begin(), entry.material.params.end(), [&](const ncl_material::Param& param) {
                return param.value.type == ncl_material::ParamType::TextureRef &&
                       ncl_material::EqualsIgnoreCaseAscii(param.value.texture_ref, image);
            });
        });
    }
} // namespace render
