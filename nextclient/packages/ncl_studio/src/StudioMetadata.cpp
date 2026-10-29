#include "ncl_studio/StudioMetadata.h"

#include <cmath>

#include <goldsrc/format/studio.h>

#include "HeaderShape.h"
#include "table_bounds.h"

namespace ncl_studio
{
    ncl_utils::ResultT<StudioMetadata> ReadStudioMetadata(const void* data, size_t size)
    {
        if (data == nullptr || size < sizeof(goldsrc::studiohdr_t))
        {
            return ncl_utils::ResultError("buffer is smaller than a studio header");
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        const goldsrc::studiohdr_t* header = reinterpret_cast<const goldsrc::studiohdr_t*>(bytes);
        std::string shape = HeaderShapeError(header);

        if (!shape.empty())
        {
            return ncl_utils::ResultError(std::move(shape));
        }

        if (!TableFits(size, header->seqindex, header->numseq, sizeof(goldsrc::mstudioseqdesc_t)))
        {
            return ncl_utils::ResultError("sequence array lies outside the buffer");
        }

        StudioMetadata result;

        for (int axis = 0; axis < 3; axis++)
        {
            if (!std::isfinite(header->bbmin[axis]) || !std::isfinite(header->bbmax[axis]))
            {
                return ncl_utils::ResultError("model declares nonfinite bounds");
            }

            result.minimum[axis] = header->bbmin[axis];
            result.maximum[axis] = header->bbmax[axis];
        }

        const goldsrc::mstudioseqdesc_t* sequences = reinterpret_cast<const goldsrc::mstudioseqdesc_t*>(bytes + header->seqindex);

        for (int index = 0; index < header->numseq; index++)
        {
            const goldsrc::mstudioseqdesc_t& sequence = sequences[index];
            size_t length = 0;

            while (length < sizeof(sequence.label) && sequence.label[length] != '\0')
            {
                length++;
            }

            result.sequences.push_back(StudioSequence{std::string(sequence.label, length), sequence.fps, sequence.numframes});
        }

        if (!TableFits(size, header->bodypartindex, header->numbodyparts, sizeof(goldsrc::mstudiobodyparts_t)))
        {
            return ncl_utils::ResultError("body part array lies outside the buffer");
        }

        const goldsrc::mstudiobodyparts_t* parts = reinterpret_cast<const goldsrc::mstudiobodyparts_t*>(bytes + header->bodypartindex);

        for (int index = 0; index < header->numbodyparts; index++)
        {
            const goldsrc::mstudiobodyparts_t& part = parts[index];

            if (!TableFits(size, part.modelindex, part.nummodels, sizeof(goldsrc::mstudiomodel_t)))
            {
                return ncl_utils::ResultError("submodel array lies outside the buffer");
            }

            StudioBodyPart owned;
            owned.base = part.base;
            const goldsrc::mstudiomodel_t* submodels = reinterpret_cast<const goldsrc::mstudiomodel_t*>(bytes + part.modelindex);

            for (int model = 0; model < part.nummodels; model++)
            {
                const char* name = submodels[model].name;
                size_t length = 0;

                while (length < sizeof(submodels[model].name) && name[length] != '\0')
                {
                    length++;
                }

                owned.submodels.emplace_back(name, length);
            }

            result.bodyparts.push_back(std::move(owned));
        }

        return std::move(result);
    }
} // namespace ncl_studio
