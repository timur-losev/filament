/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>

#include <iomanip>

#include <math/scalar.h>
#include <math/vec4.h>

#include <imageio/ImageDecoder.h>
#include <imageio/ImageEncoder.h>

#include <utils/Path.h>

#include "cmgen.h"
#include "Cubemap.h"
#include "CubemapIBL.h"
#include "CubemapSH.h"
#include "CubemapUtils.h"

using namespace math;
using namespace image;

// -----------------------------------------------------------------------------------------------

image::ImageEncoder::Format g_format = image::ImageEncoder::Format::PNG;
std::string g_compression;
bool g_extract_faces = false;
double g_extract_blur = 0.0;
utils::Path g_extract_dir;

size_t g_output_size = 0;

bool g_quiet = false;
bool g_debug = false;

size_t g_sh_compute = 0;
bool g_sh_output = false;
bool g_sh_shader = false;
bool g_sh_irradiance = false;
ShFile g_sh_file = ShFile::SH_NONE;
utils::Path g_sh_filename;

bool g_is_mipmap = false;
utils::Path g_is_mipmap_dir;
bool g_prefilter = false;
utils::Path g_prefilter_dir;
bool g_dfg = false;
utils::Path g_dfg_filename;
bool g_dfg_multiscatter = false;

bool g_deploy = false;
utils::Path g_deploy_dir;

size_t g_num_samples = 1024;

bool g_mirror = false;

// -----------------------------------------------------------------------------------------------

static void outputSh(std::ostream& out, const std::unique_ptr<math::double3[]>& sh, size_t numBands);

// -----------------------------------------------------------------------------------------------

void generateMipmaps(std::vector<Cubemap>& levels, std::vector<Image>& images) {
    Image temp;
    const Cubemap& base(levels[0]);
    size_t dim = base.getDimensions();
    size_t mipLevel = 0;
    while (dim > 1) {
        dim >>= 1;
        Cubemap dst = CubemapUtils::create(temp, dim);
        const Cubemap& src(levels[mipLevel++]);
        CubemapUtils::downsampleCubemapLevelBoxFilter(dst, src);
        dst.makeSeamless();
        images.push_back(std::move(temp));
        levels.push_back(std::move(dst));
    }
}

void sphericalHarmonics(const utils::Path& iname, const Cubemap& inputCubemap) {
    std::unique_ptr<math::double3[]> sh;
    if (g_sh_shader) {
        sh = CubemapSH::computeIrradianceSH3Bands(inputCubemap);
    } else {
        sh = CubemapSH::computeSH(inputCubemap, g_sh_compute, g_sh_irradiance);
    }

    if (g_sh_output) {
        outputSh(std::cout, sh, g_sh_compute);
    }

    if (g_sh_file != ShFile::SH_NONE || g_debug) {
        Image image;
        const size_t dim = g_output_size ? g_output_size : inputCubemap.getDimensions();
        Cubemap cm = CubemapUtils::create(image, dim);

        if (g_sh_file != ShFile::SH_NONE) {
            utils::Path outputDir(g_sh_filename.getAbsolutePath().getParent());
            if (!outputDir.exists()) {
                outputDir.mkdirRecursive();
            }

            if (g_sh_shader) {
                CubemapSH::renderPreScaledSH3Bands(cm, sh);
            } else {
                CubemapSH::renderSH(cm, sh, g_sh_compute);
            }

            if (g_sh_file == ShFile::SH_CROSS) {
                std::ofstream outputStream(g_sh_filename, std::ios::binary | std::ios::trunc);
                ImageEncoder::encode(outputStream,
                        ImageEncoder::chooseFormat(g_sh_filename.getName()),
                        image, g_compression, g_sh_filename);
            }
            if (g_sh_file == ShFile::SH_TEXT) {
                std::ofstream outputStream(g_sh_filename, std::ios::trunc);
                outputSh(outputStream, sh, g_sh_compute);
            }
        }

        if (g_debug) {
            utils::Path outputDir(g_sh_filename.getAbsolutePath().getParent());
            if (!outputDir.exists()) {
                outputDir.mkdirRecursive();
            }

            { // save a file with what we just calculated (radiance or irradiance)
                std::string basename = iname.getNameWithoutExtension();
                utils::Path filePath =
                        outputDir + (basename + "_sh" + (g_sh_irradiance ? "_i" : "_r") + ".png");
                std::ofstream outputStream(filePath, std::ios::binary | std::ios::trunc);
                ImageEncoder::encode(outputStream, ImageEncoder::Format::PNG, image, "",
                        filePath.getPath());
            }

            { // save a file with the "other one" (irradiance or radiance)
                sh = CubemapSH::computeSH(inputCubemap, g_sh_compute, !g_sh_irradiance);
                CubemapSH::renderSH(cm, sh, g_sh_compute);
                std::string basename = iname.getNameWithoutExtension();
                utils::Path filePath =
                        outputDir + (basename + "_sh" + (!g_sh_irradiance ? "_i" : "_r") + ".png");
                std::ofstream outputStream(filePath, std::ios::binary | std::ios::trunc);
                ImageEncoder::encode(outputStream, ImageEncoder::Format::PNG, image, "",
                        filePath.getPath());
            }
        }
    }
}

void outputSh(std::ostream& out,
        const std::unique_ptr<math::double3[]>& sh, size_t numBands) {
    for (ssize_t l=0 ; l<numBands ; l++) {
        for (ssize_t m=-l ; m<=l ; m++) {
            size_t i = CubemapSH::getShIndex(m, (size_t) l);
            std::string name = "L" + std::to_string(l) + std::to_string(m);
            if (g_sh_irradiance) {
                name.append(", irradiance");
            }
            if (g_sh_shader) {
                name.append(", pre-scaled base");
            }
            out << "("
                << std::fixed << std::setprecision(15) << std::setw(18) << sh[i].r << ", "
                << std::fixed << std::setprecision(15) << std::setw(18) << sh[i].g << ", "
                << std::fixed << std::setprecision(15) << std::setw(18) << sh[i].b
                << "); // " << name
                << std::endl;
        }
    }
}

void iblMipmapPrefilter(const utils::Path& iname,
        const std::vector<Image>& images, const std::vector<Cubemap>& levels,
        const utils::Path& dir) {
    utils::Path outputDir(dir.getAbsolutePath() + iname.getNameWithoutExtension());
    if (!outputDir.exists()) {
        outputDir.mkdirRecursive();
    }

    const size_t numLevels = levels.size();
    for (size_t level=0 ; level<numLevels ; level++) {
        Cubemap const& dst(levels[level]);
        Image const& img(images[level]);
        if (g_debug) {
            ImageEncoder::Format debug_format = ImageEncoder::Format::PNG;
            std::string ext = ImageEncoder::chooseExtension(debug_format);
            std::string basename = iname.getNameWithoutExtension();
            utils::Path filePath = outputDir + (basename + "_is_m" + (std::to_string(level) + ext));

            std::ofstream outputStream(filePath, std::ios::binary | std::ios::trunc);
            ImageEncoder::encode(outputStream, debug_format, img,
                                 g_compression, filePath.getPath());
        }

        std::string ext = ImageEncoder::chooseExtension(g_format);
        for (size_t i = 0; i < 6; i++) {
            Cubemap::Face face = (Cubemap::Face)i;
            std::string filename = outputDir
                    + ("is_m" + std::to_string(level) + "_" + CubemapUtils::getFaceName(face) + ext);
            std::ofstream outputStream(filename, std::ios::binary | std::ios::trunc);
            ImageEncoder::encode(outputStream, g_format, dst.getImageForFace(face),
                                 g_compression, filename);
        }
    }
}

void iblRoughnessPrefilter(const utils::Path& iname,
        const std::vector<Cubemap>& levels, const utils::Path& dir) {
    utils::Path outputDir(dir.getAbsolutePath() + iname.getNameWithoutExtension());
    if (!outputDir.exists()) {
        outputDir.mkdirRecursive();
    }

    // DEBUG: enable this to generate prefilter mipmaps at full resolution
    // (of course, they're not mimaps at this point)
    // This is useful for debugging.
    const bool DEBUG_FULL_RESOLUTION = false;

    const size_t baseExp = __builtin_ctz(g_output_size ? g_output_size : 256);
    size_t numSamples = g_num_samples;
    const size_t numLevels = baseExp + 1;
    for (ssize_t i=baseExp ; i>=0 ; --i) {
        const size_t dim = 1U << (DEBUG_FULL_RESOLUTION ? baseExp : i);
        const size_t level = baseExp - i;
        if (level >= 2) {
            // starting at level 2, we increase the number of samples per level
            // this helps as the filter gets wider, and since there are 4x less work
            // per level, this doesn't slow things down a lot.
            if (!DEBUG_FULL_RESOLUTION) {
                numSamples *= 2;
            }
        }
        const double roughness = saturate(level / (numLevels - 1.0));
        const double linear_roughness = roughness * roughness;
        if (!g_quiet) {
            std::cout << "Level " << level << ", roughness = " <<
                      std::setprecision(3) << roughness << ", roughness(lin) = " << linear_roughness
                      << std::endl;
        }
        Image image;
        Cubemap dst = CubemapUtils::create(image, dim);
        CubemapIBL::roughnessFilter(dst, levels, linear_roughness, numSamples);

        if (g_debug) {
            ImageEncoder::Format debug_format = ImageEncoder::Format::PNG;
            std::string ext = ImageEncoder::chooseExtension(debug_format);
            std::string basename = iname.getNameWithoutExtension();
            utils::Path filePath = outputDir + (basename + "_roughness_m" + (std::to_string(level) + ext));

            std::ofstream outputStream(filePath, std::ios::binary | std::ios::trunc);
            ImageEncoder::encode(outputStream, debug_format, image,
                                 g_compression, filePath.getPath());
        }

        std::string ext = ImageEncoder::chooseExtension(g_format);
        for (size_t j = 0; j < 6; j++) {
            Cubemap::Face face = (Cubemap::Face) j;
            std::string filename = outputDir
                    + ("m" + std::to_string(level) + "_" + CubemapUtils::getFaceName(face) + ext);
            std::ofstream outputStream(filename, std::ios::binary | std::ios::trunc);
            ImageEncoder::encode(outputStream, g_format, dst.getImageForFace(face),
                                 g_compression, filename);
        }
    }
}

static bool isTextFile(const utils::Path& filename) {
    std::string extension(filename.getExtension());
    return extension == "h" || extension == "hpp" ||
           extension == "c" || extension == "cpp" ||
           extension == "inc" || extension == "txt";
}

static bool isIncludeFile(const utils::Path& filename) {
    std::string extension(filename.getExtension());
    return extension == "inc";
}

void iblLutDfg(const utils::Path& filename, size_t size, bool multiscatter) {
    std::unique_ptr<uint8_t[]> buf(new uint8_t[size*size*sizeof(float3)]);
    Image image(std::move(buf), size, size, size*sizeof(float3), sizeof(float3));
    CubemapIBL::DFG(image, multiscatter);

    utils::Path outputDir(filename.getAbsolutePath().getParent());
    if (!outputDir.exists()) {
        outputDir.mkdirRecursive();
    }

    if (isTextFile(filename)) {
        const bool isInclude = isIncludeFile(filename);
        std::ofstream outputStream(filename, std::ios::trunc);

        outputStream << "// generated with: cmgen --ibl-dfg=" << filename.c_str() << std::endl;
        outputStream << "// DFG LUT stored as an RG16F texture, in GL order" << std::endl;
        if (!isInclude) {
            outputStream << "const uint16_t DFG_LUT[] = {";
        }
        for (size_t y = 0; y < size; y++) {
            for (size_t x = 0; x < size; x++) {
                if (x % 4 == 0) outputStream << std::endl << "    ";
                const half2 d = half2(static_cast<float3*>(image.getPixelRef(x, size - 1 - y))->xy);
                const uint16_t r = *reinterpret_cast<const uint16_t*>(&d.r);
                const uint16_t g = *reinterpret_cast<const uint16_t*>(&d.g);
                outputStream << "0x" << std::setfill('0') << std::setw(4) << std::hex << r << ", ";
                outputStream << "0x" << std::setfill('0') << std::setw(4) << std::hex << g << ", ";
            }
        }
        if (!isInclude) {
            outputStream << std::endl << "};" << std::endl;
        }

        outputStream << std::endl;
        outputStream.flush();
        outputStream.close();
    } else {
        std::ofstream outputStream(filename, std::ios::binary | std::ios::trunc);
        ImageEncoder::Format format = ImageEncoder::chooseFormat(filename.getName(), true);
        ImageEncoder::encode(outputStream, format, image, g_compression, filename.getPath());
    }
}

void extractCubemapFaces(const utils::Path& iname, const Cubemap& cm, const utils::Path& dir) {
    utils::Path outputDir(dir.getAbsolutePath() + iname.getNameWithoutExtension());
    if (!outputDir.exists()) {
        outputDir.mkdirRecursive();
    }
    std::string ext = ImageEncoder::chooseExtension(g_format);
    for (size_t i=0 ; i<6 ; i++) {
        Cubemap::Face face = (Cubemap::Face)i;
        std::string filename(outputDir + (CubemapUtils::getFaceName(face) + ext));
        std::ofstream outputStream(filename, std::ios::binary | std::ios::trunc);
        ImageEncoder::encode(outputStream, g_format, cm.getImageForFace(face),
                             g_compression, filename);
    }
}
