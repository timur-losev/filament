/*
 * Copyright 2018 The Android Open Source Project
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

#include <image/ColorTransform.h>
#include <image/ImageOps.h>
#include <image/ImageSampler.h>
#include <image/LinearImage.h>

#include <imageio/ImageDecoder.h>
#include <imageio/ImageEncoder.h>

#include <gtest/gtest.h>

#include <utils/Panic.h>
#include <utils/Path.h>
#include <math/vec3.h>

#include <fstream>
#include <string>
#include <sstream>

using std::istringstream;
using std::string;
using math::float3;
using std::swap;

using namespace image;

class CmgenTest : public testing::Test {};

enum class ComparisonMode {
    SKIP,
    COMPARE,
    UPDATE,
};

static ComparisonMode g_comparisonMode;
static utils::Path g_comparisonPath;

// Creates a tiny RGB image from a pattern string.
static LinearImage createColorFromAscii(const string& pattern);

// Saves an image to disk or does a load-and-compare, depending on g_comparisonMode.
static void updateOrCompare(const LinearImage& limg, const utils::Path& fname);

TEST_F(CmgenTest, ImageOps) { // NOLINT


    "cmgen -x ./ibls/ my_ibl.exr ";


    auto finalize = [] (LinearImage image) {
        return resampleImage(image, 100, 100, Filter::NEAREST);
    };
    LinearImage x22 = [finalize] () {
        auto original = createColorFromAscii("12 34");
        auto hflipped = finalize(horizontalFlip(original));
        auto vflipped = finalize(verticalFlip(original));
        return horizontalStack({finalize(original), hflipped, vflipped});
    }();
    LinearImage x23 = [finalize] () {
        auto original = createColorFromAscii("123 456");
        auto hflipped = finalize(horizontalFlip(original));
        auto vflipped = finalize(verticalFlip(original));
        return horizontalStack({finalize(original), hflipped, vflipped});
    }();
    LinearImage x32 = [finalize] () {
        auto original = createColorFromAscii("12 34 56");
        auto hflipped = finalize(horizontalFlip(original));
        auto vflipped = finalize(verticalFlip(original));
        return horizontalStack({finalize(original), hflipped, vflipped});
    }();
    auto atlas = verticalStack({x22, x23, x32});
    updateOrCompare(atlas, "imageops.png");
}

static void printUsage(const char* name) {
    std::string exec_name(utils::Path(name).getName());
    std::string usage(
            "TEST is a unit test runner\n"
            "Usages:\n"
            "    TEST compare <path-to-ref-images> [gtest options]\n"
            "    TEST update  <path-to-ref-images> [gtest options]\n"
            "    TEST [gtest options]\n"
            "\n");
    const std::string from("TEST");
    for (size_t pos = usage.find(from); pos != std::string::npos; pos = usage.find(from, pos)) {
         usage.replace(pos, from.length(), exec_name);
    }
    printf("%s", usage.c_str());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (argc < 2) {
        std::cerr << "\nWARNING: No path provided, skipping reference image comparison.\n\n";
        g_comparisonMode = ComparisonMode::SKIP;
        return RUN_ALL_TESTS();
    }
    const string cmd = argv[1];
    if (cmd == "help") {
        printUsage(argv[0]);
        return 0;
    }
    if (cmd == "compare" || cmd == "update") {
        if (argc != 3) {
            printUsage(argv[0]);
            return 1;
        }
        g_comparisonPath = argv[2];
    }
    if (cmd == "compare") {
        g_comparisonMode = ComparisonMode::COMPARE;
        return RUN_ALL_TESTS();
    }
    if (cmd == "update") {
        g_comparisonMode = ComparisonMode::UPDATE;
        return RUN_ALL_TESTS();
    }
    printUsage(argv[0]);
    return 1;
}

static LinearImage createColorFromAscii(const string& pattern) {
    uint32_t width = 0;
    uint32_t height = 0;
    string row;

    // Compute the required size.
    for (istringstream istream(pattern); istream >> row; ++height) {
        width = (uint32_t) row.size();
    }

    // Allocate the sequence of pixels.
    LinearImage result(width, height, 3);

    // Fill in the pixel data.
    istringstream istream(pattern);
    float* seq = result.getPixelRef();
    for (int i = 0; istream >> row;) {
        for (char c : row) {
            uint32_t val = c - (uint32_t)('0');
            seq[i++] = (val >> 0u) & 1u;
            seq[i++] = (val >> 1u) & 1u;
            seq[i++] = (val >> 2u) & 1u;
            auto col = (float3*) (seq + i - 3);
            *col = sRGBToLinear(*col);
        }
    }
    return result;
}

static void updateOrCompare(const LinearImage& limg, const utils::Path& fname) {
    if (g_comparisonMode == ComparisonMode::SKIP) {
        return;
    }

    // Regenerate the PNG file at the given path.
    // The encoder isn't yet robust for 1-channel data yet, we expand L to RGB.
    if (g_comparisonMode == ComparisonMode::UPDATE) {
        std::ofstream out(g_comparisonPath + fname, std::ios::binary | std::ios::trunc);
        auto format = ImageEncoder::Format::PNG_LINEAR;
        const size_t width = limg.getWidth(), height = limg.getHeight(), nchan = 3;
        const size_t bpp = nchan * sizeof(float), bpr = width * bpp, nbytes = bpr * height;
        std::unique_ptr<uint8_t[]> data(new uint8_t[nbytes]);
        if (nchan == 3) {
            memcpy(data.get(), limg.getPixelRef(), nbytes);
            Image im(std::move(data), width, height, bpr, bpp, nchan);
            ImageEncoder::encode(out, format, im, "", fname);
        } else if (nchan == 1) {
            auto limg2 = combineChannels({limg, limg, limg});
            memcpy(data.get(), limg2.getPixelRef(), nbytes);
            Image im(std::move(data), width, height, bpr, bpp, nchan);
            ImageEncoder::encode(out, format, im, "", fname);
        } else {
            ASSERT_PRECONDITION(false, "This test only supports 3-channel and 1-channel images.");
        }
        return;
    }

    // Load the PNG file at the given path.
    const string fullpath = g_comparisonPath + fname;
    std::ifstream in(fullpath, std::ios::binary);
    ASSERT_PRECONDITION(in, "Unable to open: %s", fullpath.c_str());
    Image img = ImageDecoder::decode(in, g_comparisonPath + fname,
            ImageDecoder::ColorSpace::LINEAR);
    const size_t width = img.getWidth(), height = img.getHeight(), nchan = img.getChannelsCount();
    ASSERT_PRECONDITION(nchan == 3, "This loaded file must be a 3-channel image.");

    // To keep things simple we always store rthe "expected" image in 3-channel format, so here we
    // expand the "actual" image from L to RGB.
    LinearImage actual;
    if (limg.getChannels() == 1) {
        actual = combineChannels({limg, limg, limg});
    } else {
        actual = limg;
    }

    // Perform a simple comparison of the two images with 0 threshold.
    LinearImage expected(width, height, 3);
    memcpy(expected.getPixelRef(), img.getData(), width * height * sizeof(float) * 3);
    ASSERT_PRECONDITION(compare(actual, expected, 0.0f) == 0, "Image mismatch.");
}
