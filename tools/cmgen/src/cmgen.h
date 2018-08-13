/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Cubemap.h"

#include <image/Image.h>
#include <imageio/ImageEncoder.h>

#include <utils/Path.h>

#include <string>

enum class ShFile {
    SH_NONE, SH_CROSS, SH_TEXT
};
extern image::ImageEncoder::Format g_format;
extern std::string g_compression;
extern bool g_extract_faces;
extern double g_extract_blur;
extern utils::Path g_extract_dir;

extern size_t g_output_size;

extern bool g_quiet;
extern bool g_debug;

extern size_t g_sh_compute;
extern bool g_sh_output;
extern bool g_sh_shader;
extern bool g_sh_irradiance;
extern ShFile g_sh_file;
extern utils::Path g_sh_filename;

extern bool g_is_mipmap;
extern utils::Path g_is_mipmap_dir;
extern bool g_prefilter;
extern utils::Path g_prefilter_dir;
extern bool g_dfg;
extern utils::Path g_dfg_filename;
extern bool g_dfg_multiscatter;

extern bool g_deploy;
extern utils::Path g_deploy_dir;

extern size_t g_num_samples;

extern bool g_mirror;

void iblLutDfg(const utils::Path& filename, size_t size, bool multiscatter);
void sphericalHarmonics(const utils::Path& iname, const Cubemap& inputCubemap);
void iblMipmapPrefilter(const utils::Path& iname, const std::vector<image::Image>& images,
        const std::vector<Cubemap>& levels, const utils::Path& dir);
void iblRoughnessPrefilter(const utils::Path& iname, const std::vector<Cubemap>& levels,
        const utils::Path& dir);
void extractCubemapFaces(const utils::Path& iname, const Cubemap& cm, const utils::Path& dir);
void generateMipmaps(std::vector<Cubemap>& levels, std::vector<image::Image>& images);
