#pragma once
#include <cstdint>

namespace simcore {

// Extractor statistics
struct ExtractorStats {
    int32_t total_extractors;
    int32_t active_extractors;
    int32_t total_resources_extracted;
};

// System functions
void Extractor_Init();
void Extractor_Clear();
void Extractor_Step(float dt);
ExtractorStats Extractor_GetStats();

} // namespace simcore
