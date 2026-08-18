#pragma once

#include <opencv2/opencv.hpp>
#include <string>

namespace kfc::graphics {

/// OpenCV-backed image wrapper. Everything downstream in kfc::graphics
/// builds on this instead of touching cv::Mat directly.
class Img {
public:
    Img();

    /// Loads the image at path (with alpha channel, if present) and
    /// optionally resizes it. If size is {0, 0} (the default), the image
    /// keeps its native pixel dimensions. keep_aspect shrinks so the longer
    /// side fits size while preserving aspect ratio, instead of stretching
    /// to size exactly. Throws std::runtime_error if path cannot be read.
    Img& read(const std::string& path, const std::pair<int, int>& size = {}, bool keep_aspect = false,
              int interpolation = cv::INTER_AREA);

    /// Draws this image onto other_img with its top-left corner at (x, y),
    /// alpha-blending if this image has a fourth (alpha) channel. Throws
    /// std::runtime_error if either image is unloaded or this image would
    /// not fit inside other_img at that position.
    void draw_on(Img& other_img, int x, int y);

    /// Draws txt with its baseline's left edge at (x, y).
    void put_text(const std::string& txt, int x, int y, double font_size,
                  const cv::Scalar& color = cv::Scalar(255, 255, 255, 255), int thickness = 1);

    /// Opens a window showing the image and blocks until any key is pressed.
    void show();

    /// The underlying OpenCV matrix, for callers that need raw pixel access.
    const cv::Mat& get_mat() const {
        return img_;
    }

    /// Deep copy of this image's pixels. Needed because Img's compiler-
    /// generated copy constructor only copies cv::Mat's reference-counted
    /// handle, so it would still share the source's pixel buffer.
    Img clone() const;

    /// A new Img of the given size, filled with color.
    static Img blank(int width, int height, const cv::Scalar& color = cv::Scalar(0, 0, 0, 255));

    /// Deep copy of the width x height region starting at (x, y). Throws
    /// std::runtime_error if that region doesn't fit inside this image.
    Img cropped(int x, int y, int width, int height) const;

    /// Deep copy of this image, resized to width x height.
    Img resized(int width, int height) const;

    /// CSS background-size:cover equivalent: scales up so both dimensions
    /// meet or exceed the target (aspect preserved), then center-crops to
    /// target_width x target_height.
    Img cover_scaled(int target_width, int target_height) const;

    /// Drops the alpha channel and marks the image fully opaque, so
    /// draw_on's per-pixel blend can be skipped for a layer that should
    /// always paste as a hard backdrop even if its source PNG carries
    /// near-255 alpha noise (e.g. anti-aliased edges).
    void force_opaque();

    /// Draws a translucent overlay across the bottom fraction (0..1) of a
    /// cell_size x cell_size cell at (cell_x, cell_y), pinned to the bottom
    /// edge -- an hourglass-sand stand-in with no art asset needed.
    void draw_hourglass_overlay(int cell_x, int cell_y, int cell_size, double fraction, const cv::Scalar& color);

    /// False until read() has successfully loaded an image.
    bool is_loaded() const {
        return !img_.empty();
    }

private:
    cv::Mat img_;
    // True only if img_ has a 4th channel with values below 255 somewhere;
    // computed once on load so draw_on can skip its per-pixel blend for
    // images that are nominally alpha but fully opaque.
    bool has_transparency_ = false;
    void update_transparency_flag();
};

}  // namespace kfc::graphics
